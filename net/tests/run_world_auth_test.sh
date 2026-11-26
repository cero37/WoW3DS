#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${TMPDIR:-/tmp}/w3d_world_auth_test
trap 'rm -f "$OUT"' EXIT
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -pthread -I"$ROOT/net" \
    "$ROOT/net/tests/world_auth_test.cpp" "$ROOT/net/client.cpp" "$ROOT/net/crypto.cpp" \
    "$ROOT/net/socket.cpp" -o "$OUT"
"$OUT"
