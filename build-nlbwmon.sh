#!/usr/bin/env bash
set -Eeuo pipefail

OPENWRT_VERSION="25.12.6"
SDK_TOOLCHAIN="gcc-14.3.0_musl.Linux-x86_64"
SDK_BASE="$HOME/projects/openwrt-sdk"
SOURCE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_STAGE_DIR="$HOME/projects/nlbwmon-build-src"
EXPECTED_BRANCH="feature/beta2-timestamps-sequence"
OUTPUT_BASE="$HOME/projects/openwrt-packages/$OPENWRT_VERSION"
JOBS=$(nproc)
PACKAGE_TARGET="package/feeds/packages/nlbwmon"

fail() { echo "ERROR: $*" >&2; exit 1; }
TARGET="filogic"
CLEAN=0
while (($#)); do
  case "$1" in
    --target) (($# >= 2)) || fail '--target needs an argument'; TARGET="$2"; shift 2 ;;
    --target=*) TARGET="${1#*=}"; shift ;;
    --clean) CLEAN=1; shift ;;
    -h|--help) echo "Usage: $0 [--target filogic|x86-64] [--clean]"; exit 0 ;;
    *) fail "Unknown argument: $1" ;;
  esac
done

case "$TARGET" in
  filogic|mediatek-filogic) TARGET="filogic"; SDK_TARGET="mediatek-filogic"; BUILD_TARGET="aarch64_cortex-a53" ;;
  x86-64|x86_64) TARGET="x86-64"; SDK_TARGET="x86-64"; BUILD_TARGET="x86_64" ;;
  *) fail "Unknown target '$TARGET' (use filogic or x86-64)" ;;
esac

SDK_DIR="$SDK_BASE/openwrt-sdk-${OPENWRT_VERSION}-${SDK_TARGET}_${SDK_TOOLCHAIN}"
OUTPUT_DIR="$OUTPUT_BASE/$SDK_TARGET"
BUILD_ROOT="$SDK_DIR/build_dir/target-${BUILD_TARGET}_musl"
FEED_DIR="$SDK_DIR/feeds/packages/net/nlbwmon"
FEED_MAKEFILE="$FEED_DIR/Makefile"
VERSION_FILE="$SOURCE_DIR/openwrt/fork-version.mk"
[[ -d "$SDK_DIR" && -f "$SDK_DIR/.config" ]] || fail "SDK missing or not configured: $SDK_DIR"
[[ -e "$SOURCE_DIR/.git" ]] || fail "Git source directory missing: $SOURCE_DIR"
[[ -f "$SOURCE_DIR/flow_json.c" && -f "$SOURCE_DIR/CMakeLists.txt" ]] || fail "Expected source files missing"
[[ "$(git -C "$SOURCE_DIR" branch --show-current)" == "$EXPECTED_BRANCH" ]] || fail "Wrong Git branch"
DIRTY_SOURCE="$(git -C "$SOURCE_DIR" status --porcelain --untracked-files=no -- . ':(exclude)build-nlbwmon.sh' ':(exclude)deploy-nlbwmon.sh')"
[[ -z "$DIRTY_SOURCE" ]] || fail "Tracked source changes not committed; commit first: $DIRTY_SOURCE"
[[ -f "$VERSION_FILE" ]] || fail "Missing fork version file: $VERSION_FILE"
VERSION="$(sed -n 's/^FORK_PKG_VERSION:=//p' "$VERSION_FILE")"
RELEASE="$(sed -n 's/^FORK_PKG_RELEASE:=//p' "$VERSION_FILE")"
[[ "$VERSION" =~ ^[0-9A-Za-z._]+$ && "$RELEASE" =~ ^[0-9]+$ ]] || fail "Invalid fork package version"
EXPECTED_PKG_VERSION="$VERSION-r$RELEASE"
grep -Eq '^CONFIG_PACKAGE_nlbwmon=[my]$' "$SDK_DIR/.config" || fail "nlbwmon not enabled in SDK"
[[ -f "$FEED_MAKEFILE" ]] || fail "Feed Makefile missing"
grep -Eq '^PKG_NAME:=nlbwmon$' "$FEED_MAKEFILE" || fail "Unexpected feed recipe"
[[ "$SOURCE_STAGE_DIR" != "$SOURCE_DIR" && "$SOURCE_STAGE_DIR" == "$HOME/projects/nlbwmon-build-src" ]] || fail "Unsafe staging path"
for cmd in rsync flock python3 sha256sum; do command -v "$cmd" >/dev/null || fail "$cmd is required"; done
for name in nlbwmon.init nlbwmon.config; do [[ -f "$SOURCE_DIR/openwrt/files/$name" ]] || fail "Missing $name in Git"; done

mkdir -p "$SOURCE_STAGE_DIR" "$OUTPUT_DIR/logs"
exec 9>"$SDK_BASE/.nlbwmon-build.lock"
flock -n 9 || fail "Another nlbwmon build is running"
LOG="$OUTPUT_DIR/logs/nlbwmon-$(date +%Y%m%d-%H%M%S).log"
REV="$(git -C "$SOURCE_DIR" rev-parse HEAD)"
echo "Target=$TARGET SDK=$SDK_DIR"
echo "Source=$SOURCE_DIR ($EXPECTED_BRANCH @ $REV)"
echo "Package=nlbwmon-$EXPECTED_PKG_VERSION"
echo "Log=$LOG"
TEMP="$(mktemp -d)"
trap 'rm -rf -- "$TEMP"' EXIT
git -C "$SOURCE_DIR" archive HEAD | tar -x -C "$TEMP"
rsync -a --delete "$TEMP/" "$SOURCE_STAGE_DIR/"
[[ -f "$SOURCE_STAGE_DIR/flow_json.c" ]] || fail "Staging sync failed"

