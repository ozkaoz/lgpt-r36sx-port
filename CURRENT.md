# Current Workspace State

**Last reviewed:** 2026-08-23
**Repo:** https://github.com/ozkaoz/lgpt-r36sx-port
> This is a last-known snapshot and must be verified against direct evidence (Git, build, device, release asset). If it contradicts direct evidence, direct evidence wins.

---

## Authority

Constitution: `AGENTS.md v2.0` > ACTIVE `DECISIONS.md` > direct evidence > this snapshot.
Do not trust hardcoded historical state.

## Repository

- Branch: `main` (verify: `git branch --show-current`)
- HEAD: `b616a5b65984a3e1952e25a8cc340eb53534b90c` (verify: `git rev-parse HEAD`)
- Upstream: `origin/main` (verify: `git status --short --branch`)
- Worktree: single checkout at `C:/Users/DaFunkNoise/Documents/Default Project/lgpt-r36sx-port` (verify: `git worktree list`)
- Stash: 2 entries unrelated to Bacon-1.5 (verify: `git stash list`)

## Current Product Baseline

- Version: Bacon-1.5 Golden Bootstrap (2026-08-23)
- Core: `46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6` (1559548, `cubegm/cores/lgpt_core.so`)
- ZIP: `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` (7138546, `C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E`, 56 files, H38-only)

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

- **CLASS A/B** — Modernize multi-agent infrastructure (AGENTS v2, context routing, validation gates, OpenCode roles). No runtime/release payload change.
- Expected: `CORE_CHANGED=NO  RUNTIME_CHANGED=NO  RELEASE_PAYLOAD_CHANGED=NO  SD_ROOT_CHANGED=NO`.

## Last Relevant Validation

- `RELEASE_AUDIO_BOOTSTRAP PASS` (sentinel empty, profile STEREO_48K, mode/policy LOCAL_CONSOLE, no volatile).
- `CORE SHA 46bd84` verified from `sd_root/cubegm/cores/lgpt_core.so` (1559548).

## Known Issues / Risks

- `scripts/install.sh` / `verify.sh` remain legacy U2523 (`/mnt/f`, `/mnt/d/...`) — labeled not canonical; do not use for Bacon-1.5 without audit.
- Dirty exFAT previously caused false USB failures — SD health check precedes runtime blame.

## Pending Validation

- `tests/test_agent_context_contract.py` — must pass after infra rewrite.
- `scripts/agent_preflight.sh --help` / `bash -n` syntax check.

## Next Exact Action

1. Finish docs/ai + scoped agents + preflight tool, 2. run `python3 tests/test_agent_context_contract.py && python3 tests/test_release_audio_bootstrap.py`, 3. `git diff -- sd_root` must show `NO CHANGES`, 4. independent review pass.

## Stop Conditions

- Any runtime/core/sd_root change detected → STOP.
- Any inferred `PHYSICAL PASS` without device evidence → STOP.
- Any machine-specific path reintroduced as universal authority → STOP.
