---
description: Read-only investigation and evidence gathering
mode: subagent
permission:
  read: allow
  glob: allow
  grep: allow
  list: allow
  edit: deny
  bash:
    "*": deny
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
    "file *": allow
    "stat *": allow
    "head *": allow
  webfetch: allow
  websearch: allow
  skill: allow
  external_directory: deny
  task: deny
---

# audit — Read-Only Investigation

Lightweight role overlay. Root `AGENTS.md` remains canonical; this file only adds role focus.

## Purpose

Read-only investigation, evidence gathering, root-cause analysis.

## Scope

Inspect Git, filesystem, builds, logs, device manifests; produce audit tables / findings.

## Permissions

- Edit: denied by frontmatter (`edit: deny`)
- Destructive operations: denied by frontmatter (`bash "*": deny`)
- Subagent launch: denied by frontmatter (`task: deny`) — cannot bypass edit deny via implement/release
- Allowed: `read`, `glob`, `grep`, `bash` (safe read-only list above)

## Must Do

- Start with `AGENTS.md` → `CURRENT.md` → `CONTEXT_MAP.md` → `scripts/agent_preflight.sh`
- Base findings on direct evidence (`git rev-parse`, `sha256sum`, file existence), not on doc claims
- Distinguish `filesystem failure vs bootstrap failure vs USB enumeration vs Runtime READY vs PCM flow vs physical PASS`

## Must Not

- Modify code or docs to make an audit pass
- Infer physical results
