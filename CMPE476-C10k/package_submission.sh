#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: $0 <group_id> <surname1> [surname2]" >&2
  exit 2
fi

GROUP_ID="$1"
SUR1="$2"
SUR2="${3:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
PROJECT_BASENAME="$(basename "$PROJECT_DIR")"
PARENT_DIR="$(dirname "$PROJECT_DIR")"

required=(
  "$PROJECT_DIR/protocol.c"
  "$PROJECT_DIR/buffer.c"
  "$PROJECT_DIR/threadserv.c"
  "$PROJECT_DIR/epollserv.c"
  "$PROJECT_DIR/server_api.h"
  "$PROJECT_DIR/Makefile"
  "$PROJECT_DIR/tests.c"
  "$PROJECT_DIR/README.md"
  "$PROJECT_DIR/report.pdf"
)

for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "missing required file: $f" >&2
    exit 1
  fi
done

name="CMPE476-C10k-${GROUP_ID}-${SUR1}"
if [[ -n "$SUR2" ]]; then
  name="${name}_${SUR2}"
fi
archive="${name}.tar.gz"

cd "$PARENT_DIR"
tar -czf "$archive" "$PROJECT_BASENAME"
echo "created $archive"
