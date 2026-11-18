#!/usr/bin/env bash
set -Eeuo pipefail

VERSION='25.12.6'
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="$SCRIPT_DIR/openwrt/fork-version.mk"
[[ -f "$VERSION_FILE" ]] || { echo "Missing $VERSION_FILE" >&2; exit 1; }
FORK_VERSION="$(sed -n 's/^FORK_PKG_VERSION:=//p' "$VERSION_FILE")"
FORK_RELEASE="$(sed -n 's/^FORK_PKG_RELEASE:=//p' "$VERSION_FILE")"
[[ "$FORK_VERSION" =~ ^[0-9A-Za-z._]+$ && "$FORK_RELEASE" =~ ^[0-9]+$ ]] || { echo 'Invalid package version' >&2; exit 1; }
PACKAGE="nlbwmon-${FORK_VERSION}-r${FORK_RELEASE}.apk"
BASE="$HOME/projects/openwrt-packages/$VERSION"
BACKUP_ROOT="$HOME/nlbwmon-deploy-backups"
LAB_HOST=''
FILOGIC_HOST=''
CONFIG="$SCRIPT_DIR/deploy-nlbwmon.local.conf"
[[ -f "$CONFIG" ]] && source "$CONFIG"
SSH_OPTS=(-o BatchMode=yes -o StrictHostKeyChecking=yes -o ConnectTimeout=10)
TARGET='' HOST='' MODE=''
fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
usage() { printf 'Usage: %s [--help]\nInteractive nlbwmon check/deploy/rollback for OpenWrt %s.\n' "$0" "$VERSION"; }
[[ $# -eq 0 ]] || { [[ $# -eq 1 && $1 == --help ]] && { usage; exit 0; }; usage; exit 2; }
[[ -t 0 && -t 1 ]] || fail 'Interactive terminal required'
for c in ssh scp sha256sum tar mktemp awk cmp; do command -v "$c" >/dev/null || fail "Missing: $c"; done
valid_host() { [[ "$HOST" =~ ^[a-zA-Z0-9_.-]+@[a-zA-Z0-9_.:-]+$ ]]; }
remote() { ssh "${SSH_OPTS[@]}" "$HOST" "$@"; }

choose_target() {
  printf '\nChoose platform:\n  1) x86-64 — lab VM\n  2) filogic — MediaTek\n  0) Cancel\n'
  local n; read -r -p 'Selection: ' n
  case "$n" in
    1) TARGET=x86-64; HOST=$LAB_HOST; RELEASE_TARGET=x86/64; RELEASE_ARCH=x86_64; APK_DIR=x86-64 ;;
    2) TARGET=filogic; HOST=$FILOGIC_HOST; RELEASE_TARGET=mediatek/filogic; RELEASE_ARCH=aarch64_cortex-a53; APK_DIR=mediatek-filogic ;;
    0) exit 0 ;;
    *) printf 'Invalid choice\n'; return 1 ;;
  esac
}
choose_host() {
  local preset answer
  case "$TARGET" in x86-64) preset=$LAB_HOST ;; filogic) preset=$FILOGIC_HOST ;; esac
  printf '\nSSH destination:\n  1) Configured host — %s\n  2) Enter manually\n  0) Back\n' "${preset:-not configured}"
  read -r -p 'Selection: ' answer
  case "$answer" in
    1) [[ -n "$preset" ]] || { printf 'Host not configured\n'; return 1; }; HOST=$preset ;;
    2) read -r -p 'SSH destination (user@host): ' HOST ;;
    0) return 1 ;;
    *) return 1 ;;
  esac
  valid_host || { printf 'Invalid SSH destination\n'; return 1; }
}
choose_mode() {
  printf '\nOperation:\n  1) Check only (read-only)\n  2) Deploy alpha\n  3) Roll back to official repository package\n  0) Cancel\n'
  local n; read -r -p 'Selection: ' n
  case "$n" in 1) MODE=check ;; 2) MODE=deploy ;; 3) MODE=rollback ;; 0) exit 0 ;; *) return 1 ;; esac
}
while :; do
  choose_target || continue
  choose_host || continue
  choose_mode || continue
  APK="$BASE/$APK_DIR/$PACKAGE"
  printf '\n========== REVIEW ==========\n'
  printf 'Operation:       %s\nPlatform:        %s\nSSH destination: %s\nOpenWrt version: %s\nOpenWrt target:  %s\nPackage arch:    %s\nCustom APK:      %s\nBackup root:     %s\n' "$MODE" "$TARGET" "$HOST" "$VERSION" "$RELEASE_TARGET" "$RELEASE_ARCH" "$APK" "$BACKUP_ROOT"
  printf '============================\n  1) Continue\n  2) Choose again\n  0) Cancel\n'
  read -r -p 'Selection: ' answer
  case "$answer" in 1) break ;; 2) continue ;; 0) exit 0 ;; esac
