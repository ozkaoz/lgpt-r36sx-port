#!/usr/bin/env bash
set -Eeuo pipefail

# Publish LGPT R36SX U2.52.4 to ozkaoz/lgpt-r36sx-port.
# R3 supports both a clean first run and resuming the exact partial tree left
# by the previous publishers. It still rejects unrelated working-tree changes,
# unknown remotes, missing tested binaries, mismatched checksums and failed tests.

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
REPO_ROOT="${REPO_ROOT:-$PROJECT_ROOT/GITHUB/lgpt-r36sx-port}"
SD_DRIVE="${SD_DRIVE:-F:}"
SD_MOUNT="${SD_MOUNT:-/mnt/f}"
VERSION="U2.52.4"
TAG="U2.52.4"
GITHUB_REPO="ozkaoz/lgpt-r36sx-port"
PROFILE="u2_38au8_sync_uac2"
KVER="4.4.186-release"
TARGET_REL="lgpt/otg/modules/$KVER/$PROFILE"
EXPECTED_VERMAGIC="4.4.186-release preempt MIPS32_R2 32BIT"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAYLOAD="$SCRIPT_DIR/payload"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_ROOT="$PROJECT_ROOT/LOGS"
BACKUP_ROOT="$PROJECT_ROOT/BACKUPS"
LOG="$LOG_ROOT/PUBLISH_U2524_${TIMESTAMP}.log"
MOUNTED_BY_SCRIPT=0
TMP_ROOT=""

mkdir -p "$LOG_ROOT" "$BACKUP_ROOT"
exec > >(tee -a "$LOG") 2>&1

declare -A EXPECTED_SHA256=(
  [soundcore.ko]="5cd5d4dbdb0ce7379c64611d035bf3643d9f6d3097c046bb49214b2f065d5f39"
  [snd.ko]="91742747579d9a6e8ca0fff0e920eed69afdd9f2fcab57029e351ea13c5f95bb"
  [snd-timer.ko]="25cb2142f7bea8f92edfc03d28c3a1e82cc656a1aa09ec5a5e1a99be41c87920"
  [snd-pcm.ko]="48f7d39e97aafb6f61c2ff2f8c9a2a101115875924e22d986f8fc8485f4a2704"
  [usb_f_uac2.ko]="e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe"
)
MODULES=(soundcore.ko snd.ko snd-timer.ko snd-pcm.ko usb_f_uac2.ko)

cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  set +e
  sync
  if (( MOUNTED_BY_SCRIPT == 1 )) && mountpoint -q "$SD_MOUNT"; then
    cd /
    sudo umount "$SD_MOUNT"
    echo "SD_UNMOUNT_ON_EXIT=$SD_MOUNT"
  fi
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

latest_path() {
  local root="$1" type="$2" pattern="$3"
  local candidate latest=""
  while IFS= read -r -d '' candidate; do
    if [[ -z "$latest" || "$candidate" -nt "$latest" ]]; then
      latest="$candidate"
    fi
  done < <(find "$root" -maxdepth 1 -mindepth 1 -type "$type" -name "$pattern" -print0 2>/dev/null)
  printf '%s\n' "$latest"
}

mount_sd() {
  sudo mkdir -p "$SD_MOUNT"
  if mountpoint -q "$SD_MOUNT"; then
    echo "SD_ALREADY_MOUNTED=$SD_MOUNT"
  else
    sudo mount -t drvfs "$SD_DRIVE" "$SD_MOUNT"
    MOUNTED_BY_SCRIPT=1
    echo "SD_MOUNTED=$SD_DRIVE->$SD_MOUNT"
  fi
  mountpoint -q "$SD_MOUNT" || fail "cannot mount $SD_DRIVE"
  [[ -d "$SD_MOUNT/lgpt" && -d "$SD_MOUNT/cubegm" ]] ||
    fail "$SD_DRIVE is not the tested LGPT SD"
}

unmount_sd_now() {
  sync
  if (( MOUNTED_BY_SCRIPT == 1 )); then
    cd /
    sudo umount "$SD_MOUNT"
    MOUNTED_BY_SCRIPT=0
    echo "SD_UNMOUNT_OK=$SD_MOUNT"
  fi
}

verify_module() {
  local path="$1" name actual vm
  name="$(basename "$path")"
  [[ -s "$path" ]] || fail "missing module: $path"
  actual="$(sha256sum "$path" | awk '{print $1}')"
  echo "MODULE_SHA256[$name]=$actual"
  [[ "$actual" == "${EXPECTED_SHA256[$name]}" ]] || fail "checksum mismatch: $name"
  file "$path" | grep -qE 'ELF 32-bit LSB relocatable, MIPS' || fail "wrong ELF architecture: $name"
  vm="$(modinfo -F vermagic "$path" 2>/dev/null | sed 's/[[:space:]]*$//')"
  echo "MODULE_VERMAGIC[$name]=$vm"
  [[ "$vm" == "$EXPECTED_VERMAGIC" ]] || fail "vermagic mismatch: $name"
}

