#!/bin/bash
# Packs the Currency widget by itself for one target, for hosts that take
# widget packages (Dockbar 0.6.0 and later):
#   LAUNCHER_TARGET=<target> scripts/package-widget.sh
# Writes dist/Currency-widget-<version>-<archive name>.zip from an existing
# scripts/build-widgets.sh output. An archive that exists is never replaced.
set -euo pipefail
. "$(dirname "$0")/env.sh"
. "$LAUNCHER_ROOT/scripts/target.sh"
R=$LAUNCHER_ROOT
case "$ln_target" in
  x86_64-v1) arch=aros-x86_64-abiv1-pc ;;
  x86_64-abiv11) arch=aros-x86_64-abiv11-pc ;;
  aarch64-raspi-v1) arch=aros-aarch64-abiv1-raspi ;;
esac
src=$ln_build/Widgets/org.aros.currency
[ -x "$src/bin/$ln_target/CurrencyProvider" ] || { echo "Run scripts/build-widgets.sh first" >&2; exit 1; }
version=$(sed -n 's/.*"version": *"\([^"]*\)".*/\1/p' "$src/applet.json")
name=Currency-widget-$version-$arch
out=$R/dist/$name.zip
[ ! -e "$out" ] || { echo "$out exists; delivered archives are not replaced (raise the widget's version)" >&2; exit 1; }
work=$ln_build/widget-package
rm -rf "$work"
mkdir -p "$work" "$R/dist"
cp -R "$src" "$work/org.aros.currency"
cat > "$work/org.aros.currency/ReadMe.txt" <<TXT
Currency widget $version ($arch)

Euro foreign exchange reference rates of the European Central Bank, for
programs that take widget packages.

Dockbar 0.6.0 or later: copy the org.aros.currency drawer into the Widgets
drawer beside Dockbar, then use Add widget..., Rescan, Currency, Add. The
settings name the pair of currencies (codes such as EUR, USD, PLN) and how
often to refresh. The tile shows the rate; a click shows all rates of the
day.

The network has to be running. Only the bank's public file is asked for.
Licence texts of what the provider contains are in the licenses drawer.
TXT
if grep -r -a -l -E "/Users/|/Volumes/|/home/|/private/" "$work" ; then
  echo "host paths in the package (files above)" >&2; exit 1
fi
export TZ=UTC
find "$work" -exec touch -t 202611170000 {} +
(cd "$work" && zip -X -q -r "$out" org.aros.currency)
(cd "$R/dist" && shasum -a 256 "$name.zip" >> SHA256SUMS)
echo "$out"