done

if [[ $MODE == deploy ]]; then [[ -f "$APK" ]] || fail "Custom APK missing: $APK"; fi
printf '\nChecking device identity and prerequisites...\n'
remote_check='set -eu
. /etc/openwrt_release
[ "$DISTRIB_RELEASE" = "$1" ] && [ "$DISTRIB_TARGET" = "$2" ] && [ "$DISTRIB_ARCH" = "$3" ]
[ -x /usr/sbin/nlbw ] && [ -x /etc/init.d/nlbwmon ] && [ -f /etc/config/nlbwmon ]
command -v apk >/dev/null && command -v sha256sum >/dev/null && command -v tar >/dev/null
apk info -e nlbwmon >/dev/null
'
remote "sh -s -- '$VERSION' '$RELEASE_TARGET' '$RELEASE_ARCH'" <<< "$remote_check" || fail 'Remote identity mismatch or missing prerequisites. No changes made.'
remote 'printf "Hostname: "; cat /proc/sys/kernel/hostname; printf "Installed: "; apk list --installed nlbwmon; printf "Binary SHA256: "; sha256sum /usr/sbin/nlbwmon; flow=$(uci -q get nlbwmon.flow_export.enabled 2>/dev/null || true); printf "Flow export enabled: %s\n" "${flow:-not configured}"'
if [[ $MODE == deploy ]]; then LOCAL_SHA=$(sha256sum "$APK" | awk '{print $1}'); printf 'Local APK SHA256: %s\n' "$LOCAL_SHA"; fi
if [[ $MODE == check ]]; then printf '\nCHECK OK. No changes made.\n'; exit 0; fi

# Explicit official package selection, not a generic 'apk upgrade'. Pre-fetch before stopping the service.
if [[ $MODE == rollback ]]; then
  printf '\nChecking official repository package availability (read-only)...\n'
  remote 'apk update >/dev/null && apk list -a nlbwmon' || fail 'Cannot query official repository. No changes made.'
  read -r -p 'Enter exact OFFICIAL repository package version (e.g. 2025.06.02~29236be6-r1): ' STOCK_VERSION
  [[ "$STOCK_VERSION" =~ ^[0-9A-Za-z.~_+-]+$ ]] || fail 'Invalid stock version'
  [[ "$STOCK_VERSION" != "$FORK_VERSION-r$FORK_RELEASE" ]] || fail 'Stock version equals fork version'
fi

printf '\nWARNING: %s will stop/restart nlbwmon. No automatic rollback on failure.\n' "$MODE"
printf 'Live database is preserved. A backup is taken while the service runs and may not be fully consistent.\n'
read -r -p "Type exactly '${MODE^^}' to proceed: " confirm
[[ "$confirm" == "${MODE^^}" ]] || { echo 'Cancelled'; exit 0; }

