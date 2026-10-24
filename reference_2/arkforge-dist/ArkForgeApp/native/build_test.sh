#!/usr/bin/env bash
# Build the verified public C API harness, not the legacy C++ experiment.
set -euo pipefail
NATIVE_DIR=$(cd "$(dirname "$0")" && pwd)
SRC_ROOT=${SRC_ROOT:-/opt/arkforge/src}
BUILD_DIR=${BUILD_DIR:-/opt/arkforge/build-host}
TEST_OUT=${TEST_OUT:-$BUILD_DIR/arkforge}
HEADER_DIR="$SRC_ROOT/ets_frontend/ets2panda/public"
mkdir -p "$TEST_OUT"
cc -std=c11 -Wall -Wextra "$NATIVE_DIR/test_es2panda_public.c" \
    -I"$HEADER_DIR" -I"$BUILD_DIR/tools/es2panda" -L"$BUILD_DIR/lib" -les2panda-public \
    -Wl,-rpath,"$BUILD_DIR/lib" -Wl,-rpath-link,"$BUILD_DIR/lib" \
    -o "$TEST_OUT/es2panda_public_test"
printf 'Built %s\n' "$TEST_OUT/es2panda_public_test"
