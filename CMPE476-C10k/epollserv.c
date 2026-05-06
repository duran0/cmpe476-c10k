/* epollserv.c — YOUR implementation of Part B (epoll event loop).
 *
 * See Section 8 of the project definition.
 * Usage:  ./epollserv [port]   (default port 9091)
 */
#include "server_api.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 256
#define READ_CHUNK 1024
#define INTEST 1

typedef struct {
    int fd;
    conn_buf_t inbuf;
    char *outbuf;
    size_t out_cap;
    size_t out_len;
    size_t out_off;
    int close_after_flush;
} conn_t;

static int update_interest(int epfd, conn_t *c) {
    struct epoll_event ev;

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    if (c->out_off < c->out_len) {
        ev.events |= EPOLLOUT;
    }
    ev.data.ptr = c;
    return epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
}

static int ensure_out_capacity(conn_t *c, size_t extra) {
    size_t needed = c->out_len + extra;
    size_t new_cap;
    char *p;

    if (needed <= c->out_cap) {
        return 0;
    }

    new_cap = (c->out_cap == 0) ? 1024 : c->out_cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    p = (char *)realloc(c->outbuf, new_cap);
    if (p == NULL) {
        return -1;
    }
    c->outbuf = p;
    c->out_cap = new_cap;
    return 0;
}

static int queue_bytes(conn_t *c, const char *data, size_t n) {
    if (ensure_out_capacity(c, n) < 0) {
        return -1;
    }
    memcpy(c->outbuf + c->out_len, data, n);
    c->out_len += n;
    return 0;
}

static void latch_overlong_if_needed(conn_buf_t *buf) {
    if (buf->line_too_long) {
        return;
    }
    if (memchr(buf->data, '\n', buf->len) == NULL && buf->len > MAX_LINE_LEN) {
        buf->len = 0;
        buf->line_too_long = 1;
    }
}

/* Returns:
 *   0 => keep connection open
 *   1 => connection should be closed (close-after-flush path)
 *  -1 => unrecoverable write/epoll error
 */
static int flush_output(int epfd, conn_t *c) {
    while (c->out_off < c->out_len) {
        ssize_t w = send(c->fd,
                         c->outbuf + c->out_off,
                         c->out_len - c->out_off,
                         MSG_NOSIGNAL);
        if (w > 0) {
            c->out_off += (size_t)w;
            continue;
        }
        if (w < 0 && errno == EINTR) {
            continue;
        }
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (update_interest(epfd, c) < 0) {
                return -1;
            }
            return 0;
        }
        return -1;
    }

    c->out_len = 0;
    c->out_off = 0;

    if (c->close_after_flush) {
        return 1;
    }
    if (update_interest(epfd, c) < 0) {
        return -1;
    }
    return 0;
}

static void close_connection(int epfd, conn_t *c, server_state_t *st) {
    if (c == NULL) {
        return;
    }

    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);

    free(c->outbuf);
    free(c);

    if (st->active_connections > 0) {
        st->active_connections--;
    }
}

/* Returns:
 *   0 => keep connection open
 *   1 => close connection
 *  -1 => close connection (error)
 */
static int process_buffered_lines(int epfd, conn_t *c, server_state_t *st) {
    char line[CONN_BUF_CAPACITY + 1];
    char response[MAX_RESPONSE_LEN + 1];
    char wire[MAX_RESPONSE_LEN + 2];

    if (c->close_after_flush) {
        return flush_output(epfd, c);
    }

    for (;;) {
        int had_too_long = c->inbuf.line_too_long;
        int take = buffer_take_line(&c->inbuf, line, sizeof(line));
        request_t req;
        int rn;

        if (take < 0) {
            return -1;
        }
        if (take == 0) {
            break;
        }

        if (had_too_long || strlen(line) > MAX_LINE_LEN) {
            req.kind = CMD_TOO_LONG;
            req.arg[0] = '\0';
        } else if (parse_request(line, &req) < 0) {
            return -1;
        }

        rn = format_response(&req, st, response, sizeof(response));
        if (rn < 0) {
            return -1;
        }
        memcpy(wire, response, (size_t)rn);
        wire[rn] = '\n';

        if (queue_bytes(c, wire, (size_t)rn + 1) < 0) {
            return -1;
        }

        if (req.kind == CMD_QUIT || req.kind == CMD_TOO_LONG) {
            c->close_after_flush = 1;
            break;
        }
    }

    return flush_output(epfd, c);
}

