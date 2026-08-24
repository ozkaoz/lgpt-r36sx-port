# Current Workspace State

**Last reviewed:** 2026-08-24
**Repo:** https://github.com/ozkaoz/lgpt-r36sx-port
> This is a last-known snapshot and must be verified against direct evidence (Git, build, device, release asset). If it contradicts direct evidence, direct evidence wins.

---

## Authority

Constitution: `AGENTS.md v2.1` > ACTIVE `DECISIONS.md` > direct evidence > this snapshot.
Do not trust hardcoded historical state.

## Repository

- Branch: `feature/treefrog-apps-lgpt` — verify `git branch --show-current` at session start (main at `f773504`, feature at `3751a47`)
- HEAD: RESOLVE FROM GIT — `git rev-parse HEAD` (feature 3751a47, main f773504)
- Upstream: `origin/main` — verify `git status --short --branch`
- Worktree: environment-specific — resolve `git worktree list`
- Stash: verify `git stash list`

## Current Product Baseline

- Version: Bacon-1.5 TreeFrog Apps Migration (2026-08-24)
- Core: `46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6` (1559548) — unchanged
- FrogUI: `76034bd3c142a9fe24df8729a1ef0dee6f1d8c6b4e5e046db05ebc890b54a0ef` (326700, `cubegm/cores/frogui_libretro.so`) — Apps LGPT, r36sx 028b011, CC BY-NC-SA 4.0
- TreeFrogUI required: `v1.0.15_a` (Apps-capable) — previous `v1.0.14_a` is historical
- ZIP: `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` `7295274` `faf7a230c06660b2299664f819f8d517c139311d5bbe8e8a0cbc421623ba0dec` (57 files, Apps→LGPT, Games absent) — see `docs/BACON_1_5_RELEASE_MANIFEST.md`
- Install: `Stock OS + TreeFrogUI v1.0.15_a + ZIP → Apps→LGPT` `POST_INSTALL_MANUAL_FIXES=0`

## Source Golden

- Previous: `4429d4e` + `b616a5b` (Bacon-1.5 bootstrap). New Apps migration source at `3751a47` (feature) — pending merge to main after clean-install PASS.

## Physical Golden

- Previous payload `C5C77A...` (56 files, Games→LGPT) PASS via `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md`.
- New Apps-only payload `faf7a230...` (57 files, Apps→LGPT) physically validated `PHASE_C_PHYSICAL_PASS=YES` (LOCAL/WINDOWS/SP404/ANDROID + switching PASS) via `build/frogui_candidate/apps_only/frogui_libretro.so` `76034b`. Awaiting true clean-install of exact RC ZIP.

## Release Golden

- Previous ZIP `C5C77A...` published, download-back `REMOTE_SHA==LOCAL_SHA`.
- New ZIP `faf7a230...` built deterministically from `sd_root` (57 files) — package tests `unzip -t PASS`, `bootstrap PASS`, `test_treefrog_apps_lgpt_release PASS` — pending publication after clean-install PASS. Historical release identities retained in `docs/BACON_1_5_RELEASE_MANIFEST.md`.

## Current Objective

- Phase C Apps-only physically PASS; Phase D/E release candidate prepared (feature branch). Next: true clean-install of RC ZIP on stock+TreeFrogUI v1.0.15_a, then publish as revised Bacon-1.5.

## Last Relevant Validation

- `RELEASE_AUDIO_BOOTSTRAP PASS` + `FROG_UI_APPS_LGPT PASS` + `TREEFROG_APPS_RELEASE PASS` (Apps-only, hide verified)
- `AGENT_CONTEXT_CONTRACT PASS` + `AGENT_PREFLIGHT PASS`
- `ELFs`: shipped `b07bbb` vs vanilla `f10caa` vs apps-dual `656242` vs apps-only `76034b` — all MIPS32r2 O32 hard-float, 7 PHDR, GLIBC 2.0/2.2/2.3/2.15, no generic drift

## Known Issues / Risks

- `scripts/install.sh`/`verify.sh` legacy U2523 — not canonical.
- Dirty exFAT false failures — SD health check before runtime blame.
- Generic `mipsel-linux-gnu-gcc 12.4` builds black-screen (029584…); official SDK `mips-mti 6.3.0` required.

## Pending Validation

- `TRUE_PHYSICAL_CLEAN_INSTALL` of RC ZIP `faf7a230...` on clean media → `POST_INSTALL_MANUAL_FIXES=0`
- `DOWNLOAD-BACK` after publication → `REMOTE_SHA==LOCAL_SHA`

## Next Exact Action

- True clean-install RC ZIP on R36SX (stock+TreeFrogUI v1.0.15_a, only ZIP), run full matrix, then publish revised Bacon-1.5.

## Stop Conditions

- Any protected runtime drift (lgpt wrapper/core, OTG, audio, H38) → STOP
- Any inferred PHYSICAL PASS without device → STOP
- Generic GCC FrogUI → STOP
- Machine-specific path as authority → STOP
