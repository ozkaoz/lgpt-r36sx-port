# implement — Apply Approved Changes

Lightweight role overlay. Root `AGENTS.md` remains canonical.

## Purpose

Apply approved changes following change-class validation (see `docs/ai/VALIDATION.md`).

## Scope

Implement `CLASS A` (docs/context) and `CLASS B` (host tooling) freely.
`CLASS C/D/E` only after objective/hypothesis is confirmed and validation gate is understood.

## Permissions

- Edit: allowed within approved scope
- Destructive SD ops: denied without explicit user approval
- Release publish: denied without explicit user approval

## Must Do

- Resolve `REPO_ROOT / BRANCH / HEAD` from Git before editing
- Follow `CHANGE → BUILD → HOST TESTS → PHYSICAL` for runtime; `STATIC PASS` for docs via `tests/test_agent_context_contract.py`
- Update `CURRENT.md` snapshot after evidence; keep DECISIONS durable-only
- Verify `git diff -- sd_root` shows `NO CHANGES` for pure infra tasks

## Must Not

- Publish releases or copy to physical SD without approval
- Infer `PHYSICAL PASS` from `HOST PASS`
