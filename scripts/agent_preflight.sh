#!/usr/bin/env bash
set -Eeuo pipefail

# agent_preflight.sh — non-destructive preflight for AI agents
# Default: repo diagnostics + context-contract test
# Optional: --sd <mount>  (read-only mount/fs inspection)
#           --sd-write-probe (requires --sd, creates unique temp file then removes it)
#           --allow-dirty (allow dirty worktree to continue)
# Never writes to SD by default, never runs fs repair automatically.

ROOT=""
BRANCH=""
HEAD=""
UPSTREAM=""
AHEAD_BEHIND=""
SD_PATH=""
WRITE_PROBE=0
ALLOW_DIRTY=0

usage() {
  echo "Usage: $0 [--sd <mount>] [--sd-write-probe] [--allow-dirty] [--help]"
  echo "  --sd <mount>         optional SD mount to inspect (e.g. /mnt/g, /mnt/f, G:\)"
  echo "  --sd-write-probe     with --sd, perform one unique temp-file write then remove"
  echo "  --allow-dirty        allow dirty worktree to continue (else dirty → PREFLIGHT_RESULT=FAIL)"
  echo "  --help               show this help"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sd)
      SD_PATH="${2:-}"; shift 2
      ;;
    --sd-write-probe)
      WRITE_PROBE=1; shift
      ;;
    --allow-dirty)
      ALLOW_DIRTY=1; shift
      ;;
    --help|-h)
      usage; exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2; usage; exit 1
      ;;
  esac
done

if ! ROOT="$(git rev-parse --show-toplevel 2>/dev/null)"; then
  echo "ERROR: not inside a git repository" >&2
  exit 1
fi

# Resolve from Git directly — never from docs
BRANCH="$(git -C "$ROOT" branch --show-current 2>/dev/null || echo "?")"
HEAD="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo "?")"
UPSTREAM="$(git -C "$ROOT" rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>/dev/null || echo "no upstream")"
if UPSTREAM_HEAD="$(git -C "$ROOT" rev-parse '@{u}' 2>/dev/null)"; then
  AHEAD_BEHIND="$(git -C "$ROOT" rev-list --left-right --count HEAD...@'{u}' 2>/dev/null | awk '{print "ahead "$1" behind "$2}') "
  REMOTE_HEAD="$UPSTREAM_HEAD"
else
  AHEAD_BEHIND="no upstream count"
  REMOTE_HEAD="no upstream"
fi

echo "REPO_ROOT=$ROOT"
echo "BRANCH=$BRANCH"
echo "HEAD=$HEAD"
echo "UPSTREAM=$UPSTREAM"
echo "REMOTE_HEAD=$REMOTE_HEAD"
echo "AHEAD_BEHIND=$AHEAD_BEHIND"

echo "--- git status ---"
git -C "$ROOT" status --short --branch || true

# Dirty worktree policy (AGENTS.md: unexplained local modifications require STOP)
# Default: dirty → nonzero. --allow-dirty continues but reports WORKTREE_DIRTY=YES
DIRTY_COUNT="$(git -C "$ROOT" status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
if [[ "$DIRTY_COUNT" != "0" ]]; then
  echo "WORKTREE_DIRTY=YES (modified/untracked files: $DIRTY_COUNT)"
  git -C "$ROOT" status --short 2>/dev/null || true
  git -C "$ROOT" ls-files --others --exclude-standard 2>/dev/null | head -n 20 || true
  if [[ "$ALLOW_DIRTY" -eq 0 ]]; then
    echo "PREFLIGHT_RESULT=FAIL"
    echo "PREFLIGHT_REASON=DIRTY_WORKTREE"
    echo "HINT: use --allow-dirty to inspect anyway (does not auto-clean)"
    exit 1
  else
    echo "DIRTY_ALLOWED=YES"
  fi
else
  echo "WORKTREE_DIRTY=NO"
fi

echo "--- recent commits ---"
git -C "$ROOT" log --oneline --decorate -10 || true

echo "--- worktrees ---"
git -C "$ROOT" worktree list || true

echo "--- stash count ---"
STASH_COUNT="$(git -C "$ROOT" stash list | wc -l | tr -d ' ')"
echo "STASH_COUNT=$STASH_COUNT"
if [[ "$STASH_COUNT" != "0" ]]; then
  git -C "$ROOT" stash list || true
