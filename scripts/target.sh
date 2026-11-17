# Sourced by the build scripts after env.sh: what each target is.
#   x86_64-v1         AROS x86_64 ABIv1 (mainline)
#   x86_64-abiv11     AROS x86_64 ABIv11
#   aarch64-raspi-v1  AROS Raspberry Pi AArch64 ABIv1
# Sets ln_abi, ln_zk (zunekit's name for the ABI), ln_tc, ln_sdk, ln_prefix, ln_build.
ln_target=${LAUNCHER_TARGET:?Set LAUNCHER_TARGET to x86_64-v1, x86_64-abiv11 or aarch64-raspi-v1}
case "$ln_target" in
  x86_64-v1) ln_abi=v1; ln_zk=v1; ln_prefix=x86_64-aros
    ln_tc=${LAUNCHER_TOOLCHAIN_V1:?set in local.env}; ln_sdk=${LAUNCHER_SDK_V1:?set in local.env} ;;
  x86_64-abiv11) ln_abi=v11; ln_zk=v11; ln_prefix=x86_64-aros
    ln_tc=${LAUNCHER_TOOLCHAIN_ABIV11:?set in local.env}; ln_sdk=${LAUNCHER_SDK_ABIV11:?set in local.env} ;;
  aarch64-raspi-v1) ln_abi=v1; ln_zk=v1; ln_prefix=aarch64-aros
    ln_tc=${LAUNCHER_TOOLCHAIN_RASPI:?set in local.env}; ln_sdk=${LAUNCHER_SDK_RASPI:?set in local.env} ;;
  *) echo "Unknown LAUNCHER_TARGET '$ln_target'" >&2; exit 1 ;;
esac
ln_cc=$ln_tc/$ln_prefix-gcc
ln_build=${LAUNCHER_BUILD_DIR:-"$LAUNCHER_ROOT/build/$ln_target"}

# The two x86_64 ABIs share a compiler name and predefined macros, and a
# wrong compiler/SDK pair builds without complaint. Mainline's driver knows
# -noposixc and its SDK has libposixc; ABIv11 has neither.
ln_check_abi() {
  if echo 'int ln_probe;' | "$ln_cc" --sysroot="$ln_sdk" -noposixc -x c -c -o /dev/null - 2>/dev/null; then cc_abi=v1; else cc_abi=v11; fi
  if [ -e "$ln_sdk/lib/libposixc.a" ]; then sdk_abi=v1; else sdk_abi=v11; fi
  if [ "$cc_abi" != "$ln_abi" ] || [ "$sdk_abi" != "$ln_abi" ]; then
    echo "$ln_target needs an ABI$ln_abi compiler and SDK; the compiler is ABI$cc_abi, the SDK is ABI$sdk_abi" >&2
    exit 1
  fi
}

# The zunekit checkout must be at the commit this repository was written against.
ln_check_zunekit() {
  zk=${LAUNCHER_ZUNEKIT:?set in local.env: a checkout of zunekit}
  want=$(cat "$LAUNCHER_ROOT/zunekit.pin")
  have=$(git -C "$zk" rev-parse HEAD)
  [ "$want" = "$have" ] || { echo "zunekit is at $have, zunekit.pin names $want" >&2; exit 1; }
  git -C "$zk" diff --quiet -- src third_party scripts || { echo "zunekit has local changes in src, third_party or scripts" >&2; exit 1; }
}
