#!/usr/bin/env bash
set -Eeuo pipefail
# Run from a clean checkout of feature/flow-lifecycle-events.
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"
[[ "$(git branch --show-current)" == "feature/flow-lifecycle-events" ]] || { echo "Wrong Git branch" >&2; exit 1; }
[[ -z "$(git status --porcelain)" ]] || { echo "Dirty Git tree: commit or stash first" >&2; exit 1; }
[[ $# -eq 1 ]] || { echo "Usage: $0 /absolute/path/to/openwrt-sdk" >&2; exit 1; }
SDK="$(realpath "$1")"
[[ -f "$SDK/rules.mk" && -f "$SDK/feeds/packages/net/nlbwmon/Makefile" ]] || { echo "Not a supported SDK / nlbwmon feed missing" >&2; exit 1; }
[[ -f "$ROOT/openwrt/fork-version.mk" ]] || exit 1
VERSION="$(sed -n 's/^FORK_PKG_VERSION:=//p' "$ROOT/openwrt/fork-version.mk")"
RELEASE="$(sed -n 's/^FORK_PKG_RELEASE:=//p' "$ROOT/openwrt/fork-version.mk")"
[[ "$VERSION" =~ ^[a-zA-Z0-9._]+$ && "$RELEASE" =~ ^[0-9]+$ ]] || { echo "Invalid fork version" >&2; exit 1; }
STAGE="$HOME/projects/nlbwmon-build-src"
mkdir -p "$STAGE"
find "$STAGE" -mindepth 1 -maxdepth 1 -exec rm -rf -- {} +
git archive HEAD | tar -xf - -C "$STAGE"
RECIPE="$SDK/feeds/packages/net/nlbwmon/Makefile"
cp -n "$RECIPE" "$RECIPE.upstream.bak"
python3 - "$RECIPE" "$STAGE" "$VERSION" "$RELEASE" <<'PY'
import pathlib, re, sys
p=pathlib.Path(sys.argv[1])
stage, version, release=sys.argv[2:]
s=p.read_text()
s=re.sub(r'^USE_SOURCE_DIR:=.*\n', '', s, flags=re.M)
s=re.sub(r'^PKG_VERSION:=.*\n', '', s, flags=re.M)
s=re.sub(r'^PKG_RELEASE:=.*$', 'PKG_RELEASE:='+release, s, flags=re.M)
assert re.search(r'^PKG_RELEASE:=', s, re.M)
s=s.replace('include $(TOPDIR)/rules.mk\n', 'include $(TOPDIR)/rules.mk\nUSE_SOURCE_DIR:='+stage+'\n', 1)
s=s.replace('PKG_NAME:=nlbwmon\n', 'PKG_NAME:=nlbwmon\nPKG_VERSION:='+version+'\n', 1)
p.write_text(s)
PY
for name in nlbwmon.init nlbwmon.config; do
  [[ -f "$ROOT/openwrt/files/$name" ]] || { echo "Missing $name" >&2; exit 1; }
  cp "$ROOT/openwrt/files/$name" "$SDK/feeds/packages/net/nlbwmon/files/$name"
done
echo "Git commit: $(git rev-parse HEAD)"
echo "Fork package: nlbwmon-$VERSION-r$RELEASE"
echo "SDK: $SDK"
echo "Source: $STAGE"
echo "Building..."
make -C "$SDK" package/nlbwmon/clean V=s
make -C "$SDK" package/nlbwmon/compile V=s
echo "Built packages:"
find "$SDK/bin/packages" -type f -name "nlbwmon-*.apk" -print
echo "Inspect APK metadata with: $SDK/staging_dir/host/bin/apk adbdump <APK>"
