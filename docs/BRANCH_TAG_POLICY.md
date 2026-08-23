# Branch & Tag Workflow — Policy

**Main is canonical accepted repository state.** `main` holds the most recently accepted checkpoint. Resolve current identities via `CURRENT.md`, `git log --oneline --decorate`, and `docs/BACON_1_5_RELEASE_MANIFEST.md` — not hardcoded SHAs.

`main` may advance through CLASS A/B (docs/tooling) commits without changing the current PHYSICAL/RELEASE GOLDEN. Release Golden is separately identified via release tag + manifests + physical evidence.

## Branches

- `main` — canonical accepted state; advances by fast-forward from validated checkpoints.
- Feature: `feature/<name>` short-lived, created from `main`.
  1. Implement (no audio/driver/FrogUI changes unless validated)
  2. Host tests (`run_host_startup_project_actions.sh`, `host_syntax_check.sh`)
  3. MIPS build (`BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh`, TC mipsel)
  4. Physical R36SX validation (Local/Mixer/EQ8/Analyzer/Pitch/Chopper/Sampler, projects)
  5. Checkpoint/tag if needed (`single-*`, `checkpoint-*`)
  6. Fast-forward to `main` (`git merge --ff-only`), push, delete feature branch (`git push origin --delete`).

Do not accumulate permanent feature branches. `main` → feature → physical PASS → main → tag → delete.

## Tags

- **Release:** `Bacon-X.Y` (e.g. `Bacon-1.5`, LATEST). Public distribution, **IMMUTABLE once published**. If a public release asset needs correction while code/tag is functionally identical, update the release assets/body per release policy without silently moving the Git tag. Any intentional tag movement requires explicit human approval and documented reason.
- **Physical checkpoint:** `single-*`, `sp404-functional-p3`, `golden-bacon-1.4` etc. Immutable validation markers — never moved.
- **Legacy preservation:** `Bacon-1.5-U2523-legacy`, `main-pre-bacon15-consolidation-20260822` etc. Immutable historical markers. Keep.
- **Build checkpoint:** only for exceptional recovery; not every experiment.

Tags are cheap, preserve history. Do not delete release/checkpoint/legacy tags. Only `ACCIDENTAL` tags are candidates for deletion. Git remains authority for exact mappings; historical tag tables belong in evidence docs if needed, not policy.

## Hygiene

- Release ZIP `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` (direct-copy, no wrapper, H38-only APK at root) + `LGPT_R36SX_Bacon-1.5_Android.apk` + `SHA256SUMS.txt` are the public artifacts (see `docs/ai/RELEASE_CONTRACT.md`).
- `sd_root/` is the canonical public payload (port overlay). TreeFrog vendor base (e.g. `cubegm/cores/libemu_*`) is not duplicated in git; recorded in `PHYSICAL_SD_FINAL_*` manifests.
- Docs: `SINGLE_STARTUP_ACTIONS_CHECKPOINT.md`, `BACON_1_5_RELEASE_MANIFEST.md`, `PHYSICAL_SD_FINAL_*`, `RELEASE_SD_*` document exact validated state.
