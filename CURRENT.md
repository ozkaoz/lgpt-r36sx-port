# Current Workspace State

**Last reviewed:** 2026-08-23
**Repo:** https://github.com/ozkaoz/lgpt-r36sx-port
> This is a last-known snapshot and must be verified against direct evidence (Git, build, device, release asset). If it contradicts direct evidence, direct evidence wins.

---

## Authority

Constitution: `AGENTS.md v2.1` > ACTIVE `DECISIONS.md` > direct evidence > this snapshot.
Do not trust hardcoded historical state.

## Repository

- Branch: last-known `main` — verify `git branch --show-current` at session start
- HEAD: RESOLVE FROM GIT AT SESSION START — `git rev-parse HEAD` (do not hardcode self-SHA here)
- Upstream: last-known `origin/main` — verify `git status --short --branch`
- Worktree: environment-specific — resolve `git worktree list`
- Stash: verify `git stash list` (example: 2 unrelated entries observed)

## Current Product Baseline

- Version: Bacon-1.5 Golden Bootstrap (2026-08-23)
- Core: `46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6` (1559548, `cubegm/cores/lgpt_core.so`) — authoritative via `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt` + `sd_root`
- ZIP: `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` (7138546, `C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E`, 56 files, H38-only) — see `docs/BACON_1_5_RELEASE_MANIFEST.md`

## Source Golden

- Commit `4429d4e49d1b47775ea02d6a6e3d9667d6c80dd9` + merge `b616a5b` — deterministic clean-install closure without core change.

## Physical Golden

- Payload `C5C77A...` installed via `Stock OS + TreeFrogUI + ZIP contents` with `POST_INSTALL_MANUAL_FIXES=0`.
- Matrix PASS (staged bootstrap, exFAT Healthy): `LOCAL PASS | WINDOWS DETECT+PLAYBACK+RECORD PASS | SP404 DETECT+PLAYBACK PASS | ANDROID BRIDGE+PLAYBACK+RECORD PASS`.
- Evidence: `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md`.

## Release Golden

- ZIP `C5C77A...` published, downloaded back, `REMOTE_SHA == LOCAL_SHA`, `unzip -t PASS`, `BOOTSTRAP_TEST PASS`, `MANIFEST_CONSISTENT=YES`.
- Evidence: `docs/BACON_1_5_RELEASE_MANIFEST.md`, `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt`.

## Current Objective

- No active implementation/runtime task. Await explicit user-approved objective.
- At next task: run `bash scripts/agent_preflight.sh`, classify A/B/C/D/E via `docs/ai/VALIDATION.md`, route through `CONTEXT_MAP.md`.

## Last Relevant Validation

- `RELEASE_AUDIO_BOOTSTRAP PASS` (sentinel empty, profile STEREO_48K, mode/policy LOCAL_CONSOLE, no volatile).
- `AGENT_CONTEXT_CONTRACT PASS` + `AGENT_PREFLIGHT PASS` (V2.1 pre-push).
- `CORE SHA 46bd84` verified from `sd_root/cubegm/cores/lgpt_core.so` (1559548).

## Known Issues / Risks

- `scripts/install.sh` / `verify.sh` remain legacy U2523 — labeled not canonical; do not use for Bacon-1.5 without audit.
- Dirty exFAT previously caused false USB failures — SD health check precedes runtime blame.

## Pending Validation

- None — V2.1 infra validated; next task must run `agent_preflight` and classify change.

## Next Exact Action

- Run `bash scripts/agent_preflight.sh` at start of next task, classify requested change via `docs/ai/VALIDATION.md`, then route through `CONTEXT_MAP.md`.

## Stop Conditions

- Any runtime/core/sd_root change detected → STOP.
- Any inferred `PHYSICAL PASS` without device evidence → STOP.
- Any machine-specific path reintroduced as universal authority → STOP.
