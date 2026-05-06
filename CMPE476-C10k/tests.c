/* ============================================================
 * tests.c  —  CUnit unit tests for the CMPE476 C10K project
 *
 * This file is the instructor's test harness. It is linked
 * directly against the student's  protocol.c  and  buffer.c.
 * Students should NOT modify this file; we will replace it with
 * the authoritative version at grading time.
 *
 * Build & run:   make test && ./test_runner
 * Exit code:     0 if all tests pass, non-zero otherwise
 * ============================================================ */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#include "server_api.h"

/* ------------------------------------------------------------
 *  parse_request tests
 * ------------------------------------------------------------ */

static void test_parse_null_args(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request(NULL, &r), -1);
    CU_ASSERT_EQUAL(parse_request("PING", NULL), -1);
}

static void test_parse_ping(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("PING", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_PING);
    CU_ASSERT_STRING_EQUAL(r.arg, "");
}

static void test_parse_time(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("TIME", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_TIME);
}

static void test_parse_stats(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("STATS", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_STATS);
}

static void test_parse_quit(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("QUIT", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_QUIT);
}

static void test_parse_echo_empty(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("ECHO", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_ECHO);
    CU_ASSERT_STRING_EQUAL(r.arg, "");
}

static void test_parse_echo_payload(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("ECHO hello world", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_ECHO);
    CU_ASSERT_STRING_EQUAL(r.arg, "hello world");
}

static void test_parse_echo_preserves_extra_spaces(void) {
    /* exactly one space separates ECHO from the payload;
       any further whitespace is part of the payload */
    request_t r;
    CU_ASSERT_EQUAL(parse_request("ECHO  hi", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_ECHO);
    CU_ASSERT_STRING_EQUAL(r.arg, " hi");
}

static void test_parse_empty_line(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_UNKNOWN);
}

static void test_parse_unknown(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("HELLO", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_UNKNOWN);
}

static void test_parse_leading_space_is_unknown(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request(" PING", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_UNKNOWN);
}

static void test_parse_case_sensitive(void) {
    request_t r;
    CU_ASSERT_EQUAL(parse_request("ping", &r), 0);
    CU_ASSERT_EQUAL(r.kind, CMD_UNKNOWN);
}

/* ------------------------------------------------------------
 *  format_response tests
 * ------------------------------------------------------------ */

static void test_format_null_args(void) {
    char buf[128];
    request_t r = { .kind = CMD_PING, .arg = "" };
    server_state_t st = { .active_connections = 0, .total_connections = 0 };
    CU_ASSERT_EQUAL(format_response(NULL, &st, buf, sizeof buf), -1);
    CU_ASSERT_EQUAL(format_response(&r, &st, NULL, 0), -1);
}

static void test_format_ping(void) {
    char buf[128];
    request_t r = { .kind = CMD_PING, .arg = "" };
    server_state_t st = { 0, 0 };
    int n = format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT(n > 0);
    CU_ASSERT_STRING_EQUAL(buf, "PONG");
}

static void test_format_echo(void) {
    char buf[128];
    request_t r = { .kind = CMD_ECHO };
    strcpy(r.arg, "hello");
    server_state_t st = { 0, 0 };
    format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT_STRING_EQUAL(buf, "hello");
}

static void test_format_echo_empty(void) {
    char buf[128];
    request_t r = { .kind = CMD_ECHO };
    r.arg[0] = '\0';
    server_state_t st = { 0, 0 };
    format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT_STRING_EQUAL(buf, "");
}

static void test_format_time(void) {
    char buf[128];
    request_t r = { .kind = CMD_TIME, .arg = "" };
    server_state_t st = { 0, 0 };
    time_t before = time(NULL);
    format_response(&r, &st, buf, sizeof buf);
    time_t after = time(NULL);
    uint64_t returned = strtoull(buf, NULL, 10);
    CU_ASSERT(returned >= (uint64_t)before && returned <= (uint64_t)after);
}

static void test_format_stats(void) {
    char buf[128];
    request_t r = { .kind = CMD_STATS, .arg = "" };
    server_state_t st = { .active_connections = 42, .total_connections = 999 };
    format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT_STRING_EQUAL(buf, "42");
}

static void test_format_quit(void) {
    char buf[128];
    request_t r = { .kind = CMD_QUIT, .arg = "" };
    server_state_t st = { 0, 0 };
    format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT_STRING_EQUAL(buf, "BYE");
}

static void test_format_unknown(void) {
    char buf[128];
    request_t r = { .kind = CMD_UNKNOWN, .arg = "" };
    server_state_t st = { 0, 0 };
    format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT_STRING_EQUAL(buf, "ERR unknown_command");
}

static void test_format_too_long(void) {
    char buf[128];
    request_t r = { .kind = CMD_TOO_LONG, .arg = "" };
    server_state_t st = { 0, 0 };
    format_response(&r, &st, buf, sizeof buf);
    CU_ASSERT_STRING_EQUAL(buf, "ERR line_too_long");
}

/* ------------------------------------------------------------
 *  set_nonblocking
 * ------------------------------------------------------------ */

