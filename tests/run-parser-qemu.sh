#!/bin/sh
# Build and execute the production conntrack parser integration test under QEMU.
set -eu

SDK="${OPENWRT_SDK:-$HOME/projects/openwrt-sdk/openwrt-sdk-25.12.6-mediatek-filogic_gcc-14.3.0_musl.Linux-x86_64}"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="$SDK/staging_dir/target-aarch64_cortex-a53_musl"
TOOLCHAIN="$SDK/staging_dir/toolchain-aarch64_cortex-a53_gcc-14.3.0_musl"
SYSROOT="$TARGET/root-mediatek"
CC="$TOOLCHAIN/bin/aarch64-openwrt-linux-musl-gcc"

for f in "$CC" "$TARGET/usr/include/libnl-tiny/netlink/netlink.h" "$SYSROOT/lib/ld-musl-aarch64.so.1"; do
    if [ ! -e "$f" ]; then
        echo "Missing SDK dependency: $f" >&2
        exit 1
    fi
done
if ! command -v qemu-aarch64 >/dev/null 2>&1; then
    echo "Install qemu-user (qemu-aarch64) before running this test" >&2
    exit 1
fi

mkdir -p "$ROOT/build-test"
export STAGING_DIR="$SDK/staging_dir"
"$CC" -D_GNU_SOURCE -std=gnu99 -O0 -g -ffunction-sections -fdata-sections -I"$TARGET/usr/include" -I"$TARGET/usr/include/libnl-tiny" "$ROOT/tests/test_nfnetlink_parser.c" "$ROOT/nfnetlink.c" "$ROOT/flow_export.c" -Wl,--gc-sections -L"$TARGET/usr/lib" -lnl-tiny -lubox -o "$ROOT/build-test/test_nfnetlink_parser"
qemu-aarch64 -L "$SYSROOT" -E LD_LIBRARY_PATH="$TARGET/usr/lib" "$ROOT/build-test/test_nfnetlink_parser"
