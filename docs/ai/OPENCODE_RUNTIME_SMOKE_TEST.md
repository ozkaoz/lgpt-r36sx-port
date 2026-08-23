# OpenCode Runtime Permission Smoke Test

> UI-level only — do not automate via CLI if runtime unavailable.
> Verifies that `.opencode/agents/*.md` frontmatter permissions are actually enforced by OpenCode runtime.

## Procedure

1. In the actual OpenCode UI/session invoke `@audit`.
2. Verify the agent is discoverable (autocomplete shows `audit`, description matches).
3. Ask `audit` to create/edit a harmless temporary project file (e.g. `tmp_opencode_audit_probe.txt` in repo root — **never** a real runtime/source file).
4. Expected: edit is denied by runtime permission (`edit: deny` in `audit.md` frontmatter). Agent should report permission denied and not create the file.
5. Repeat with `@review` — same expected denial.
6. Remove any test artifact only through an authorized agent/user (`implement` with `edit: allow` or manual `rm` by user).
7. Never use a real runtime/source file for the denial test.

## Notes

- This test requires the actual OpenCode TUI/CLI runtime (`opencode` binary). In shells where `opencode --version` / `opencode agent list` is not available (e.g. PowerShell without binary in PATH), report `OPENCODE_RUNTIME_DISCOVERY=NOT_TESTED_IN_SHELL` and rely on static frontmatter validation (`tests/test_agent_context_contract.py`).
- Do not claim `OPENCODE_RUNTIME_DISCOVERY=PASS` from `git ls-tree` alone — static presence ≠ runtime enforcement.
- If a permission is found to be incorrectly enforced (e.g. `audit` can edit), fix frontmatter and re-run this smoke test in UI.
