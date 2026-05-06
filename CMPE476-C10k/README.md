# CMPE476 C10K Project Submission

## Group Members
- Name Surname, Student ID, contribution summary.
- Name Surname, Student ID, contribution summary.

Update this section before submission.

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

## Benchmark
Use:
```bash
./run_bench.sh
```

This generates:
- `benchmark_results.csv`
- `report.md` (fill in machine-specific details and chart screenshot path)

Then export `report.md` to `report.pdf`.
Example:
```bash
pandoc report.md -o report.pdf
```

## Package
From the parent directory of `CMPE476-C10k/`:
```bash
./CMPE476-C10k/package_submission.sh <group_id> <surname1> [surname2]
```
