#!/usr/bin/env bash
set -euo pipefail
NATIVE_DIR=$(cd "$(dirname "$0")" && pwd)
OUT_DIR=${OUT_DIR:-/opt/arkforge/build-quickjs}
QJS_DIR=${QJS_DIR:-$NATIVE_DIR/quickjs}
mkdir -p "$OUT_DIR"
cc -O2 -D_GNU_SOURCE -DSANDBOX_VERIFY -DCONFIG_VERSION='"arkforge-repro"' \
    -I"$QJS_DIR" -I"$NATIVE_DIR" \
    "$NATIVE_DIR/runtime_test.c" "$NATIVE_DIR/jsvm_bridge.c" \
    "$QJS_DIR/quickjs.c" "$QJS_DIR/cutils.c" "$QJS_DIR/libregexp.c" \
    "$QJS_DIR/libunicode.c" "$QJS_DIR/dtoa.c" -lm -lpthread \
    -o "$OUT_DIR/runtime_test"
"$OUT_DIR/runtime_test"
