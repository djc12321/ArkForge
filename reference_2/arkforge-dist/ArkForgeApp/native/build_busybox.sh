#!/usr/bin/env bash
set -euo pipefail
app="$(cd "$(dirname "$0")/.." && pwd)"
cache="${ARKFORGE_NATIVE_CACHE:-/home/djc/.cache/arkforge}"
source="$cache/busybox-source/busybox-1.37.0"
build="$cache/busybox-1.37.0-ohos"
archive="$app/../../../_vendor/busybox-1.37.0-source/busybox-1.37.0.tar.bz2"
cc="$cache/ohos-sdk-toolchain/bin/clang"
printf '%s  %s\n' 3311dff32e746499f4df0d5df04d7eb396382d7e108bb9250e7b519b837043a4 "$archive" | sha256sum -c -
mkdir -p "$cache/busybox-source" "$build"
if [ ! -f "$source/Makefile" ]; then tar -xf "$archive" -C "$cache/busybox-source"; fi
make -s -C "$source" O="$build" ARCH=arm64 allnoconfig > "$build/config.log"
python3 - "$build/.config" "$app" <<'PY'
import pathlib,sys,re
p=pathlib.Path(sys.argv[1]);text=p.read_text()
enabled='DESKTOP LONG_OPTS SHOW_USAGE FEATURE_VERBOSE_USAGE FEATURE_COMPRESS_USAGE LFS PLATFORM_LINUX BUSYBOX FEATURE_PREFER_APPLETS FEATURE_SH_STANDALONE FEATURE_SH_NOFORK ASH SH_IS_ASH ASH_BASH_COMPAT ASH_JOB_CONTROL ASH_ALIAS ASH_RANDOM_SUPPORT ASH_EXPAND_PRMT ASH_ECHO ASH_PRINTF ASH_TEST ASH_GETOPTS ASH_CMDCMD FEATURE_EDITING FEATURE_EDITING_HISTORY FEATURE_EDITING_SAVEHISTORY FEATURE_TAB_COMPLETION FEATURE_EDITING_FANCY_PROMPT UNICODE_SUPPORT FEATURE_CHECK_UNICODE_IN_ENV CAT ECHO PRINTF TEST TEST1 TEST2 PWD LS FEATURE_LS_FILETYPES FEATURE_LS_FOLLOWLINKS FEATURE_LS_RECURSIVE FEATURE_LS_SORTFILES FEATURE_LS_TIMESTAMPS FEATURE_LS_USERNAME FEATURE_LS_COLOR MKDIR RMDIR TOUCH FEATURE_TOUCH_NODEREF FEATURE_TOUCH_SUSV3 CP FEATURE_CP_LONG_OPTIONS MV RM LN READLINK FEATURE_READLINK_FOLLOW REALPATH CHMOD CHOWN HEAD FEATURE_FANCY_HEAD TAIL FEATURE_FANCY_TAIL WC FEATURE_WC_LARGE CUT SORT FEATURE_SORT_BIG UNIQ TR FEATURE_TR_CLASSES FEATURE_TR_EQUIV TEE FEATURE_TEE_USE_BLOCK_IO EXPR BASENAME DIRNAME DATE FEATURE_DATE_ISOFMT SLEEP FEATURE_FANCY_SLEEP ENV WHICH SEQ CLEAR DU FEATURE_DU_DEFAULT_BLOCKSIZE_1K DF STAT FEATURE_STAT_FORMAT FIND FEATURE_FIND_NAME FEATURE_FIND_TYPE FEATURE_FIND_MTIME FEATURE_FIND_MAXDEPTH FEATURE_FIND_DELETE FEATURE_FIND_EXEC FEATURE_FIND_PRINT0 GREP FEATURE_GREP_EGREP_ALIAS FEATURE_GREP_FGREP_ALIAS FEATURE_GREP_CONTEXT SED AWK XARGS FEATURE_XARGS_SUPPORT_QUOTES FEATURE_XARGS_SUPPORT_ZERO_TERM TAR FEATURE_TAR_CREATE FEATURE_TAR_GZIP GZIP GUNZIP ZCAT UNZIP MD5SUM SHA1SUM SHA256SUM FEATURE_MD5_SHA1_SUM_CHECK YES FALSE TRUE'.split()
for name in enabled:
    text=re.sub(r'(?m)^(?:# )?CONFIG_'+name+r'(?:=.*| is not set)$','CONFIG_'+name+'=y',text)
