/* buffer.c — YOUR implementation goes here.
 * See server_api.h for the exact signatures and required behaviour.
 */
#include "server_api.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

void buffer_init(conn_buf_t *b) {
    if (b == NULL) {
        return;
    }
    b->len = 0;
    b->line_too_long = 0;
}

int buffer_append(conn_buf_t *b, const char *data, size_t n) {
    int has_newline;

    if (b == NULL || (data == NULL && n > 0)) {
        return -1;
    }
    if (n == 0) {
        return 0;
    }

    if (b->len > CONN_BUF_CAPACITY) {
        b->len = 0;
    }

    if (n > (CONN_BUF_CAPACITY - b->len)) {
        has_newline = (memchr(b->data, '\n', b->len) != NULL) ||
                      (memchr(data, '\n', n) != NULL);
        if (!has_newline) {
            b->len = 0;
            b->line_too_long = 1;
            return 0;
        }
        n = CONN_BUF_CAPACITY - b->len;
    }

    if (n > 0) {
        memcpy(b->data + b->len, data, n);
        b->len += n;
    }
    return 0;
}

int buffer_take_line(conn_buf_t *b, char *out, size_t outmax) {
    char *nl;
    size_t raw_len;
    size_t line_len;
    size_t copy_len;
    size_t consumed;

    if (b == NULL || out == NULL || outmax == 0) {
        return -1;
    }

    if (b->line_too_long) {
        out[0] = '\0';
        b->line_too_long = 0;
        return 1;
    }

    nl = memchr(b->data, '\n', b->len);
    if (nl == NULL) {
        return 0;
    }

    raw_len = (size_t)(nl - b->data);
    line_len = raw_len;
    if (line_len > 0 && b->data[line_len - 1] == '\r') {
        line_len--;
    }

    copy_len = line_len;
    if (copy_len >= outmax) {
        copy_len = outmax - 1;
    }
    if (copy_len > 0) {
        memcpy(out, b->data, copy_len);
    }
    out[copy_len] = '\0';

    consumed = raw_len + 1;
    if (consumed < b->len) {
        memmove(b->data, b->data + consumed, b->len - consumed);
    }
    b->len -= consumed;
    return 1;
}
