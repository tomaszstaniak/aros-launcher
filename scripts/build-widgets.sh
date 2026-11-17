#!/bin/bash
# Builds the provider of the bundled Currency widget for one target:
#   LAUNCHER_TARGET=<target> scripts/build-widgets.sh
# Output: build/<target>/Widgets/org.aros.currency, complete (manifest,
# certificates, licence texts, provider). The provider links libcurl and its
# TLS library statically, so the SDK must have them (mainline: contrib).
# When LAUNCHER_SDK_V1 has none, LAUNCHER_SDK_V1_NET names an SDK that does:
# the build still uses LAUNCHER_SDK_V1 for the system's own headers and
# libraries and takes only what is missing there from the other one.
set -euo pipefail
. "$(dirname "$0")/env.sh"
. "$LAUNCHER_ROOT/scripts/target.sh"
R=$LAUNCHER_ROOT
ln_check_zunekit
ZK=$LAUNCHER_ZUNEKIT
sdk=$ln_sdk
net=$ln_sdk
case "$ln_target" in
  x86_64-abiv11) libs="-lcurl -lssl -lcrypto -lzstd -lz.static -lm"; extra= ;;
  x86_64-v1) [ -f "$ln_sdk/lib/libcurl.a" ] || net=${LAUNCHER_SDK_V1_NET:-$ln_sdk}
    # contrib curl over contrib OpenSSL; sockets through miami.library and
    # net.lib; -lpthread named so that its init and exit sets are linked
    libs="-lcurl -lssl -lcrypto -lzstd-static -lbrotlidec -lbrotlicommon -lz.static -lmiami -lnet -latomic -lpthread -lm"
    extra="-DZK_HTTPS_STDC_COMPAT $ZK/src/zk_https_stdc_compat.c" ;;
  aarch64-raspi-v1) [ -f "$ln_sdk/lib/libcurl.a" ] || net=${LAUNCHER_SDK_RASPI_NET:-$ln_sdk}
    libs="-lcurl -lssl -lcrypto -lzstd-static -lbrotlidec -lbrotlicommon -lz.static -lmiami -lnet -latomic -lpthread -lm"
    extra="-DZK_HTTPS_STDC_COMPAT $ZK/src/zk_https_stdc_compat.c" ;;
esac
for need in lib/libcurl.a lib/libssl.a lib/libcrypto.a include/curl/curl.h; do
  [ -f "$net/$need" ] || { echo "The Currency provider for $ln_target needs $need in $net" >&2; exit 1; }
done
src=$R/widgets/org.aros.currency
out=$ln_build/Widgets/org.aros.currency
rm -rf "$out"
mkdir -p "$out/bin/$ln_target"
pmap="-ffile-prefix-map=$R=launcher -ffile-prefix-map=$sdk=sdk -ffile-prefix-map=$net=netsdk -ffile-prefix-map=$ZK=zunekit"
# headers and libraries of the second SDK come after the first one's
over=
[ "$net" = "$sdk" ] || over="-idirafter $net/include -L$net/lib"
# shellcheck disable=SC2086
"$ln_cc" --sysroot="$sdk" -std=gnu99 -O2 -Wall -Wextra -Werror -Wno-pointer-sign $pmap -I"$ZK/src" -I"$src/provider" $over \
  "$src/provider/main.c" "$src/provider/rates.c" "$ZK/src/zk_https.c" "$ZK/src/zk_json.c" "$ZK/src/zk_widget_snapshot.c" $extra \
  -o "$out/bin/$ln_target/CurrencyProvider" $libs
"$ln_tc/$ln_prefix-strip" --strip-debug "$out/bin/$ln_target/CurrencyProvider"
# The static curl and OpenSSL carry the paths they were built at (a compiler
# line, a source file name). Where those name this machine's volume, the
# volume's part is overwritten in place with a neutral one of the same length.
python3 - "$out/bin/$ln_target/CurrencyProvider" "$net" <<'PY'
import sys
path, net = sys.argv[1], sys.argv[2]
parts = net.split('/')
if len(parts) > 3 and parts[1] in ('Volumes', 'Users', 'home'):
    old = ('/' + parts[1] + '/' + parts[2]).encode()
    new = ('/' + 'sdk'.ljust(len(old) - 1, '_')).encode()
    data = open(path, 'rb').read()
    if old in data:
        open(path, 'wb').write(data.replace(old, new))
PY
# an init or exit set entry left outside the lists never runs (AROS issue 2071)
orphans=$("$ln_tc/$ln_prefix-nm" "$out/bin/$ln_target/CurrencyProvider" | awk '$3 ~ /^__aros_set_/ && $1 ~ /^0+$/ {print $3}')
[ -z "$orphans" ] || { echo "provider has init/exit set entries outside the lists:" $orphans >&2; exit 1; }
cp "$src/applet.json" "$src/settings.json" "$src/views.json" "$src/LICENSE" "$out/"
cp -R "$src/certs" "$src/licenses" "$src/assets" "$out/"
echo "$out"
