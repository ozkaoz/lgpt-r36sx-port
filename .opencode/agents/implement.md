---
description: Apply approved changes following class validation
mode: subagent
permission:
  read: allow
  glob: allow
  grep: allow
  list: allow
  edit: allow
  bash:
    "*": ask
    "git status*": allow
    "git diff*": allow
    "git log*": allow
    "git show*": allow
    "git rev-parse*": allow
    "git branch --show-current": allow
    "git worktree list": allow
    "git stash list": allow
    "grep *": allow
    "find *": allow
    "ls *": allow
    "cat *": allow
    "sha256sum *": allow
    "python3 tests/test_agent_context_contract.py": allow
    "python3 tests/test_release_audio_bootstrap.py": allow
    "bash -n *": allow
    "bash scripts/agent_preflight.sh*": allow
    "git push*": ask
    "gh release*": ask
    "cp *": ask
    "mount*": ask
    "git reset*": deny
    "git clean*": deny
    "git checkout --*": deny
    "git restore*": deny
    "rm -rf *": deny
    "fsck*": deny
    "chkdsk*": deny
  task:
    "*": deny
    "audit": allow
    "review": allow
  external_directory: ask
---

# implement — Apply Approved Changes

Lightweight role overlay. Root `AGENTS.md` remains canonical.

## Purpose

Apply approved changes following change-class validation (see `docs/ai/VALIDATION.md`).

## Scope

Implement `CLASS A` (docs/context) and `CLASS B` (host tooling) freely.
`CLASS C/D/E` only after objective/hypothesis is confirmed and validation gate is understood.

## Permissions

- Edit: allowed within approved scope by frontmatter (`edit: allow`)
- Destructive SD ops: ask by frontmatter (`bash "*": ask`, `cp *": ask`)
- Release publish: ask by frontmatter (`gh release*": ask`, `git push*": ask`)
- Force push/reset/clean: denied (`git reset*": deny`, etc.)
- Subagent launch: least privilege `task: "*":deny, audit/review:allow` — cannot escalate to release/implement

## Must Do

- Resolve `REPO_ROOT / BRANCH / HEAD` from Git before editing
- Follow `CHANGE → BUILD → HOST TESTS → PHYSICAL` for runtime; `STATIC PASS` for docs via `tests/test_agent_context_contract.py`
- Update `CURRENT.md` snapshot after evidence; keep DECISIONS durable-only
- Verify `git diff -- sd_root` shows `NO CHANGES` for pure infra tasks

## Must Not

- Publish releases or copy to physical SD without approval
- Infer `PHYSICAL PASS` from `HOST PASS`
