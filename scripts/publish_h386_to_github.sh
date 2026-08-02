#!/usr/bin/env bash
set -Eeuo pipefail

# Publish LGPT R36SX H38.6 (linear phrase volume, live mixer VU, startup-menu
# project rename, unified Record-style text editor) to ozkaoz/lgpt-r36sx-port.
# The release ZIP is copy-to-SD-root (cubegm/, lgpt/, roms/, LGPT_OTG_LOGS/, ANDROID/)
# and contains the Android APK plus the full repository source snapshot.

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
REPO_ROOT="${REPO_ROOT:-$PROJECT_ROOT/GITHUB/lgpt-r36sx-port}"
VERSION="H38.6"
TAG="H38.6"
GITHUB_REPO="ozkaoz/lgpt-r36sx-port"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAYLOAD="$SCRIPT_DIR/payload"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_ROOT="$PROJECT_ROOT/LOGS"
BACKUP_ROOT="$PROJECT_ROOT/BACKUPS"
LOG="$LOG_ROOT/PUBLISH_H386_${TIMESTAMP}.log"
TMP_ROOT=""

mkdir -p "$LOG_ROOT" "$BACKUP_ROOT"
exec > >(tee -a "$LOG") 2>&1

CORE_SHA256="06ae7a356a037c88ec7b8bf25a15f4bc1dc9d806aaa01428cc044581aa9c9b2f"
DAEMON_SHA256="53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815"
APK_SHA256="fd9787727ed683236e12d50c180a9c999db362785bd53758754d8c74e697cd06"

SD_BIN_REQUIRED=(
  r36s_u241_usb_audio_io
  otg_u241_common.sh
  otg_u241_setup_once.sh
  otg_u241_shutdown.sh
  otg_u241_apply_profile_once.sh
  otg_h37_apply_driver_mode.sh
  otg_h37_android_runtime_supervisor.sh
  r36s_aoa_bulk_audio_io_h36
  r36s_aoa_bulk_receiver_h36
)

cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  set +e
  sync
  [[ -z "$TMP_ROOT" ]] || rm -rf "$TMP_ROOT"
  echo "SCRIPT_EXIT_CODE=$rc"
  echo "PUBLISH_LOG=$LOG"
  return "$rc"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

fail() {
  echo "ERROR=$*" >&2
  return 1
}

need_tool() {
  command -v "$1" >/dev/null 2>&1 || fail "required tool missing: $1"
}

