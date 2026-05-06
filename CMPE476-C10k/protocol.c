/* protocol.c — YOUR implementation goes here.
 * See server_api.h for the exact signatures and required behaviour.
 * See the project definition (CMPE476-C10K-Project.docx, Section 6) for
 * the semantics of each command.
 */
#include "server_api.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int write_string(char *out, size_t outlen, const char *src) {
    int n;

    if (outlen == 0 || src == NULL) {
        return -1;
    }

    n = snprintf(out, outlen, "%s", src);
    if (n < 0) {
        return -1;
    }
    if ((size_t)n >= outlen) {
        return (int)(outlen - 1);
    }
    return n;
}

int parse_request(const char *line, request_t *out) {
    if (line == NULL || out == NULL) {
        return -1;
    }

    out->arg[0] = '\0';

    if (strcmp(line, "PING") == 0) {
        out->kind = CMD_PING;
        return 0;
    }
    if (strcmp(line, "TIME") == 0) {
        out->kind = CMD_TIME;
        return 0;
    }
    if (strcmp(line, "STATS") == 0) {
        out->kind = CMD_STATS;
        return 0;
    }
    if (strcmp(line, "QUIT") == 0) {
        out->kind = CMD_QUIT;
        return 0;
    }
    if (strcmp(line, "ECHO") == 0) {
        out->kind = CMD_ECHO;
        return 0;
    }
    if (strncmp(line, "ECHO ", 5) == 0) {
        out->kind = CMD_ECHO;
        strncpy(out->arg, line + 5, MAX_LINE_LEN);
        out->arg[MAX_LINE_LEN] = '\0';
        return 0;
    }

    out->kind = CMD_UNKNOWN;
    return 0;
}

int format_response(const request_t *req, const server_state_t *st,
                    char *out, size_t outlen) {
    int n;

    if (req == NULL || st == NULL || out == NULL || outlen == 0) {
        return -1;
    }

    switch (req->kind) {
        case CMD_PING:
            return write_string(out, outlen, "PONG");
        case CMD_ECHO:
            return write_string(out, outlen, req->arg);
        case CMD_TIME: {
            uint64_t now = (uint64_t)time(NULL);
            n = snprintf(out, outlen, "%" PRIu64, now);
            break;
        }
        case CMD_STATS:
            n = snprintf(out, outlen, "%d", st->active_connections);
            break;
        case CMD_QUIT:
            return write_string(out, outlen, "BYE");
        case CMD_TOO_LONG:
            return write_string(out, outlen, "ERR line_too_long");
        case CMD_UNKNOWN:
        default:
            return write_string(out, outlen, "ERR unknown_command");
    }

    if (n < 0) {
        return -1;
    }
    if ((size_t)n >= outlen) {
        return (int)(outlen - 1);
    }
    return n;
}