static int handle_readable(int epfd, conn_t *c, server_state_t *st) {
    char chunk[READ_CHUNK];

    if (c->close_after_flush) {
        return flush_output(epfd, c);
    }

    for (;;) {
        ssize_t r = read(c->fd, chunk, sizeof(chunk));
        int pr;

        if (r > 0) {
            if (buffer_append(&c->inbuf, chunk, (size_t)r) < 0) {
                return -1;
            }
            latch_overlong_if_needed(&c->inbuf);
            pr = process_buffered_lines(epfd, c, st);
            if (pr != 0) {
                return pr;
            }
            if (c->close_after_flush) {
                return 0;
            }
            continue;
        }

        if (r == 0) {
            return 1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        return -1;
    }
    return 0;
}

static int handle_writable(int epfd, conn_t *c, server_state_t *st) {
    int fr = flush_output(epfd, c);
    if (fr != 0) {
        return fr;
    }
    return process_buffered_lines(epfd, c, st);
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 9091;
    int lfd;
    int epfd;
    int one = 1;
    struct sockaddr_in addr;
    struct epoll_event lev;
    server_state_t state = {0, 0};
    struct epoll_event events[MAX_EVENTS];

    (void)INTEST;

    signal(SIGPIPE, SIG_IGN);

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        perror("socket");
        return 1;
    }
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        close(lfd);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(lfd);
        return 1;
    }
    if (listen(lfd, SOMAXCONN) < 0) {
        perror("listen");
        close(lfd);
        return 1;
    }
    if (set_nonblocking(lfd) < 0) {
        perror("set_nonblocking(listen)");
        close(lfd);
        return 1;
    }

    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        close(lfd);
        return 1;
    }

    memset(&lev, 0, sizeof(lev));
    lev.events = EPOLLIN;
    lev.data.ptr = NULL;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &lev) < 0) {
        perror("epoll_ctl(ADD listen)");
        close(epfd);
        close(lfd);
        return 1;
    }

    fprintf(stderr, "epollserv listening on %d\n", port);

    for (;;) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        int i;

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        for (i = 0; i < n; i++) {
            if (events[i].data.ptr == NULL) {
                for (;;) {
                    int cfd = accept(lfd, NULL, NULL);
                    conn_t *c;
                    struct epoll_event cev;

                    if (cfd < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        perror("accept");
                        break;
                    }

                    state.total_connections++;

                    if (set_nonblocking(cfd) < 0) {
                        close(cfd);
                        continue;
                    }

                    c = (conn_t *)calloc(1, sizeof(*c));
                    if (c == NULL) {
                        close(cfd);
                        continue;
                    }
                    c->fd = cfd;
                    buffer_init(&c->inbuf);

                    memset(&cev, 0, sizeof(cev));
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.ptr = c;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev) < 0) {
                        close(cfd);
                        free(c);
                        continue;
                    }

                    state.active_connections++;
                }
                continue;
            }

            {
                conn_t *c = (conn_t *)events[i].data.ptr;
                int should_close = 0;
                int rc;

                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    should_close = 1;
                }

                if (!should_close && (events[i].events & EPOLLIN)) {
                    rc = handle_readable(epfd, c, &state);
                    if (rc != 0) {
                        should_close = 1;
                    }
                }

                if (!should_close && (events[i].events & EPOLLOUT)) {
                    rc = handle_writable(epfd, c, &state);
                    if (rc != 0) {
                        should_close = 1;
                    }
                }

                if (should_close) {
                    close_connection(epfd, c, &state);
                }
            }
        }
    }

    close(epfd);
    close(lfd);
    return 1;
}
