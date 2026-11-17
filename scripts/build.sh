#!/bin/bash
# Builds the launcher for one target: LAUNCHER_TARGET=<target> scripts/build.sh
# Output: build/<target>/Launcher. The text layer comes from zunekit's
# build-textlib.sh, built with this target's compiler and SDK.
set -euo pipefail
. "$(dirname "$0")/env.sh"
. "$LAUNCHER_ROOT/scripts/target.sh"
R=$LAUNCHER_ROOT
case "$R" in *" "*) echo "The repository path must not contain spaces" >&2; exit 1 ;; esac
case "$ln_target" in x86_64-*) ln_check_abi ;; esac
ln_check_zunekit
ZK=$LAUNCHER_ZUNEKIT
mkdir -p "$ln_build"
if [ "$ln_prefix" = x86_64-aros ]; then
  ZK_TC=$ln_tc ZK_SDK=$ln_sdk "$ZK/scripts/build-textlib.sh" "$ln_zk" "$ln_build"
else
  ZK_TC=$ln_tc ZK_SDK=$ln_sdk ZK_CC=$ln_cc ZK_CXX=$ln_tc/$ln_prefix-g++ ZK_AR=$ln_tc/$ln_prefix-ar ZK_NM=$ln_tc/$ln_prefix-nm \
    "$ZK/scripts/build-textlib.sh" "$ln_zk" "$ln_build"
fi
# Binaries carry no build-machine paths.
pmap="-ffile-prefix-map=$R=launcher -ffile-prefix-map=$ln_sdk=sdk -ffile-prefix-map=$ZK=zunekit"
src=$(ls "$R"/src/*.c)
# shellcheck disable=SC2086
"$ln_cc" --sysroot="$ln_sdk" -std=gnu99 -O2 -Wall -Wextra -Werror -Wno-pointer-sign $pmap \
  -I"$R/src" -I"$ZK/src" -I"$ZK/third_party/utf8proc" -DUTF8PROC_STATIC -I"$ln_sdk/include/freetype" \
  $src "$ZK/src/zk_edit_core.c" \
  -o "$ln_build/Launcher" "$ln_build/libzktext.a" "$ln_sdk/lib/libfreetype2.static.a" "$ln_sdk/lib/libpng_nostdio.a" "$ln_sdk/lib/libz.static.a" -lm
"$ln_tc/$ln_prefix-strip" --strip-debug "$ln_build/Launcher"
echo "$ln_build/Launcher"
