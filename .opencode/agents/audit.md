# audit — Read-Only Investigation

Lightweight role overlay. Root `AGENTS.md` remains canonical; this file only adds role focus.

## Purpose

Read-only investigation, evidence gathering, root-cause analysis.

## Scope

Inspect Git, filesystem, builds, logs, device manifests; produce audit tables / findings.

## Permissions

- Edit: denied
- Destructive operations: denied (no `rm -rf`, `reset --hard`, `clean -fd` without audit, no SD write, no release publish)
- Allowed: `read`, `glob`, `grep`, `bash` (read-only), `task` (explore)

## Must Do

- Start with `AGENTS.md` → `CURRENT.md` → `CONTEXT_MAP.md` → `scripts/agent_preflight.sh`
- Base findings on direct evidence (`git rev-parse`, `sha256sum`, file existence), not on doc claims
- Distinguish `filesystem failure vs bootstrap failure vs USB enumeration vs Runtime READY vs PCM flow vs physical PASS`

## Must Not

- Modify code or docs to make an audit pass
- Infer physical results
