#!/bin/bash
# Builds and runs the host tests of the pure modules (no AROS needed) with
# AddressSanitizer and UBSan. Needs a C and C++ compiler, FreeType
# (pkg-config freetype2) and the zunekit checkout named in local.env.
set -euo pipefail
. "$(dirname "$0")/env.sh"
R=$LAUNCHER_ROOT
ZK=${LAUNCHER_ZUNEKIT:?set in local.env: a checkout of zunekit}
[ "$(cat "$R/zunekit.pin")" = "$(git -C "$ZK" rev-parse HEAD)" ] || { echo "zunekit is not at the commit in zunekit.pin" >&2; exit 1; }
B=$R/build/host
mkdir -p "$B"
CC=${CC:-cc}
HB_O=$B/harfbuzz.o
if [ ! -f "$HB_O" ] || [ "$ZK/third_party/harfbuzz/src/harfbuzz.cc" -nt "$HB_O" ]; then
  ${CXX:-c++} -std=c++11 -O2 -DHB_NO_MT -DHB_NO_MMAP -DHB_NO_SETLOCALE -c "$ZK/third_party/harfbuzz/src/harfbuzz.cc" -o "$HB_O"
fi
F="-std=gnu99 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I$R/src -I$ZK/src -I$ZK/third_party/utf8proc -I$ZK/third_party/harfbuzz/src -DUTF8PROC_STATIC"
FT=$(pkg-config --cflags --libs freetype2)
# A font for the drawing tests: one of zunekit's fixtures.
export LN_TEST_FONT=${LN_TEST_FONT:-$ZK/tests/fixtures/fonts/OpenSans-Regular.ttf}
# Pure sources: everything in src/ that is not an AROS module or the program's main.
pure=$(ls "$R"/src/*.c 2>/dev/null | grep -v -e '_aros\.c$' -e '/main\.c$' || true)
for t in "$R"/tests/host/test_*.c; do
  n=$(basename "$t" .c)
  # shellcheck disable=SC2086
  $CC $F -o "$B/$n" "$t" $pure "$ZK/src/zk_edit_core.c" "$ZK/src/zk_glass_core.c" "$ZK/src/zk_text.c" "$ZK/third_party/utf8proc/utf8proc.c" "$HB_O" $FT ${CXXLIB:--lc++} -lm
  (cd "$R/tests" && "$B/$n")
  echo "ok $n"
done
# Pictures of the panel to look at: build/host/looks/*.png
mkdir -p "$B/looks"
# shellcheck disable=SC2086
$CC $F -o "$B/render_panel" "$R/tests/host/render_panel.c" $pure "$ZK/src/zk_edit_core.c" "$ZK/src/zk_glass_core.c" "$ZK/src/zk_text.c" "$ZK/third_party/utf8proc/utf8proc.c" "$HB_O" $FT ${CXXLIB:--lc++} -lm
"$B/render_panel" "$LN_TEST_FONT" "$B/looks"