# Keep an untouched upstream recipe as the basis of every build, even after a previous fork build.
UPSTREAM_MAKEFILE="$FEED_MAKEFILE.upstream.bak"
if [[ ! -f "$UPSTREAM_MAKEFILE" ]]; then
  grep -q '^PKG_VERSION:=' "$FEED_MAKEFILE" && fail "Recipe already modified and no upstream backup exists"
  cp -p "$FEED_MAKEFILE" "$UPSTREAM_MAKEFILE"
fi
python3 - "$UPSTREAM_MAKEFILE" "$FEED_MAKEFILE" "$SOURCE_STAGE_DIR" "$VERSION" "$RELEASE" <<'PY'
import pathlib, re, sys
source, dest, stage, version, release = sys.argv[1:]
s = pathlib.Path(source).read_text()
if not re.search(r'^PKG_NAME:=nlbwmon$', s, re.M) or not re.search(r'^PKG_RELEASE:=', s, re.M):
    raise SystemExit('Unexpected upstream nlbwmon Makefile')
s = re.sub(r'^USE_SOURCE_DIR:=.*\n', '', s, flags=re.M)
s = re.sub(r'^PKG_VERSION:=.*\n', '', s, flags=re.M)
s = re.sub(r'^PKG_RELEASE:=.*$', 'PKG_RELEASE:=' + release, s, count=1, flags=re.M)
s = s.replace('include $(TOPDIR)/rules.mk\n', 'include $(TOPDIR)/rules.mk\nUSE_SOURCE_DIR:=' + stage + '\n', 1)
s = s.replace('PKG_NAME:=nlbwmon\n', 'PKG_NAME:=nlbwmon\nPKG_VERSION:=' + version + '\n', 1)
pathlib.Path(dest).write_text(s)
PY
for name in nlbwmon.init nlbwmon.config; do cp -f "$SOURCE_STAGE_DIR/openwrt/files/$name" "$FEED_DIR/files/$name"; done

run_make() { (cd "$SDK_DIR" && env -u USE_SOURCE_DIR -u TOPDIR -u STAGING_DIR -u BUILD_DIR make "$@") 2>&1 | tee -a "$LOG"; }
echo "OpenWrt=$OPENWRT_VERSION Target=$TARGET Commit=$REV Package=$EXPECTED_PKG_VERSION" > "$LOG"
# The package version and installed files changed: force a clean rebuild.
run_make "$PACKAGE_TARGET/clean" V=s || fail "Clean failed; see $LOG"
mapfile -t OLD_APKS < <(find "$SDK_DIR/bin/packages" -type f -name 'nlbwmon-*.apk' 2>/dev/null || true)
for apk in "${OLD_APKS[@]}"; do rm -f -- "$apk"; done
if ! run_make "$PACKAGE_TARGET/compile" V=s -j"$JOBS"; then
  grep -nEi 'error:|fatal error|ERROR:|\*\*\*' "$LOG" | tail -35 >&2 || true
  fail "Build failed; see $LOG"
fi
FOUND=0
for p in "$BUILD_ROOT"/nlbwmon-*; do
  if [[ -L "$p" && "$(readlink -f "$p")" == "$(readlink -f "$SOURCE_STAGE_DIR")" ]]; then FOUND=1; break; fi
done
(( FOUND )) || fail "Build directory does not link to staging source"
mapfile -t APKS < <(find "$SDK_DIR/bin/packages" -type f -name 'nlbwmon-*.apk' 2>/dev/null)
(( ${#APKS[@]} == 1 )) || fail "Expected exactly one freshly built APK, got ${#APKS[@]}"
APK="${APKS[0]}"
[[ "$(basename "$APK")" == "nlbwmon-$EXPECTED_PKG_VERSION.apk" ]] || fail "Unexpected APK filename: $APK"
APK_TOOL="$SDK_DIR/staging_dir/host/bin/apk"
[[ -x "$APK_TOOL" ]] || fail "SDK APK inspection tool missing"
"$APK_TOOL" adbdump "$APK" > "$TEMP/apk-metadata.txt" || fail "APK metadata inspection failed"
grep -Fq "$EXPECTED_PKG_VERSION" "$TEMP/apk-metadata.txt" || fail "Fork version not found in APK metadata"
grep -Fq "$BUILD_TARGET" "$TEMP/apk-metadata.txt" || fail "Target architecture not found in APK metadata"
cp -v -- "$APK" "$OUTPUT_DIR/"
sha256sum "$OUTPUT_DIR/$(basename "$APK")" | tee "$OUTPUT_DIR/$(basename "$APK").sha256"
printf 'Git commit: %s\nOpenWrt: %s\nTarget: %s\nPackage: %s\n' "$REV" "$OPENWRT_VERSION" "$SDK_TARGET" "$EXPECTED_PKG_VERSION" > "$OUTPUT_DIR/$(basename "$APK").buildinfo"
echo "SUCCESS: $APK -> $OUTPUT_DIR"
echo "No router was modified. Verify installation and rollback in the lab before release."