find_r5_modules() {
  local build_root="$PROJECT_ROOT/BUILD"
  local candidate zip extract source

  while IFS= read -r -d '' candidate; do
    if [[ -d "$candidate/$TARGET_REL" ]]; then
      printf '%s\n' "$candidate/$TARGET_REL"
      return 0
    fi
  done < <(find "$build_root" -maxdepth 1 -mindepth 1 -type d -name 'UAC2_STAGE2_ALSA_R5_*' -print0 2>/dev/null)

  zip="$(latest_path "$build_root" f 'LGPT_R36SX_UAC2_ALSA_STAGE2_R5_*_FULL_SOURCE.zip')"
  [[ -n "$zip" ]] || fail "R5 full-source ZIP not found under $build_root"

  extract="$TMP_ROOT/r5"
  mkdir -p "$extract"
  unzip -q "$zip" -d "$extract"
  source="$(find "$extract" -type d -path "*/$TARGET_REL" -print -quit 2>/dev/null)"
  [[ -n "$source" ]] || fail "R5 ZIP does not contain $TARGET_REL"
  echo "R5_MODULE_SOURCE=$source" >&2
  printf '%s\n' "$source"
}

copy_tested_file() {
  local rel="$1"
  local target="$REPO_ROOT/sd_root/$rel"
  local candidate=""

  if [[ -s "$target" ]]; then
    echo "PAYLOAD_FILE_PRESENT=$rel"
    return 0
  fi

  while IFS= read -r -d '' candidate; do
    [[ "$candidate" == "$target" ]] && continue
    if [[ -s "$candidate" ]]; then
      mkdir -p "$(dirname "$target")"
      cp -f "$candidate" "$target"
      echo "PAYLOAD_FILE_RECOVERED=$rel source=$candidate"
      return 0
    fi
  done < <(find "$PROJECT_ROOT" -type f -path "*/$rel" -print0 2>/dev/null)

  mount_sd
  [[ -s "$SD_MOUNT/$rel" ]] || fail "tested SD also lacks $rel"
  mkdir -p "$(dirname "$target")"
  cp -f "$SD_MOUNT/$rel" "$target"
  echo "PAYLOAD_FILE_COPIED_FROM_SD=$rel"
}


