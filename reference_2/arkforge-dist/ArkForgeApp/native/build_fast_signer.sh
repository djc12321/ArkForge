#!/usr/bin/env bash
set -euo pipefail
app="$(cd "$(dirname "$0")/.." && pwd)"
vendor="$(cd "$app/../../../_vendor" && pwd)"
cache="${ARKFORGE_NATIVE_CACHE:-/home/djc/.cache/arkforge}"
source="${ARKFORGE_OPENSSL_SOURCE:-$vendor/openssl}"
crypto="$cache/openssl-ohos-arm64-fast"
mkdir -p "$crypto"
cd "$crypto"
if [ ! -f Makefile ]; then
  CC="$cache/ohos-sdk-toolchain/bin/clang" AR="$cache/ohos-sdk-toolchain/bin/llvm-ar" \
    perl "$source/Configure" linux-aarch64 --target=aarch64-linux-ohos -fPIC \
      no-shared no-tests no-module no-async --prefix="$crypto/install" --libdir=lib
fi
make -j8 build_libs
make install_dev
signer="${ARKFORGE_SIGNER_SOURCE:-$cache/native-adapters-lf/hapsigner/hapsigntool_cpp}"
cmake -S "$app/native/signer" -B "$cache/signer-ohos-arm64-fast" -G Ninja \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_C_COMPILER="$cache/ohos-sdk-toolchain/bin/clang" \
  -DCMAKE_CXX_COMPILER="$cache/ohos-sdk-toolchain/bin/clang++" \
  -DCMAKE_C_FLAGS=--target=aarch64-linux-ohos -DCMAKE_CXX_FLAGS=--target=aarch64-linux-ohos \
  -DOPENSSL_ROOT="$crypto/install" -DVENDOR="$vendor" -DSIGNER_SOURCE="$signer" \
  -DSECUREC_ROOT="$vendor/securec"
cmake --build "$cache/signer-ohos-arm64-fast" -j8
cp "$cache/signer-ohos-arm64-fast/libsigntool.so" "$app/entry/libs/arm64-v8a/libsigntool.so"
python3 - "$app" <<'PYMETA'
from pathlib import Path
import sys, json, hashlib
app = Path(sys.argv[1])
path = app / 'entry/src/main/resources/rawfile/backend/signer-build.json'
record = json.loads(path.read_text())
record['sha256'] = hashlib.sha256((app / 'entry/libs/arm64-v8a/libsigntool.so').read_bytes()).hexdigest()
record['crypto'] = 'Upstream linux-aarch64 OpenSSL assembly with runtime CPU feature detection'
record['buildScript'] = 'native/build_fast_signer.sh'
path.write_text(json.dumps(record, indent=2) + '\n')
PYMETA
printf 'Built official signer with upstream ARM64 OpenSSL assembly and runtime CPU feature detection.\n'
