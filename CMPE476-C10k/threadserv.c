/* threadserv.c — YOUR implementation of Part A (thread-per-connection).
 *
 * See Section 7 of the project definition.
 * Usage:  ./threadserv [port]   (default port 9090)
 */
#include "server_api.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    int fd;
} worker_arg_t;

static server_state_t g_state = {0, 0};
static pthread_mutex_t g_state_mu = PTHREAD_MUTEX_INITIALIZER;

static server_state_t state_snapshot(void) {
    server_state_t snap;
    pthread_mutex_lock(&g_state_mu);
    snap = g_state;
    pthread_mutex_unlock(&g_state_mu);
    return snap;
}

static void state_inc_active(void) {
    pthread_mutex_lock(&g_state_mu);
    g_state.active_connections++;
    pthread_mutex_unlock(&g_state_mu);
}

static void state_dec_active(void) {
    pthread_mutex_lock(&g_state_mu);
    if (g_state.active_connections > 0) {
        g_state.active_connections--;
    }
    pthread_mutex_unlock(&g_state_mu);
}

static void state_inc_total(void) {
    pthread_mutex_lock(&g_state_mu);
    g_state.total_connections++;
    pthread_mutex_unlock(&g_state_mu);
}

static int write_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, MSG_NOSIGNAL);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return -1;
    }
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

static void *worker_main(void *arg) {
    worker_arg_t *warg = (worker_arg_t *)arg;
    int cfd = warg->fd;
    conn_buf_t buf;
    char read_buf[1024];
    char line[CONN_BUF_CAPACITY + 1];
    char response[MAX_RESPONSE_LEN + 1];
    char wire[MAX_RESPONSE_LEN + 2];
    int done = 0;

    free(warg);

    buffer_init(&buf);
    state_inc_active();

    while (!done) {
        ssize_t r = read(cfd, read_buf, sizeof(read_buf));
        if (r > 0) {
            if (buffer_append(&buf, read_buf, (size_t)r) < 0) {
                break;
            }
            latch_overlong_if_needed(&buf);

            while (!done) {
                int had_too_long = buf.line_too_long;
                int take = buffer_take_line(&buf, line, sizeof(line));
                request_t req;
                server_state_t snap;
                int rn;

                if (take < 0) {
                    done = 1;
                    break;
                }
                if (take == 0) {
                    break;
                }

                if (had_too_long || strlen(line) > MAX_LINE_LEN) {
                    req.kind = CMD_TOO_LONG;
                    req.arg[0] = '\0';
                } else if (parse_request(line, &req) < 0) {
                    done = 1;
                    break;
                }

                snap = state_snapshot();
                rn = format_response(&req, &snap, response, sizeof(response));
                if (rn < 0) {
                    done = 1;
                    break;
                }

                memcpy(wire, response, (size_t)rn);
                wire[rn] = '\n';
                if (write_all(cfd, wire, (size_t)rn + 1) < 0) {
                    done = 1;
                    break;
                }

                if (req.kind == CMD_QUIT || req.kind == CMD_TOO_LONG) {
                    done = 1;
                    break;
                }
            }
            continue;
        }

        if (r == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    close(cfd);
    state_dec_active();
    return NULL;
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 9090;
    int lfd;
    int optval = 1;
    struct sockaddr_in addr;

    signal(SIGPIPE, SIG_IGN);

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        perror("socket");
        return 1;
    }

    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
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

    fprintf(stderr, "threadserv listening on %d\n", port);

    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        pthread_t tid;
        worker_arg_t *arg;

        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        state_inc_total();

        arg = (worker_arg_t *)malloc(sizeof(*arg));
        if (arg == NULL) {
            close(cfd);
            continue;
        }
        arg->fd = cfd;

        if (pthread_create(&tid, NULL, worker_main, arg) != 0) {
            close(cfd);
            free(arg);
            continue;
        }
        pthread_detach(tid);
    }

    close(lfd);
    return 0;
}
