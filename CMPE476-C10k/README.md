# CMPE476 C10K Project Submission

## Group Members
- Duran Kaan Altın, 2020400108, implemented `protocol.c`, `buffer.c`, and benchmarking automation.
- Üveys Aydemir, 2020400069, implemented  `threadserv.c`, `epollserv.c`, and report.

## Build And Test (Linux)
```bash
make clean
make test
make
make client_flood
```

## Run
```bash
./threadserv 9090
./epollserv 9091
```

Quick manual protocol check:
```bash
nc 127.0.0.1 9091
PING
ECHO hello
TIME
STATS
BADCOMMAND
QUIT
```

