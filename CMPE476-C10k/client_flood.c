/* client_flood.c — reference load generator for Part C.
 *
 * Opens N concurrent TCP connections to host:port, sends M requests on each,
 * measures wall-clock time, and prints aggregate statistics. All clients
 * are driven from a single thread using epoll + non-blocking sockets so
 * we can actually reach 10 000 concurrent connections without spawning
 * 10 000 threads (which would defeat the purpose of the measurement).
 *
 * Build:   make client_flood     (after adding it to the Makefile)
 *    or:   gcc -O2 -o client_flood client_flood.c
 *
 * Usage:   ./client_flood <host> <port> <N_clients> <M_requests_each>
 *
 * You are free to modify this tool or replace it with one of your own.
 * The only requirement is that your report's methodology be reproducible.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#define REQ_LINE "PING\n"
#define REQ_LEN  5
#define READ_BUF 256

typedef struct {
    int fd;
    int sent;           /* requests sent so far */
    int recv;           /* complete responses received so far */
    int total;          /* target M */
    char rbuf[READ_BUF];
    int rlen;
} client_t;

static int set_nb(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <host> <port> <N_clients> <M_requests>\n", argv[0]);
        return 2;
    }
    const char *host = argv[1];
    int port = atoi(argv[2]);
    int N = atoi(argv[3]);
    int M = atoi(argv[4]);

    signal(SIGPIPE, SIG_IGN);

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        /* allow hostname */
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
        struct addrinfo *res;
        if (getaddrinfo(host, NULL, &hints, &res) != 0) {
            fprintf(stderr, "bad host: %s\n", host); return 2;
        }
        sa.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    client_t *C = calloc(N, sizeof *C);
    int ep = epoll_create1(0);
    double t0 = now_sec();
    int established = 0;

    for (int i = 0; i < N; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { perror("socket"); N = i; break; }
        set_nb(fd);
        int r = connect(fd, (struct sockaddr*)&sa, sizeof sa);
        if (r < 0 && errno != EINPROGRESS) { perror("connect"); close(fd); continue; }
        C[i].fd = fd;
        C[i].total = M;
        struct epoll_event ev = { .events = EPOLLOUT | EPOLLIN | EPOLLET, .data.u32 = i };
        epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);
    }

    int done = 0;
    long long total_rx = 0;

    while (done < N) {
        struct epoll_event evs[256];
        int n = epoll_wait(ep, evs, 256, 5000);
        if (n <= 0) {
            fprintf(stderr, "epoll_wait timed out at %d/%d done\n", done, N);
            break;
        }
        for (int k = 0; k < n; k++) {
            int i = evs[k].data.u32;
            client_t *c = &C[i];
            if (c->fd < 0) continue;

            /* writable: send as many requests as fit */
            if (evs[k].events & EPOLLOUT) {
                if (!c->sent) established++;
                while (c->sent < c->total) {
                    ssize_t w = send(c->fd, REQ_LINE, REQ_LEN, MSG_NOSIGNAL);
                    if (w < 0) { if (errno == EAGAIN) break;
                                 close(c->fd); c->fd = -1; done++; break; }
                    c->sent++;
                }
            }
            /* readable: drain */
            if (c->fd >= 0 && (evs[k].events & EPOLLIN)) {
                for (;;) {
                    ssize_t r = recv(c->fd, c->rbuf + c->rlen, READ_BUF - c->rlen, 0);
                    if (r < 0) { if (errno == EAGAIN) break;
                                 close(c->fd); c->fd = -1; done++; break; }
                    if (r == 0) { close(c->fd); c->fd = -1; done++; break; }
                    c->rlen += r;
                    total_rx += r;
                    /* count newlines */
                    int j = 0, consumed = 0;
                    for (j = 0; j < c->rlen; j++) {
                        if (c->rbuf[j] == '\n') {
                            c->recv++;
                            consumed = j + 1;
                        }
                    }
                    if (consumed) {
                        memmove(c->rbuf, c->rbuf + consumed, c->rlen - consumed);
                        c->rlen -= consumed;
                    }
                    if (c->recv >= c->total) {
                        close(c->fd); c->fd = -1; done++; break;
                    }
                }
            }
        }
    }

    double t1 = now_sec();
    double elapsed = t1 - t0;
    long long total_req = 0;
    int completed = 0;
    for (int i = 0; i < N; i++) {
        total_req += C[i].recv;
        if (C[i].recv >= C[i].total) completed++;
    }
    printf("clients_target=%d established=%d completed=%d\n", N, established, completed);
    printf("requests_completed=%lld elapsed=%.3f s  rps=%.0f\n",
           total_req, elapsed, total_req / elapsed);
    printf("bytes_rx=%lld\n", total_rx);

    free(C);
    close(ep);
    return 0;
}