path_is_expected_release_change() {
  local path="$1"
  case "$path" in
    VERSION|\
    README.md|\
    CHANGELOG.md|\
    .gitignore|\
    ANDROID/*|\
    deployment/*|\
    device/*|\
    docs/RELEASE_H38.6_ES.md|\
    scripts/*|\
    source/*|\
    sd_root/*|\
    tests/*|\
    validation/*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

verify_resume_worktree() {
  local line status path unexpected=0
  local porcelain
  porcelain="$(git -C "$REPO_ROOT" status --porcelain=v1 --untracked-files=all)"

  if [[ -z "$porcelain" ]]; then
    echo "WORKTREE_MODE=CLEAN_FIRST_RUN"
    return 0
  fi

  echo "WORKTREE_MODE=RESUME_EXPECTED_PARTIAL_RELEASE"
  while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    status="${line:0:2}"
    path="${line:3}"

    if [[ "$path" == *" -> "* ]]; then
      echo "UNEXPECTED_RENAME=$line"
      unexpected=1
      continue
    fi

    if path_is_expected_release_change "$path"; then
      echo "EXPECTED_DIRTY_PATH status=$status path=$path"
    else
      echo "UNEXPECTED_DIRTY_PATH status=$status path=$path"
      unexpected=1
    fi
  done <<<"$porcelain"

  (( unexpected == 0 )) ||
    fail "repository contains changes unrelated to H38.6; they were not modified"

  git -C "$REPO_ROOT" diff --cached --quiet ||
    fail "staged changes exist; unstage them before resuming"
}

preflight() {
  local tool origin login
  for tool in git gh rsync unzip zip python3 sha256sum file find tar; do
    need_tool "$tool"
  done

  [[ -d "$REPO_ROOT/.git" ]] || fail "Git clone missing: $REPO_ROOT"

  gh auth status --hostname github.com >/dev/null ||
    fail "GitHub CLI is not authenticated. Run: gh auth login --web --git-protocol https"
  gh auth setup-git >/dev/null
  login="$(gh api user --jq .login)"
  echo "GITHUB_LOGIN=$login"

  origin="$(git -C "$REPO_ROOT" remote get-url origin)"
  echo "ORIGIN=$origin"
  [[ "$origin" =~ github\.com[:/]ozkaoz/lgpt-r36sx-port(\.git)?$ ]] ||
    fail "unexpected origin remote: $origin"

  [[ -n "$(git -C "$REPO_ROOT" config user.name || true)" ]] || fail "git user.name is not configured"
  [[ -n "$(git -C "$REPO_ROOT" config user.email || true)" ]] || fail "git user.email is not configured"

  git -C "$REPO_ROOT" fetch origin --tags
  [[ "$(git -C "$REPO_ROOT" branch --show-current)" == "main" ]] ||
    fail "expected current branch main"

  verify_resume_worktree

  ! git -C "$REPO_ROOT" rev-parse -q --verify "refs/tags/$TAG" >/dev/null || fail "local tag already exists: $TAG"
  [[ -z "$(git -C "$REPO_ROOT" ls-remote --tags origin "refs/tags/$TAG")" ]] || fail "remote tag already exists: $TAG"
  ! gh release view "$TAG" --repo "$GITHUB_REPO" >/dev/null 2>&1 || fail "GitHub release already exists: $TAG"

  mkdir -p "$BACKUP_ROOT"
  git -C "$REPO_ROOT" bundle create "$BACKUP_ROOT/lgpt-r36sx-port_before_${TAG}_${TIMESTAMP}.bundle" --all
  echo "GIT_BACKUP_BUNDLE=$BACKUP_ROOT/lgpt-r36sx-port_before_${TAG}_${TIMESTAMP}.bundle"
}

sha_of() {
  sha256sum "$1" | awk '{print $1}'
}

validate_tree() {
  local sd_root="$REPO_ROOT/sd_root" bin rel actual
  echo "=== VALIDATE SD PAYLOAD ==="

  rel="cubegm/cores/lgpt_r36sx_port_libretro.so"
  actual="$(sha_of "$sd_root/$rel")"
  echo "CORE_SHA256[$rel]=$actual"
  [[ "$actual" == "$CORE_SHA256" ]] || fail "core checksum mismatch: $rel"

  rel="lgpt/otg/bin/r36s_u241_usb_audio_io"
  actual="$(sha_of "$sd_root/$rel")"
  echo "DAEMON_SHA256[$rel]=$actual"
  [[ "$actual" == "$DAEMON_SHA256" ]] || fail "daemon checksum mismatch: $rel"

  rel="ANDROID/LGPTUsbAudioBridge-H36-debug.apk"
  actual="$(sha_of "$sd_root/$rel")"
  echo "APK_SHA256[$rel]=$actual"
  [[ "$actual" == "$APK_SHA256" ]] || fail "APK checksum mismatch: $rel"

  for bin in "${SD_BIN_REQUIRED[@]}"; do
    rel="lgpt/otg/bin/$bin"
    [[ -s "$sd_root/$rel" ]] || fail "missing SD binary: $rel"
    [[ -x "$sd_root/$rel" ]] || fail "SD binary not executable: $rel"
    echo "SD_BIN_OK $rel"
  done

  [[ -s "$sd_root/lgpt/otg/H38_6_ABI7_THREE_MODE.txt" ]] || fail "H38.6 marker missing in SD payload"
  grep -F 'VERSION=H38.6_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND' "$sd_root/lgpt/otg/H38_6_ABI7_THREE_MODE.txt" >/dev/null ||
    fail "H38.6 marker content invalid"

  grep -F 'H38.6_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND' \
    "$REPO_ROOT/VERSION" "$REPO_ROOT/sd_root/VERSION.txt" >/dev/null ||
    fail "VERSION files not updated"

  [[ -s "$sd_root/lgpt/config.xml" ]] || fail "config.xml missing in SD payload"
  grep -F 'BACKGROUND value="0A0A18"' "$sd_root/lgpt/config.xml" >/dev/null ||
    fail "H38.6 palette missing in config.xml"

  echo "=== VALIDATE LAYOUT ==="
  bash "$REPO_ROOT/scripts/verify_copy_root_layout.sh" "$REPO_ROOT/sd_root"
  bash "$REPO_ROOT/tests/test_copy_root_launcher.sh"

  git -C "$REPO_ROOT" diff --check
}

commit_release() {
  echo "=== COMMIT RELEASE ==="
  git -C "$REPO_ROOT" add -A
  git -C "$REPO_ROOT" add -f \
    "sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so" \
    "sd_root/lgpt/otg/bin/r36s_u241_usb_audio_io" \
    "sd_root/lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36" \
    "sd_root/lgpt/otg/bin/r36s_aoa_bulk_receiver_h36" \
    "sd_root/ANDROID/LGPTUsbAudioBridge-H36-debug.apk"

  git -C "$REPO_ROOT" diff --cached --check
  git -C "$REPO_ROOT" diff --cached --stat
  [[ -n "$(git -C "$REPO_ROOT" diff --cached --name-only)" ]] || fail "nothing to commit"

  git -C "$REPO_ROOT" commit -m "Release H38.6: dynamic mixer VU bars, startup menu Rename/Export/Delete, phrase auto-volume, audio/GUI perf optimizations"
  git -C "$REPO_ROOT" tag -a "$TAG" -m "LGPT R36SX H38.6 fixes from hardware feedback + perf optimizations"
  echo "RELEASE_COMMIT=$(git -C "$REPO_ROOT" rev-parse HEAD)"
}

build_asset() {
  local asset sha_file
  echo "=== BUILD COPY-TO-SD-ROOT ASSET ==="
  asset="$REPO_ROOT/dist/LGPT_R36SX_H386_COPYROOT_3AUDIO_FULL_SOURCE.zip"
  rm -f "$asset" "$asset.sha256"
  bash "$REPO_ROOT/scripts/build_from_full_clone.sh" "$asset"
  sha256sum "$asset" > "$asset.sha256"
  unzip -t "$asset" >/dev/null
  if ! unzip -l "$asset" > "$TMP_ROOT/asset.list"; then
    fail "cannot list release ZIP"
  fi
  grep -q 'ANDROID/LGPTUsbAudioBridge-H36-debug.apk' "$TMP_ROOT/asset.list" ||
    fail "release ZIP does not contain the Android APK"
  grep -q 'cubegm/lgpt' "$TMP_ROOT/asset.list" ||
    fail "release ZIP is not copy-to-SD-root (cubegm/lgpt missing at root)"
  echo "RELEASE_ASSET=$asset"
  echo "RELEASE_ASSET_SHA256=$(cat "$asset.sha256")"
  printf '%s\n' "$asset"
}

publish() {
  local asset="$1" url
  echo "=== PUSH COMMIT AND TAG ==="
  git -C "$REPO_ROOT" push origin main
  git -C "$REPO_ROOT" push origin "$TAG"

  echo "=== CREATE GITHUB RELEASE ==="
  gh release create "$TAG" \
    "$asset#Copy-to-SD-root full ZIP (includes Android APK)" \
    "$asset.sha256#SHA-256 checksum" \
    --repo "$GITHUB_REPO" \
    --title "LGPT R36SX H38.6 — Mixer VU dinamico, menu de inicio Rename/Export/Delete, auto-volumen de phrase y optimizaciones de rendimiento" \
    --notes-file "$REPO_ROOT/docs/RELEASE_H38.6_ES.md" \
    --verify-tag \
    --latest

  url="$(gh release view "$TAG" --repo "$GITHUB_REPO" --json url --jq .url)"
  echo "GITHUB_RELEASE_URL=$url"
  gh release view "$TAG" --repo "$GITHUB_REPO" --json tagName,name,isDraft,isPrerelease,assets,url
}

main() {
  local asset
  TMP_ROOT="$(mktemp -d "$HOME/lgpt-r36sx-publish-h386.XXXXXX")"

  echo "============================================================"
  echo "PUBLISH LGPT R36SX $VERSION — THREE-MODE AUDIO"
  echo "============================================================"
  echo "DATE=$(date -Iseconds)"
  echo "PROJECT_ROOT=$PROJECT_ROOT"
  echo "REPO_ROOT=$REPO_ROOT"
  echo "TAG=$TAG"

  preflight
  validate_tree
  commit_release
  asset="$(build_asset | tee /dev/stderr | tail -n1)"
  [[ -s "$asset" ]] || fail "release asset was not generated"
  publish "$asset"

  echo "PUBLISH_RESULT=GITHUB_RELEASE_CREATED_H386"
}

main "$@"
