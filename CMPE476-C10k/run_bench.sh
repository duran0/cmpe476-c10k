#!/usr/bin/env bash
set -euo pipefail

# Reproducible benchmark runner for Part C.
# Runs both servers at 1000/5000/10000 concurrent clients
# and stores throughput + RSS in benchmark_results.csv.

HOST="${HOST:-127.0.0.1}"
REQ_PER_CLIENT="${REQ_PER_CLIENT:-100}"
CONCURRENCY_LIST=(${CONCURRENCY_LIST:-1000 5000 10000})
RESULTS_FILE="${RESULTS_FILE:-benchmark_results.csv}"
THREAD_PORT="${THREAD_PORT:-9090}"
EPOLL_PORT="${EPOLL_PORT:-9091}"

need_bin() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing required command: $1" >&2
    exit 1
  }
}

need_bin awk
need_bin sed
need_bin grep
need_bin sleep
need_bin kill

if [[ ! -x ./threadserv || ! -x ./epollserv || ! -x ./client_flood ]]; then
  echo "build binaries first: make && make client_flood" >&2
  exit 1
fi

read_rss_kb() {
  local pid="$1"
  if [[ -r "/proc/$pid/status" ]]; then
    awk '/VmRSS:/ {print $2; exit}' "/proc/$pid/status"
  else
    echo 0
  fi
}

run_one() {
  local server_bin="$1"
  local port="$2"
  local clients="$3"

  local log_file=".bench_${server_bin}_${clients}.log"
  local out_file=".bench_client_${server_bin}_${clients}.out"

  "./$server_bin" "$port" >"$log_file" 2>&1 &
  local spid=$!
  sleep 1

  local rss_before
  rss_before="$(read_rss_kb "$spid")"

  ./client_flood "$HOST" "$port" "$clients" "$REQ_PER_CLIENT" >"$out_file" 2>&1 || true

  local rss_after
  rss_after="$(read_rss_kb "$spid")"

  kill "$spid" >/dev/null 2>&1 || true
  wait "$spid" >/dev/null 2>&1 || true

  local requests elapsed rps established completed
  requests="$(awk -F'[ =]+' '/requests_completed=/{print $2}' "$out_file" | tail -n1)"
  elapsed="$(awk -F'[ =]+' '/requests_completed=/{print $4}' "$out_file" | tail -n1)"
  rps="$(awk -F'[ =]+' '/requests_completed=/{print $6}' "$out_file" | tail -n1)"
  established="$(awk -F'[ =]+' '/clients_target=/{print $4}' "$out_file" | tail -n1)"
  completed="$(awk -F'[ =]+' '/clients_target=/{print $6}' "$out_file" | tail -n1)"

  requests="${requests:-0}"
  elapsed="${elapsed:-0}"
  rps="${rps:-0}"
  established="${established:-0}"
  completed="${completed:-0}"

  echo "${server_bin},${clients},${REQ_PER_CLIENT},${requests},${elapsed},${rps},${rss_before},${rss_after},${established},${completed}" >>"$RESULTS_FILE"
}

echo "server,clients,requests_each,requests_completed,elapsed_seconds,rps,rss_kb_before,rss_kb_after,established,completed" >"$RESULTS_FILE"

for c in "${CONCURRENCY_LIST[@]}"; do
  run_one "threadserv" "$THREAD_PORT" "$c"
done

for c in "${CONCURRENCY_LIST[@]}"; do
  run_one "epollserv" "$EPOLL_PORT" "$c"
done

if [[ ! -f report.md ]]; then
cat > report.md <<'EOF'
# CMPE476 C10K Benchmark Report

## 1. Test Setup
- Machine:
- CPU / RAM:
- OS and kernel:
- `ulimit -n`:
- Any `sysctl` tuning:

## 2. Methodology
- Built with `make` from the submitted source.
- Load generator: `client_flood`.
- For each server (`threadserv`, `epollserv`) and each concurrency level (1000, 5000, 10000):
  - launch server
  - run `client_flood <host> <port> <clients> <requests_each>`
  - record requests/sec and RSS from `/proc/<pid>/status`
  - stop server

## 3. Results Table
Import rows from `benchmark_results.csv`:

| Server | Clients | Requests Each | Completed Requests | Elapsed (s) | RPS | RSS Before (kB) | RSS After (kB) |
|---|---:|---:|---:|---:|---:|---:|---:|
| threadserv | 1000 | | | | | | |
| threadserv | 5000 | | | | | | |
| threadserv | 10000 | | | | | | |
| epollserv | 1000 | | | | | | |
| epollserv | 5000 | | | | | | |
| epollserv | 10000 | | | | | | |

## 4. Chart
Include at least one chart:
- Throughput (RPS) vs concurrent clients, or
- RSS vs concurrent clients.

## 5. Analysis
- Compare scaling behavior.
- Explain where thread-per-connection starts degrading.
- Explain why epoll/ET + nonblocking improves memory and throughput.
- Mention any anomalies and likely causes.

EOF
fi

echo "benchmark complete -> ${RESULTS_FILE}"
echo "report template path -> report.md"