text=re.sub(r'(?m)^CONFIG_FEATURE_EDITING_HISTORY=.*$', 'CONFIG_FEATURE_EDITING_HISTORY=256',text)
for name in ('SH_IS_NONE','SH_IS_HUSH','SHA1_HWACCEL','SHA256_HWACCEL'):
    text=re.sub(r'(?m)^(?:# )?CONFIG_'+name+r'(?:=.*| is not set)$','# CONFIG_'+name+' is not set',text)
text=re.sub(r'(?m)^CONFIG_EXTRA_CFLAGS=.*$', 'CONFIG_EXTRA_CFLAGS="--target=aarch64-linux-ohos -O2 -fPIC -include '+sys.argv[2]+'/native/busybox_ohos_compat.h"',text)
p.write_text(text)
PY
set +o pipefail
yes '' | make -s -C "$source" O="$build" ARCH=arm64 oldconfig >> "$build/config.log"
set -o pipefail
"$cc" --target=aarch64-linux-ohos -O2 -fPIC -c "$app/native/git_exec_bridge.c" -o "$build/arkforge-exec.o"
"$cc" --target=aarch64-linux-ohos -O2 -fPIC -c "$app/native/compat/musl_strverscmp.c" -o "$build/musl-strverscmp.o"
make -C "$source" O="$build" ARCH=arm64 -j6 CC="$cc" LD="$cache/linux-clang/usr/lib/llvm-18/bin/ld.lld" AR="$cache/ohos-sdk-toolchain/bin/llvm-ar" STRIP=true \
  CFLAGS_busybox="-shared -Wl,-u,main,-Bsymbolic,--wrap=fork,--wrap=vfork,--wrap=execve,--wrap=execvp,--wrap=atexit,--wrap=exit $build/arkforge-exec.o $build/musl-strverscmp.o" busybox_unstripped
cp "$build/busybox_unstripped" "$app/entry/libs/arm64-v8a/libbusybox_exec.so"
mkdir -p "$app/entry/src/main/resources/rawfile/terminal"
cp "$source/LICENSE" "$app/entry/src/main/resources/rawfile/terminal/BusyBox-LICENSE.txt"
cp "$app/native/compat/musl-COPYRIGHT" "$app/entry/src/main/resources/rawfile/terminal/musl-COPYRIGHT.txt"
cp "$build/.config" "$app/native/busybox-ohos.config"
python3 - "$app" <<'PY'
import pathlib,json,hashlib,sys
p=pathlib.Path(sys.argv[1]);sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
data={'upstream':'https://busybox.net/downloads/busybox-1.37.0.tar.bz2','version':'1.37.0','sourceSha256':'3311dff32e746499f4df0d5df04d7eb396382d7e108bb9250e7b519b837043a4','sourceChanges':[], 'shell':'BusyBox ash','license':'GPL-2.0','adapter':'native/git_exec_bridge.c','adapterSha256':sha(p/'native/git_exec_bridge.c'),'configSha256':sha(p/'native/busybox-ohos.config'),'librarySha256':sha(p/'entry/libs/arm64-v8a/libbusybox_exec.so'),'buildScript':'native/build_busybox.sh'}
(p/'entry/src/main/resources/rawfile/terminal/manifest.json').write_text(json.dumps(data,indent=2)+'\n')
PY
