#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${TMPDIR:-/tmp}/w3d_crypto_wotlk_test
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$ROOT/net" \
    "$ROOT/net/tests/crypto_wotlk_test.cpp" "$ROOT/net/crypto.cpp" -o "$OUT"
"$OUT"
