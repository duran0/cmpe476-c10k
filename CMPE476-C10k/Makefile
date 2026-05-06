# =============================================================
#  CMPE476 — C10K Project Makefile
#  Build:      make              (builds threadserv and epollserv)
#  Unit test:  make test         (compiles and runs CUnit tests)
#  Clean:      make clean
# =============================================================

CC       = gcc
CFLAGS   = -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -O2 -g
LDFLAGS  =
LDLIBS   = -lpthread
CUNIT_LIBS = -lcunit

SHARED_OBJS = protocol.o buffer.o

THREAD_OBJS = threadserv.o $(SHARED_OBJS)
EPOLL_OBJS  = epollserv.o  $(SHARED_OBJS)
TEST_OBJS   = tests.o      $(SHARED_OBJS)

BINS = threadserv epollserv

.PHONY: all test clean

all: $(BINS)

threadserv: $(THREAD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

epollserv: $(EPOLL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

client_flood: client_flood.c
	$(CC) $(CFLAGS) -o $@ client_flood.c

test_runner: $(TEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(CUNIT_LIBS)

test: test_runner
	./test_runner

%.o: %.c server_api.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BINS) test_runner client_flood *.o
