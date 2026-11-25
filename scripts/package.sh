#!/bin/bash
# Packs a test build for one target from an existing scripts/build.sh output:
#   LAUNCHER_TARGET=<target> scripts/package.sh
# Writes dist/Jumpstart-<version>-<archive name>.zip (LAUNCHER_LABEL, when
# set, goes between the version and the archive name) and adds its
# checksum to dist/SHA256SUMS. An archive that exists is never replaced.
set -euo pipefail
. "$(dirname "$0")/env.sh"
. "$LAUNCHER_ROOT/scripts/target.sh"
R=$LAUNCHER_ROOT
label=${LAUNCHER_LABEL:-}
version=$(sed -n 's/.*\$VER: Jumpstart \([0-9.]*\) .*/\1/p' "$R/src/main.c")
[ -n "$version" ] || { echo "No version in src/main.c" >&2; exit 1; }
case "$ln_target" in
  x86_64-v1) arch=aros-x86_64-abiv1-pc; requires="AROS x86_64 ABIv1 (mainline), a nightly from 2026-11-24 or later." ;;
  x86_64-abiv11) arch=aros-x86_64-abiv11-pc; requires="AROS x86_64 ABIv11 (an ABIv11 distribution)." ;;
  aarch64-raspi-v1) arch=aros-aarch64-abiv1-raspi; requires="AROS Raspberry Pi AArch64 ABIv1 (mainline), a nightly from 2026-11-24 or later." ;;
esac
bin=$ln_build/Launcher
[ -x "$bin" ] || { echo "Run scripts/build.sh first" >&2; exit 1; }
name=Jumpstart-$version${label:+-$label}-$arch
out=$R/dist/$name.zip
[ ! -e "$out" ] || { echo "$out exists; delivered archives are not replaced (raise the version)" >&2; exit 1; }
work=$ln_build/package
rm -rf "$work"
mkdir -p "$work/Jumpstart" "$R/dist"
cp "$bin" "$work/Jumpstart/Jumpstart"
# the bundled widgets (scripts/build-widgets.sh), beside the program
[ -x "$ln_build/Widgets/org.aros.currency/bin/$ln_target/CurrencyProvider" ] || { echo "Run scripts/build-widgets.sh first" >&2; exit 1; }
mkdir -p "$work/Jumpstart/Widgets"
cp -R "$ln_build/Widgets/org.aros.currency" "$work/Jumpstart/Widgets/"
# colour themes (Limpet's theme files) with the licences of their colours
mkdir -p "$work/Jumpstart/Themes"
cp "$R"/themes/*.theme "$R/themes/THEME-LICENSES.txt" "$work/Jumpstart/Themes/"
cp "$R/packaging/Jumpstart.info" "$work/Jumpstart/Jumpstart.info"
cp "$R/packaging/drawer.info" "$work/Jumpstart.info"
sed -e "s/@VERSION@/$version/" -e "s/@TARGET@/$arch/" -e "s|@REQUIRES@|$requires|" "$R/packaging/ReadMe.txt" > "$work/Jumpstart/ReadMe.txt"
# /home/runner: the same for the SDKs of nightly builds made on GitHub
# runners (seen in libcurl and libcrypto of nightly 2026-11-24).
# Nothing in the package names the machine it was built on. /home/vsts is
# the AROS project's own build service, whose paths are inside the contrib
# curl and OpenSSL of the official Pi SDK.
found=$(LC_ALL=C grep -r -a -o -h -E "(/Users/|/Volumes/|/private/|/home/)[A-Za-z0-9_.-]*" "$work" | grep -v -x -e "/home/vsts" -e "/home/runner" | sort -u || true)
if [ -n "$found" ]; then
  echo "host paths in the package:" $found >&2; exit 1
fi
export TZ=UTC
find "$work" -exec touch -t 202611170000 {} +
(cd "$work" && zip -X -q -r "$out" Jumpstart.info Jumpstart)
(cd "$R/dist" && shasum -a 256 "$name.zip" >> SHA256SUMS)
unzip -l "$out"
echo "$out"
