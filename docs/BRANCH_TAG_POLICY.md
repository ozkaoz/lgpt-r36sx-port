# Branch & Tag Workflow — Bacon 1.5 Canon

**Main is canonical stable state.** `main` always equals the most recently accepted checkpoint/release. Resolve current identities (e.g. Bacon-1.5, single-startup-actions) via `CURRENT.md`, `git log --oneline --decorate`, and `docs/BACON_1_5_RELEASE_MANIFEST.md` — not hardcoded SHAs.

## Branches
- `main` — stable, physically validated, single LGPT, 46bd84 core, H38-only.
- Feature: `feature/<name>` short-lived, created from `main`.
  1. Implement (no audio/driver/FrogUI changes unless validated)
  2. Host tests (`run_host_startup_project_actions.sh`, `host_syntax_check.sh`)
  3. MIPS build (`BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh`, TC mipsel)
  4. Physical R36SX validation (Local/Mixer/EQ8/Analyzer/Pitch/Chopper/Sampler, projects)
  5. Checkpoint/tag if needed (`single-*`, `checkpoint-*`)
  6. Fast-forward to `main` (`git merge --ff-only`), push, delete feature branch (`git push origin --delete`).

Do not accumulate permanent feature branches. `main` → feature → physical PASS → main → tag → delete.

## Tags
- **Release:** `Bacon-X.Y` (e.g. `Bacon-1.5` → `c778512`, LATEST, prerelease false). Public distribution, movable only for hygiene/docs (functional identical).
- **Physical checkpoint:** `single-startup-actions` (557b26d, immutable), `single-lgpt-functional` (b7b2e46), `sp404-functional-p3` (beb8a12), `golden-bacon-1.4` etc. Immutable, never moved.
- **Legacy preservation:** `Bacon-1.5-U2523-legacy` (6f944d6, old Bacon-1.5 U2.52.3), `main-pre-bacon15-consolidation-20260822` (449041f, old main). Keep.
- **Build checkpoint:** only for exceptional recovery; not every experiment.

Tags are cheap, preserve history. Do not delete release/checkpoint/legacy tags. Only `ACCIDENTAL` tags are candidates for deletion.

## Hygiene
- Release ZIP `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` (direct-copy, no wrapper, H38-only APK at root) + `LGPT_R36SX_Bacon-1.5_Android.apk` + `SHA256SUMS.txt` are the public artifacts.
- `sd_root/` is the canonical public payload (port overlay). TreeFrog vendor base (e.g. `cubegm/cores/libemu_*`) is not duplicated in git; recorded in `PHYSICAL_SD_FINAL_*` manifests.
- Docs: `SINGLE_STARTUP_ACTIONS_CHECKPOINT.md`, `BACON_1_5_RELEASE_MANIFEST.md`, `PHYSICAL_SD_FINAL_*`, `RELEASE_SD_*` document exact validated state.
