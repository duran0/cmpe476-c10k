# Submission Checklist

## 1. Required Files
- `protocol.c`
- `buffer.c`
- `threadserv.c`
- `epollserv.c`
- `server_api.h` (unmodified)
- `Makefile` (unmodified unless instructor allowed)
- `tests.c` (unmodified)
- `report.pdf`
- `README.md` with each member's name, ID, and contribution sentence

## 2. Pre-Submission Validation (Linux)
```bash
make clean
make test
make
make client_flood
```

Expected:
- `make test` returns zero failures.
- `make` builds `threadserv` and `epollserv` with no warnings.

## 3. Functional Spot Checks
Run:
```bash
./epollserv 9091
```
In another terminal:
```bash
nc 127.0.0.1 9091
PING
ECHO hello
ECHO
TIME
STATS
BADCOMMAND
QUIT
```

Expected:
- `PONG`
- `hello`
- empty line for `ECHO`
- unix timestamp for `TIME`
- active connection count for `STATS`
- `ERR unknown_command`
- `BYE` then socket closes

## 4. Benchmark + Report
```bash
./run_bench.sh
```

Then:
- fill machine/setup details in `report.md`
- add chart
- export to `report.pdf`

## 5. Packaging
Archive format required by the spec:
`CMPE476-C10k-<group_id>-<surname1>[_<surname2>].tar.gz`

Example packaging command:
```bash
tar -czf CMPE476-C10k-07-Yilmaz_Cetin.tar.gz CMPE476-C10k/
```

