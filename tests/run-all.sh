#!/bin/sh
# Run six native flow tests and the OpenWrt/QEMU production parser test.
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
CC="${CC:-cc}"
for name in flow_export flow_json flow_tcp flow_tcp_loopback flow_tcp_reconnect flow_tcp_reset; do
  case "$name" in
    flow_export) src="flow_export.c" ;;
    flow_json) src="flow_json.c" ;;
    *) src="flow_tcp.c" ;;
  esac
  echo "=== $name ==="
  "$CC" -D_GNU_SOURCE -std=gnu99 -O0 -g -Wall -Wextra -I"$ROOT" "$ROOT/tests/test_$name.c" "$ROOT/$src" -o "$OUT/test_$name"
  "$OUT/test_$name"
done
echo "=== nfnetlink_parser (OpenWrt SDK/QEMU) ==="
"$ROOT/tests/run-parser-qemu.sh"
echo "ALL SEVEN TESTS PASSED"