STAMP=$(date -u +%Y%m%dT%H%M%SZ)
SAFE_HOST=${HOST//@/_}; SAFE_HOST=${SAFE_HOST//:/_}
BACKUP_DIR="$BACKUP_ROOT/$SAFE_HOST-$TARGET-$STAMP"
(umask 077; mkdir -p "$BACKUP_DIR")
printf '%s\n' "$HOST" > "$BACKUP_DIR/host.txt"
if [[ $MODE == deploy ]]; then cp -- "$APK" "$BACKUP_DIR/custom.apk"; sha256sum "$BACKUP_DIR/custom.apk" > "$BACKUP_DIR/SHA256SUMS"; fi
remote 'set -eu; paths=""; for p in /etc/config/nlbwmon /etc/nlbwmon /var/lib/nlbwmon /usr/share/nlbwmon; do if [ -e "$p" ]; then paths="$paths ${p#/}"; fi; done; [ -n "$paths" ]; tar -czf - -C / $paths' > "$BACKUP_DIR/router-data.tar.gz" || fail "Backup failed. No install performed. Directory: $BACKUP_DIR"
tar -tzf "$BACKUP_DIR/router-data.tar.gz" >/dev/null || fail 'Backup archive corrupt. No install performed.'
remote 'sha256sum /etc/config/nlbwmon' > "$BACKUP_DIR/config-before.sha256" || fail 'Cannot fingerprint configuration'
remote 'apk list --installed nlbwmon' > "$BACKUP_DIR/installed-before.txt"
printf 'Backup saved locally: %s\n' "$BACKUP_DIR"

REMOTE_APK="/tmp/nlbwmon-deploy-$$.apk"
if [[ $MODE == deploy ]]; then
  scp "${SSH_OPTS[@]}" -O -q "$APK" "$HOST:$REMOTE_APK" || fail 'Upload failed; service unchanged'
  REMOTE_SHA=$(remote "sha256sum '$REMOTE_APK'" | awk '{print $1}')
  [[ $LOCAL_SHA == "$REMOTE_SHA" ]] || fail 'Transferred APK checksum mismatch; service unchanged'
else
  # Fetch repository candidate, then verify exact version and architecture before installing.
  STOCK_DIR="/tmp/nlbwmon-stock-$$"
  remote "set -eu; mkdir -p '$STOCK_DIR'; apk fetch --from repositories --output '$STOCK_DIR' nlbwmon" || fail 'Official package fetch failed. Service unchanged.'
  remote "set -eu; candidate='$STOCK_DIR/nlbwmon-$STOCK_VERSION.apk'; test -f \"\$candidate\"; meta=\$(apk adbdump \"\$candidate\"); printf '%s\\n' \"\$meta\" | grep -Fx '  name: nlbwmon' >/dev/null; printf '%s\\n' \"\$meta\" | grep -Fx '  version: $STOCK_VERSION' >/dev/null; printf '%s\\n' \"\$meta\" | grep -Fx '  arch: $RELEASE_ARCH' >/dev/null; cp \"\$candidate\" '$REMOTE_APK'; test -s '$REMOTE_APK'" || fail 'Official package identity check failed. Service unchanged.'
fi

if ! remote "set -eu; /etc/init.d/nlbwmon stop; if ! apk add --allow-untrusted --force-reinstall '$REMOTE_APK'; then /etc/init.d/nlbwmon start || true; exit 1; fi; /etc/init.d/nlbwmon start; /etc/init.d/nlbwmon status; nlbw -c csv >/dev/null"; then
  printf '%s FAILED. Inspect device; no automatic rollback. Backup: %s\n' "$MODE" "$BACKUP_DIR" >&2
  exit 1
fi
remote 'sha256sum /etc/config/nlbwmon' > "$BACKUP_DIR/config-after.sha256" || fail 'Unable to verify configuration after install'
if ! cmp -s "$BACKUP_DIR/config-before.sha256" "$BACKUP_DIR/config-after.sha256"; then
  printf 'WARNING: Configuration changed! Backup: %s\n' "$BACKUP_DIR" >&2
  exit 1
fi
remote "rm -f '$REMOTE_APK'; rm -rf '/tmp/nlbwmon-stock-$$'" || printf 'WARNING: temporary package may remain on device\n' >&2
printf '\n%s OK. Configuration unchanged. Backup: %s\n' "${MODE^^}" "$BACKUP_DIR"
