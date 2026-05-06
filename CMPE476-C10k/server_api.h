/* ============================================================
 * server_api.h  —  CMPE476 Project: C10K Concurrent TCP Server
 *
 * DO NOT MODIFY THIS FILE.
 *
 * This file defines the fixed interface that your implementation
 * MUST expose so that the instructor's CUnit test harness can link
 * against your object files and grade your submission automatically.
 *
 * You will implement the functions declared here in two places:
 *   - threadserv.c  (thread-per-connection server)
 *   - epollserv.c   (epoll-based event-loop server)
 * Both servers share the same protocol functions (parse_request,
 * format_response) and the same buffer helpers (buffer_*).
 * ============================================================ */

#ifndef SERVER_API_H
#define SERVER_API_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* ------------------------------------------------------------
 *  Protocol
 * ------------------------------------------------------------
 *  Each request is ONE line terminated by '\n' (optionally '\r\n').
 *  Each response is ONE line terminated by '\n'.
 *  Commands (case-sensitive):
 *     PING                  -> PONG
 *     ECHO <text>           -> <text>
 *     TIME                  -> <unix_seconds>
 *     STATS                 -> <active_connections>
 *     QUIT                  -> BYE  (server then closes the socket)
 *     anything else         -> ERR unknown_command
 *  Max accepted line length (excluding newline): 1024 bytes.
 *  Lines longer than that produce: ERR line_too_long
 *  and the connection is closed.
 * ------------------------------------------------------------ */

#define MAX_LINE_LEN       1024
#define MAX_RESPONSE_LEN   1088      /* a little extra headroom */
#define CONN_BUF_CAPACITY  2048

/* Command enumeration produced by parse_request(). */
typedef enum {
    CMD_PING = 1,
    CMD_ECHO = 2,
    CMD_TIME = 3,
    CMD_STATS = 4,
    CMD_QUIT = 5,
    CMD_UNKNOWN = 6,
    CMD_TOO_LONG = 7     /* set by the caller when the raw line exceeded MAX_LINE_LEN */
} cmd_kind_t;

/* Parsed request. */
typedef struct {
    cmd_kind_t kind;
    /* For CMD_ECHO: the payload (NUL-terminated, no leading space, no trailing newline).
       For every other kind: arg[0] == '\0'. */
    char       arg[MAX_LINE_LEN + 1];
} request_t;

/* Server-wide state that the instructor's tests will inspect. */
typedef struct {
    /* Number of clients currently connected (accept()ed but not yet closed). */
    int active_connections;
    /* Total clients accepted since server start. */
    uint64_t total_connections;
} server_state_t;

/* Per-connection line-buffer used by the epoll server. */
typedef struct {
    char   data[CONN_BUF_CAPACITY];
    size_t len;          /* bytes currently stored */
    int    line_too_long;/* latched: set to 1 once a line exceeded MAX_LINE_LEN */
} conn_buf_t;


/* ============================================================
 *  Functions YOU must implement  (in protocol.c and buffer.c)
 *  The CUnit test harness links directly against these.
 * ============================================================ */

/* --- Protocol (shared by both servers) --- */

/* Parse ONE line (no trailing '\n') into *out.
 * Returns  0 on success (including CMD_UNKNOWN).
 * Returns -1 if line == NULL or out == NULL.
 * Leading/trailing whitespace around the command word is NOT allowed:
 *   "PING"       -> CMD_PING
 *   " PING"      -> CMD_UNKNOWN
 *   "ECHO"       -> CMD_ECHO with arg[0]=='\0'
 *   "ECHO hi"    -> CMD_ECHO with arg == "hi"
 *   "ECHO  hi"   -> CMD_ECHO with arg == " hi"  (exactly one space
 *                   after ECHO is the separator; the rest is payload)
 *   ""           -> CMD_UNKNOWN
 * If the caller has already detected that the raw line exceeded
 * MAX_LINE_LEN, it is permitted (but not required) to skip this
 * function and construct a request_t with kind = CMD_TOO_LONG. */
int parse_request(const char *line, request_t *out);

/* Produce the response line (WITHOUT a trailing newline) for a given
 * request and server state. Writes at most outlen-1 bytes plus a NUL.
 * Returns the number of bytes written (excluding NUL), or -1 on error.
 * The caller is responsible for appending '\n' before sending.
 *
 * Response table:
 *   CMD_PING       -> "PONG"
 *   CMD_ECHO       -> the arg verbatim (may be empty -> empty string)
 *   CMD_TIME       -> decimal representation of (uint64_t)time(NULL)
 *   CMD_STATS      -> decimal representation of st->active_connections
 *   CMD_QUIT       -> "BYE"
 *   CMD_UNKNOWN    -> "ERR unknown_command"
 *   CMD_TOO_LONG   -> "ERR line_too_long"
 */
int format_response(const request_t *req,
                    const server_state_t *st,
                    char *out, size_t outlen);

/* --- Socket helper (shared) --- */

/* Set O_NONBLOCK on fd. Returns 0 on success, -1 on failure (and sets errno). */
int set_nonblocking(int fd);

/* --- Line-buffer helpers (used by the epoll server) --- */

/* Initialise an empty buffer. */
void buffer_init(conn_buf_t *b);

/* Append n bytes from data into b->data.
 * If appending would overflow CONN_BUF_CAPACITY AND no '\n' is present
 * in the combined content, the buffer is reset and line_too_long is
 * latched to 1. Returns 0 on success, -1 on error (b == NULL). */
int buffer_append(conn_buf_t *b, const char *data, size_t n);

/* If b contains at least one complete line (terminated by '\n'):
 *   - copy the line WITHOUT the trailing '\n' (and without a trailing
 *     '\r' if present) into out, NUL-terminated, truncating to outmax-1
 *     bytes if needed;
 *   - remove that line (including the '\n') from the buffer;
 *   - return 1.
 * If no complete line is present, return 0 and leave the buffer alone.
 * If the line_too_long flag is latched, this function must return 1
 * ONCE with out[0] == '\0' and then clear the flag, so the caller
 * knows to emit "ERR line_too_long" and close the connection.
 * Returns -1 on error (NULL args). */
int buffer_take_line(conn_buf_t *b, char *out, size_t outmax);


#endif /* SERVER_API_H */