fi

echo "--- context contract test ---"
CONTRACT_EXIT=0
if [[ -f "$ROOT/tests/test_agent_context_contract.py" ]]; then
  if command -v python3 >/dev/null 2>&1; then
    if ! python3 "$ROOT/tests/test_agent_context_contract.py"; then
      CONTRACT_EXIT=1
    fi
  elif command -v python >/dev/null 2>&1; then
    if ! python "$ROOT/tests/test_agent_context_contract.py"; then
      CONTRACT_EXIT=1
    fi
  else
    echo "ERROR: python not found but context-contract test is required" >&2
    echo "PREFLIGHT_RESULT=FAIL"
    echo "PREFLIGHT_REASON=PYTHON_MISSING"
    exit 1
  fi
  if [[ "$CONTRACT_EXIT" -ne 0 ]]; then
    echo "PREFLIGHT_RESULT=FAIL"
    echo "PREFLIGHT_REASON=CONTEXT_CONTRACT"
    exit 1
  fi
else
  echo "WARN: tests/test_agent_context_contract.py not found" >&2
  echo "PREFLIGHT_RESULT=FAIL"
  echo "PREFLIGHT_REASON=CONTRACT_MISSING"
  exit 1
fi

# Optional SD inspection — only when explicitly requested
if [[ -n "$SD_PATH" ]]; then
  echo "--- SD inspection: $SD_PATH ---"
  if [[ ! -e "$SD_PATH" ]]; then
    echo "SD_PATH not found: $SD_PATH"
  else
    # mount / filesystem / options / ro check — read-only probes only
    if command -v mountpoint >/dev/null 2>&1; then
      mountpoint "$SD_PATH" 2>&1 || echo "mountpoint check: not a mountpoint or not available"
    fi
    if command -v df >/dev/null 2>&1; then
      df -h "$SD_PATH" 2>&1 || true
    fi
    if command -v mount >/dev/null 2>&1; then
      if mount | grep -F -- "$SD_PATH" >/dev/null 2>&1; then
        mount | grep -F -- "$SD_PATH" || true
      else
        echo "mount: no entry exactly matching $SD_PATH (checking parent)"
        mount | grep -i "sd" || true
      fi
    fi
    # read-only vs writable indication via mount options and test
    if [[ -r "$SD_PATH" ]]; then echo "SD readable: YES"; else echo "SD readable: NO"; fi
    # Token-aware ro detection (not substring)
    if command -v findmnt >/dev/null 2>&1; then
      if findmnt -n -o OPTIONS --target "$SD_PATH" 2>/dev/null | tr ',' '\n' | grep -qx "ro"; then
        echo "mount options: read-only (ro)"
      else
        echo "mount options: writable (rw or not ro)"
      fi
    fi
    ls -ld "$SD_PATH" 2>&1 || true

    if [[ "$WRITE_PROBE" -eq 1 ]]; then
      echo "--- SD write probe (explicit) ---"
      PROBE_FILE="$SD_PATH/.lgpt_agent_write_probe_$$_$(date +%s).tmp"
      # Use unique name, write, sync, verify, remove
      if echo "probe" > "$PROBE_FILE" 2>&1; then
        sync 2>/dev/null || true
        if [[ -f "$PROBE_FILE" ]]; then
          echo "write probe: PASS (created $PROBE_FILE)"
          rm -f "$PROBE_FILE" 2>/dev/null || true
          echo "write probe: cleaned up"
        else
          echo "write probe: FAIL (file not visible after write)"
        fi
      else
        echo "write probe: FAIL (cannot write to $SD_PATH — read-only or permission?)"
        echo "HINT: filesystem may be dirty/exFAT read-only — do NOT auto-repair; ask user"
      fi
    else
      echo "SD write probe: SKIPPED (use --sd-write-probe to enable one temp-file probe)"
    fi
    echo "NOTE: filesystem repair never runs automatically — requires explicit user authorization"
  fi
else
  echo "--- SD inspection: SKIPPED (use --sd <mount> to enable read-only inspection) ---"
fi

echo "PREFLIGHT_RESULT=PASS"
echo "PREFLIGHT_DONE"
