#!/bin/bash
# Packs a test build for one target from an existing scripts/build.sh output:
#   LAUNCHER_TARGET=<target> LAUNCHER_LABEL=test1 scripts/package.sh
# Writes dist/Launcher-<version>-<label>-<archive name>.zip and adds its
# checksum to dist/SHA256SUMS. An archive that exists is never replaced.
set -euo pipefail
. "$(dirname "$0")/env.sh"
. "$LAUNCHER_ROOT/scripts/target.sh"
R=$LAUNCHER_ROOT
label=${LAUNCHER_LABEL:?Set LAUNCHER_LABEL, for example test1}
version=$(sed -n 's/.*\$VER: Launcher \([0-9.]*\) .*/\1/p' "$R/src/main.c")
[ -n "$version" ] || { echo "No version in src/main.c" >&2; exit 1; }
case "$ln_target" in
  x86_64-v1) arch=aros-x86_64-abiv1-pc; requires="AROS x86_64 ABIv1 (mainline), a nightly from 2026-11-06 or later." ;;
  x86_64-abiv11) arch=aros-x86_64-abiv11-pc; requires="AROS x86_64 ABIv11 (an ABIv11 distribution)." ;;
  aarch64-raspi-v1) arch=aros-aarch64-abiv1-raspi; requires="AROS Raspberry Pi AArch64 ABIv1 (mainline)." ;;
esac
bin=$ln_build/Launcher
[ -x "$bin" ] || { echo "Run scripts/build.sh first" >&2; exit 1; }
name=Launcher-$version-$label-$arch
out=$R/dist/$name.zip
[ ! -e "$out" ] || { echo "$out exists; delivered archives are not replaced (use a new label)" >&2; exit 1; }
work=$ln_build/package
rm -rf "$work"
mkdir -p "$work/Launcher" "$R/dist"
cp "$bin" "$work/Launcher/Launcher"
# the bundled widgets (scripts/build-widgets.sh), beside the program
[ -x "$ln_build/Widgets/org.aros.currency/bin/$ln_target/CurrencyProvider" ] || { echo "Run scripts/build-widgets.sh first" >&2; exit 1; }
mkdir -p "$work/Launcher/Widgets"
cp -R "$ln_build/Widgets/org.aros.currency" "$work/Launcher/Widgets/"
cp "$R/packaging/Launcher.info" "$work/Launcher/Launcher.info"
cp "$R/packaging/drawer.info" "$work/Launcher.info"
sed -e "s/@LABEL@/$label/" -e "s/@TARGET@/$arch/" -e "s|@REQUIRES@|$requires|" "$R/packaging/ReadMe.txt" > "$work/Launcher/ReadMe.txt"
# Nothing in the package names the machine it was built on.
if grep -r -a -l -E "/Users/|/Volumes/|/home/|/private/" "$work" ; then
  echo "host paths in the package (files above)" >&2; exit 1
fi
export TZ=UTC
find "$work" -exec touch -t 202611170000 {} +
(cd "$work" && zip -X -q -r "$out" Launcher.info Launcher)
(cd "$R/dist" && shasum -a 256 "$name.zip" >> SHA256SUMS)
unzip -l "$out"
echo "$out"