static void test_set_nonblocking_sets_flag(void) {
    int fds[2];
    CU_ASSERT_EQUAL(pipe(fds), 0);
    int before = fcntl(fds[0], F_GETFL, 0);
    CU_ASSERT(!(before & O_NONBLOCK));
    CU_ASSERT_EQUAL(set_nonblocking(fds[0]), 0);
    int after = fcntl(fds[0], F_GETFL, 0);
    CU_ASSERT(after & O_NONBLOCK);
    close(fds[0]);
    close(fds[1]);
}

/* ------------------------------------------------------------
 *  buffer tests
 * ------------------------------------------------------------ */

static void test_buffer_init(void) {
    conn_buf_t b;
    buffer_init(&b);
    CU_ASSERT_EQUAL(b.len, 0);
    CU_ASSERT_EQUAL(b.line_too_long, 0);
}

static void test_buffer_single_line(void) {
    conn_buf_t b;
    buffer_init(&b);
    CU_ASSERT_EQUAL(buffer_append(&b, "PING\n", 5), 0);
    char line[128];
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "PING");
    CU_ASSERT_EQUAL(b.len, 0);
    /* no more lines now */
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 0);
}

static void test_buffer_crlf_stripped(void) {
    conn_buf_t b;
    buffer_init(&b);
    buffer_append(&b, "PING\r\n", 6);
    char line[128];
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "PING");
}

static void test_buffer_partial_line(void) {
    conn_buf_t b;
    buffer_init(&b);
    buffer_append(&b, "PI", 2);
    char line[128];
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 0);
    buffer_append(&b, "NG\n", 3);
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "PING");
}

static void test_buffer_multiple_lines(void) {
    conn_buf_t b;
    buffer_init(&b);
    buffer_append(&b, "PING\nECHO hi\nQUIT\n", 18);
    char line[128];
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "PING");
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "ECHO hi");
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "QUIT");
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 0);
}

static void test_buffer_overflow_latches_too_long(void) {
    conn_buf_t b;
    buffer_init(&b);
    char junk[CONN_BUF_CAPACITY + 8];
    memset(junk, 'A', sizeof junk);
    buffer_append(&b, junk, sizeof junk);
    char line[128];
    /* take_line must surface the error exactly once with an empty line */
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 1);
    CU_ASSERT_STRING_EQUAL(line, "");
    /* flag cleared after being surfaced */
    CU_ASSERT_EQUAL(buffer_take_line(&b, line, sizeof line), 0);
}

static void test_buffer_null_guards(void) {
    char line[32];
    conn_buf_t b;
    buffer_init(&b);
    CU_ASSERT_EQUAL(buffer_append(NULL, "x", 1), -1);
    CU_ASSERT_EQUAL(buffer_take_line(NULL, line, sizeof line), -1);
    CU_ASSERT_EQUAL(buffer_take_line(&b, NULL, sizeof line), -1);
}

/* ------------------------------------------------------------
 *  test registration / main
 * ------------------------------------------------------------ */

int main(void) {
    if (CU_initialize_registry() != CUE_SUCCESS) return CU_get_error();

    CU_pSuite s1 = CU_add_suite("parse_request", NULL, NULL);
    CU_ADD_TEST(s1, test_parse_null_args);
    CU_ADD_TEST(s1, test_parse_ping);
    CU_ADD_TEST(s1, test_parse_time);
    CU_ADD_TEST(s1, test_parse_stats);
    CU_ADD_TEST(s1, test_parse_quit);
    CU_ADD_TEST(s1, test_parse_echo_empty);
    CU_ADD_TEST(s1, test_parse_echo_payload);
    CU_ADD_TEST(s1, test_parse_echo_preserves_extra_spaces);
    CU_ADD_TEST(s1, test_parse_empty_line);
    CU_ADD_TEST(s1, test_parse_unknown);
    CU_ADD_TEST(s1, test_parse_leading_space_is_unknown);
    CU_ADD_TEST(s1, test_parse_case_sensitive);

    CU_pSuite s2 = CU_add_suite("format_response", NULL, NULL);
    CU_ADD_TEST(s2, test_format_null_args);
    CU_ADD_TEST(s2, test_format_ping);
    CU_ADD_TEST(s2, test_format_echo);
    CU_ADD_TEST(s2, test_format_echo_empty);
    CU_ADD_TEST(s2, test_format_time);
    CU_ADD_TEST(s2, test_format_stats);
    CU_ADD_TEST(s2, test_format_quit);
    CU_ADD_TEST(s2, test_format_unknown);
    CU_ADD_TEST(s2, test_format_too_long);

    CU_pSuite s3 = CU_add_suite("set_nonblocking", NULL, NULL);
    CU_ADD_TEST(s3, test_set_nonblocking_sets_flag);

    CU_pSuite s4 = CU_add_suite("buffer", NULL, NULL);
    CU_ADD_TEST(s4, test_buffer_init);
    CU_ADD_TEST(s4, test_buffer_single_line);
    CU_ADD_TEST(s4, test_buffer_crlf_stripped);
    CU_ADD_TEST(s4, test_buffer_partial_line);
    CU_ADD_TEST(s4, test_buffer_multiple_lines);
    CU_ADD_TEST(s4, test_buffer_overflow_latches_too_long);
    CU_ADD_TEST(s4, test_buffer_null_guards);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    unsigned int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return (failures == 0) ? 0 : 1;
}
