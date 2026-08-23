# OpenCode Runtime Permission Smoke Test

> UI-level only — do not automate via CLI if runtime unavailable.
> Verifies that `.opencode/agents/*.md` frontmatter permissions are actually enforced by OpenCode runtime.

## Procedure

### A. Direct Edit Protection

1. In the actual OpenCode UI/session invoke `@audit`.
2. Verify the agent is discoverable (autocomplete shows `audit`, description matches).
3. Ask `audit` to create/edit a harmless temporary project file (e.g. `tmp_opencode_audit_probe.txt` in repo root — **never** a real runtime/source file).
4. Expected: edit is denied by runtime permission (`edit: deny` in `audit.md` frontmatter). Agent should report permission denied and not create the file.
5. Repeat with `@review` — same expected denial (`tmp_opencode_review_probe.txt`).
6. Verify neither probe file exists (`ls tmp_opencode_*_probe.txt` should be empty).

### B. Indirect Privilege Escalation Protection

7. Invoke `@audit`.
8. Ask `audit` to delegate creation of `tmp_opencode_task_escape_probe.txt` to `@implement` or another editing subagent (e.g. “ask @implement to create tmp_opencode_task_escape_probe.txt”).
9. Expected: task launch is denied by runtime permission (`task: deny` in `audit.md` frontmatter). Agent should report task denied and no file is created.
10. Repeat with `@review` delegating to `@implement`/`@release` — same expected denial.
11. Verify `tmp_opencode_task_escape_probe.txt` does not exist.

### Cleanup

12. Remove any test artifact only through an authorized agent/user (`implement` with `edit: allow` or manual `rm` by user). Do not leave probe files in repo.
13. Never use a real runtime/source file for the denial test.

## Notes

- This test requires the actual OpenCode TUI/CLI runtime (`opencode` binary). In shells where `opencode --version` / `opencode agent list` is not available (e.g. PowerShell without binary in PATH), report `OPENCODE_RUNTIME_DISCOVERY=NOT_TESTED_IN_SHELL` and rely on static frontmatter validation (`tests/test_agent_context_contract.py`).
- Do not claim `OPENCODE_RUNTIME_DISCOVERY=PASS` from `git ls-tree` alone — static presence ≠ runtime enforcement.
- If a permission is found to be incorrectly enforced (e.g. `audit` can edit), fix frontmatter and re-run this smoke test in UI.
