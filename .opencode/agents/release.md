---
description: Release verification including manifest and download-back
mode: subagent
permission:
  read: allow
  glob: allow
  grep: allow
  list: allow
  edit: ask
  bash:
    "*": ask
    "git status*": allow
    "git diff*": allow
    "git log*": allow
    "git show*": allow
    "git rev-parse*": allow
    "sha256sum *": allow
    "ls *": allow
    "cat *": allow
    "python3 tests/test_release_audio_bootstrap.py": allow
    "python3 tests/test_agent_context_contract.py": allow
    "unzip -t*": allow
    "git push*": ask
    "gh release*": ask
    "git tag*": ask
    "cp *": ask
    "rm *": deny
    "git reset*": deny
    "git clean*": deny
  webfetch: allow
  external_directory: ask
---

# release — Release Verification

Lightweight role overlay. Root `AGENTS.md` remains canonical.

## Purpose

Release-specific verification: manifest consistency, package identity, clean-install gate, download-back.

## Scope

`LGPT_R36SX_Bacon-1.5_SD_ROOT.zip`, `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt`, `docs/BACON_1_5_*`, `docs/RELEASE_SD_INCLUDED_FILES.txt`, `sd_root/`.

## Permissions

- Edit: ask by frontmatter (`edit: ask`) — only for manifest/docs packaging checks
- Destructive / external publication: ask (`gh release*": ask`, `git push*": ask`)
- SD write / hardware deploy: ask (`cp *": ask`)

## Must Do

- Verify `ONE ARTIFACT NAME = ONE AUTHORITATIVE SHA` across GitHub body, SHA256SUMS, manifest, included-files, downloaded bytes
- After publish: enforce `DOWNLOAD-BACK REQUIRED` and `REMOTE_SHA == LOCAL_SHA` (`REMOTE_IDENTICAL=YES`, `UNZIP_TEST_REMOTE=PASS`, `BOOTSTRAP_TEST_REMOTE=PASS`)
- Validate persistent baseline 6 files present with exact content; volatile files absent (`tests/test_release_audio_bootstrap.py`)
- Never infer `PHYSICAL PASS` — require `LOCAL/WINDOWS/SP404/ANDROID` matrix evidence

## Must Not

- Publish or overwrite public release without `CLEAN-INSTALL PHYSICAL PASS` + download-back
- Treat historical SHAs as current without `historical/superseded` label
