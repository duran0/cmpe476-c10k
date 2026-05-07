#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <surname1> [surname2]" >&2
  exit 2
fi

SUR1="$1"
SUR2="${2:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
PROJECT_BASENAME="$(basename "$PROJECT_DIR")"
PARENT_DIR="$(dirname "$PROJECT_DIR")"

required_files=(
  "protocol.c"
  "buffer.c"
  "threadserv.c"
  "epollserv.c"
  "server_api.h"
  "Makefile"
  "tests.c"
  "README.md"
  "report.pdf"
)

for f in "${required_files[@]}"; do
  if [[ ! -f "$PROJECT_DIR/$f" ]]; then
    echo "missing required file: $PROJECT_DIR/$f" >&2
    exit 1
  fi
done

name="CMPE476-C10k-${SUR1}"
if [[ -n "$SUR2" ]]; then
  name="${name}_${SUR2}"
fi
archive="${name}.tar.gz"

# Avoid packaging compiled artifacts by staging only required files.
if command -v make >/dev/null 2>&1; then
  (cd "$PROJECT_DIR" && make clean >/dev/null 2>&1 || true)
fi

stage_root="$(mktemp -d)"
trap 'rm -rf "$stage_root"' EXIT
stage_dir="$stage_root"

for f in "${required_files[@]}"; do
  cp "$PROJECT_DIR/$f" "$stage_dir/$f"
done

(cd "$stage_root" && tar -czf "$PARENT_DIR/$archive" "${required_files[@]}")
echo "created $PARENT_DIR/$archive"
