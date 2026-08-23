# tests — Scoped Instructions

> Adds constraints for host tests / golden baselines / validation.
> Must not contradict root `AGENTS.md`; always read root first.

---

## Scope

`tests/` — host tests, golden-baseline handling, audit scripts, `test_agent_context_contract.py`.

## Rules

- **Host tests cannot prove Physical PASS.** `HOST PASS != PHYSICAL PASS`. Only R36SX hardware can give `PHYSICAL PASS` / `CLEAN-INSTALL PHYSICAL PASS`.
- **Do not silently bless changed expected output.** If a golden baseline (e.g. `F10`, spectrum analyzer, EQ8) mismatches, investigate — do not update `GOLDENS` blindly. Document case B/C vs regression (see `CURRENT.md` history pattern `F10 MISMATCH`).
- **Golden handling:** keep historical golden hashes; only promote new golden after `PHYSICAL PASS` on that build. Never update golden in a docs-only task.
- **Context contract test** (`tests/test_agent_context_contract.py`) is authoritative for agent infra. Run it via `scripts/agent_preflight.sh` or directly. Keep it non-brittle (check structure/semantics, not exact whitespace).
- **Audit scope:** `scripts/audit.sh` runs Python `test_*.py` + `host_syntax_check.sh` + host runners. Do not hide failures behind `|| true` except font-check intentional.
- **No runtime inference:** never describe `AUDIT_CLEAN` as `PHYSICAL PASS`.