path_is_expected_release_change() {
  local path="$1"
  case "$path" in
    VERSION|\
    deployment/config.stock.xml|\
    deployment/start.lgpt|\
    device/lgpt_launcher_u241.sh|\
    docs/BUILD_ALSA_UAC2_ES.md|\
    docs/COPY_ROOT_INSTALL_ES.md|\
    docs/RELEASE_U2.52.4_ES.md|\
    recovery/u2_38au8_sync_uac2/*|\
    scripts/build_copy_root_release.py|\
    scripts/build_copy_root_release.sh|\
    scripts/build_from_full_clone.sh|\
    scripts/publish_u2524_to_github.sh|\
    scripts/publish_u2524_to_github_r2.sh|\
    scripts/publish_u2524_to_github_r3.sh|\
    scripts/refresh_manifest.sh|\
    scripts/uac2/*|\
    scripts/verify_copy_root_layout.sh|\
    sd_root/*|\
    tests/test_copy_root_launcher.sh|\
    validation/U2.52.4/*)
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

    # Rename records are not expected in this release.
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
    fail "repository contains changes unrelated to U2.52.4; they were not modified"

  git -C "$REPO_ROOT" diff --cached --quiet ||
    fail "staged changes exist; unstage them before resuming"
}

backup_partial_worktree() {
  local prefix="$BACKUP_ROOT/lgpt-r36sx-port_partial_${TAG}_${TIMESTAMP}"
  local untracked_list="$TMP_ROOT/untracked-files.zlist"

  git -C "$REPO_ROOT" diff --binary >"${prefix}.patch"
  git -C "$REPO_ROOT" ls-files --others --exclude-standard -z >"$untracked_list"

  if [[ -s "$untracked_list" ]]; then
    tar -C "$REPO_ROOT" --null -czf "${prefix}_untracked.tar.gz" -T "$untracked_list"
    echo "PARTIAL_UNTRACKED_BACKUP=${prefix}_untracked.tar.gz"
  fi

  echo "PARTIAL_PATCH_BACKUP=${prefix}.patch"
}

preflight() {
  local tool origin login
  for tool in git gh rsync unzip zip python3 sha256sum file modinfo find tar mountpoint findmnt; do
    need_tool "$tool"
  done

  [[ -d "$PAYLOAD/copyroot_overlay" ]] || fail "publisher payload missing"
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

  local head remote_head
  head="$(git -C "$REPO_ROOT" rev-parse HEAD)"
  remote_head="$(git -C "$REPO_ROOT" rev-parse origin/main)"
  echo "LOCAL_HEAD=$head"
  echo "ORIGIN_MAIN=$remote_head"
  [[ "$head" == "$remote_head" ]] ||
    fail "local main is not exactly origin/main; do not merge while the release tree is dirty"

  verify_resume_worktree

  ! git -C "$REPO_ROOT" rev-parse -q --verify "refs/tags/$TAG" >/dev/null || fail "local tag already exists: $TAG"
  [[ -z "$(git -C "$REPO_ROOT" ls-remote --tags origin "refs/tags/$TAG")" ]] || fail "remote tag already exists: $TAG"
  ! gh release view "$TAG" --repo "$GITHUB_REPO" >/dev/null 2>&1 || fail "GitHub release already exists: $TAG"

  mkdir -p "$BACKUP_ROOT"
  git -C "$REPO_ROOT" bundle create "$BACKUP_ROOT/lgpt-r36sx-port_before_${TAG}_${TIMESTAMP}.bundle" --all
  echo "GIT_BACKUP_BUNDLE=$BACKUP_ROOT/lgpt-r36sx-port_before_${TAG}_${TIMESTAMP}.bundle"
  backup_partial_worktree
}

apply_sources() {
  local modules_src target m
  echo "=== APPLY COPY-ROOT OVERLAY ==="
  rsync -a "$PAYLOAD/copyroot_overlay/" "$REPO_ROOT/"

  mkdir -p "$REPO_ROOT/scripts/uac2" "$REPO_ROOT/validation/$VERSION" "$REPO_ROOT/docs"
  rsync -a "$PAYLOAD/uac2_scripts/" "$REPO_ROOT/scripts/uac2/"
  rsync -a "$PAYLOAD/validation/" "$REPO_ROOT/validation/$VERSION/"
  rsync -a "$PAYLOAD/docs/" "$REPO_ROOT/docs/"
  cp -f "$SCRIPT_DIR/$(basename "$0")" "$REPO_ROOT/scripts/publish_u2524_to_github.sh"
  cp -f "$SCRIPT_DIR/$(basename "$0")" "$REPO_ROOT/scripts/publish_u2524_to_github_r3.sh"

  printf '%s\n' "$VERSION" > "$REPO_ROOT/VERSION"
  mkdir -p "$REPO_ROOT/sd_root/LGPT_OTG_LOGS"
  : > "$REPO_ROOT/sd_root/LGPT_OTG_LOGS/.keep"
  cp -f "$PAYLOAD/docs/RELEASE_U2.52.4_ES.md" "$REPO_ROOT/sd_root/README_PRIMERO_ES.md"
  printf '%s\n' "$VERSION" > "$REPO_ROOT/sd_root/VERSION.txt"

  modules_src="$(find_r5_modules)"
  target="$REPO_ROOT/sd_root/$TARGET_REL"
  mkdir -p "$target" "$REPO_ROOT/recovery/$PROFILE"

  for m in "${MODULES[@]}"; do
    verify_module "$modules_src/$m"
    cp -f "$modules_src/$m" "$target/$m"
    cp -f "$modules_src/$m" "$REPO_ROOT/recovery/$PROFILE/$m"
  done

  copy_tested_file "cubegm/cores/lgpt_r36sx_port_libretro.so"
  copy_tested_file "lgpt/otg/bin/r36s_u241_usb_audio_io"

  [[ -s "$REPO_ROOT/device/otg_u241_setup_once.sh" ]] ||
    fail "source missing device/otg_u241_setup_once.sh"
  mkdir -p "$REPO_ROOT/sd_root/lgpt/otg/bin"
  cp -f "$REPO_ROOT/device/otg_u241_setup_once.sh"     "$REPO_ROOT/sd_root/lgpt/otg/bin/otg_u241_setup_once.sh"
  echo "PAYLOAD_FILE_MAPPED=device/otg_u241_setup_once.sh->sd_root/lgpt/otg/bin/otg_u241_setup_once.sh"

  chmod +x     "$REPO_ROOT/device/lgpt_launcher_u241.sh"     "$REPO_ROOT/device/otg_u241_setup_once.sh"     "$REPO_ROOT/sd_root/cubegm/lgpt"     "$REPO_ROOT/sd_root/lgpt/otg/bin/otg_u241_setup_once.sh"     "$REPO_ROOT/sd_root/lgpt/otg/bin/r36s_u241_usb_audio_io"
  find "$REPO_ROOT/scripts" "$REPO_ROOT/tests" -type f -name '*.sh' -exec chmod +x {} +
  chmod +x "$REPO_ROOT/scripts/publish_u2524_to_github.sh"     "$REPO_ROOT/scripts/publish_u2524_to_github_r3.sh"

  unmount_sd_now
}

validate_tree() {
  local m
  echo "=== VALIDATE TREE ==="
  bash "$REPO_ROOT/scripts/verify_copy_root_layout.sh" "$REPO_ROOT/sd_root"
  bash "$REPO_ROOT/tests/test_copy_root_launcher.sh"

  for m in "${MODULES[@]}"; do
    verify_module "$REPO_ROOT/sd_root/$TARGET_REL/$m"
    verify_module "$REPO_ROOT/recovery/$PROFILE/$m"
  done

  grep -F 'LGPT R36SX U2.52.4 copy-root + ALSA/UAC2 validated' "$REPO_ROOT/sd_root/cubegm/lgpt" >/dev/null ||
    fail "U2.52.4 launcher marker missing"
  grep -F '<SAMPLELIB value="/mnt/sdcard/lgpt/samples" />' "$REPO_ROOT/sd_root/lgpt/config.xml" >/dev/null ||
    fail "sample library path is invalid"

  [[ -x "$REPO_ROOT/sd_root/lgpt/otg/bin/otg_u241_setup_once.sh" ]] ||
    fail "copy-root OTG setup is missing or not executable"
  [[ -x "$REPO_ROOT/sd_root/lgpt/otg/bin/r36s_u241_usb_audio_io" ]] ||
    fail "copy-root OTG daemon is missing or not executable"

  cmp -s     "$REPO_ROOT/device/otg_u241_setup_once.sh"     "$REPO_ROOT/sd_root/lgpt/otg/bin/otg_u241_setup_once.sh" ||
    fail "copy-root OTG setup differs from device source"

  git -C "$REPO_ROOT" diff --check
}

commit_release() {
  echo "=== COMMIT RELEASE ==="
  git -C "$REPO_ROOT" add -A
  git -C "$REPO_ROOT" add -f \
    "sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so" \
    "sd_root/$TARGET_REL"/*.ko \
    "recovery/$PROFILE"/*.ko

  git -C "$REPO_ROOT" diff --cached --check
  git -C "$REPO_ROOT" diff --cached --stat
  [[ -n "$(git -C "$REPO_ROOT" diff --cached --name-only)" ]] || fail "nothing to commit"

  git -C "$REPO_ROOT" commit -m "Release U2.52.4: copy-root sampler fix and validated ALSA/UAC2"
  git -C "$REPO_ROOT" tag -a "$TAG" -m "LGPT R36SX U2.52.4 copy-root + validated ALSA/UAC2"
  echo "RELEASE_COMMIT=$(git -C "$REPO_ROOT" rev-parse HEAD)"
}

build_asset() {
  local asset sha_file
  echo "=== BUILD FULL-SOURCE COPY-ROOT ASSET ==="
  asset="$REPO_ROOT/dist/LGPT_R36SX_U2524_COPYROOT_UAC2_FULL_SOURCE.zip"
  rm -f "$asset" "$asset.sha256"
  bash "$REPO_ROOT/scripts/build_from_full_clone.sh" "$asset"
  sha256sum "$asset" > "$asset.sha256"
  unzip -t "$asset" >/dev/null
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
    "$asset#Copy-root full source ZIP" \
    "$asset.sha256#SHA-256 checksum" \
    --repo "$GITHUB_REPO" \
    --title "LGPT R36SX U2.52.4 — Copy-root + ALSA/UAC2" \
    --notes-file "$REPO_ROOT/docs/RELEASE_U2.52.4_ES.md" \
    --verify-tag \
    --latest

  url="$(gh release view "$TAG" --repo "$GITHUB_REPO" --json url --jq .url)"
  echo "GITHUB_RELEASE_URL=$url"
  gh release view "$TAG" --repo "$GITHUB_REPO" --json tagName,name,isDraft,isPrerelease,assets,url
}

main() {
  local asset
  TMP_ROOT="$(mktemp -d "$HOME/lgpt-r36sx-publish-u2524-r3.XXXXXX")"

  echo "============================================================"
  echo "PUBLISH LGPT R36SX $VERSION — RESUMABLE R3"
  echo "============================================================"
  echo "DATE=$(date -Iseconds)"
  echo "PROJECT_ROOT=$PROJECT_ROOT"
  echo "REPO_ROOT=$REPO_ROOT"
  echo "TAG=$TAG"

  preflight
  apply_sources
  validate_tree
  commit_release
  asset="$(build_asset | tee /dev/stderr | tail -n1)"
  [[ -s "$asset" ]] || fail "release asset was not generated"
  publish "$asset"

  echo "PUBLISH_RESULT=GITHUB_RELEASE_CREATED_R3"
}

main "$@"
