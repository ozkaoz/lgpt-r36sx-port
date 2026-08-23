# Branch & Tag Workflow — Policy

**Main is canonical accepted repository state.** `main` holds the most recently accepted checkpoint. Resolve current identities via `CURRENT.md`, `git log --oneline --decorate`, and `docs/BACON_1_5_RELEASE_MANIFEST.md` — not hardcoded SHAs.

`main` may advance through CLASS A/B (docs/tooling) commits without changing the current PHYSICAL/RELEASE GOLDEN. Release Golden is separately identified via release tag + manifests + physical evidence.

## Branches

- `main` — canonical accepted state; advances by fast-forward from validated checkpoints.
- Feature: `feature/<name>` short-lived, created from `main`.
  1. Classify change using `AGENTS.md` / `docs/ai/VALIDATION.md` (CLASS A/B/C/D/E)
  2. Execute ONLY the validation gate required by that class (see `docs/ai/VALIDATION.md` as authority)
  3. Accepted checkpoint/tag if applicable
  4. Fast-forward/approved merge to `main`, push, delete short-lived feature branch when appropriate (`git push origin --delete`).

  Examples:
  - CLASS A: context/static tests only
  - CLASS B: relevant host/tool tests
  - CLASS C: build + host + physical R36SX
  - CLASS D: package + physical clean-install implications
  - CLASS E: deterministic package + clean-install physical + download-back

Do not accumulate permanent feature branches.

## Tags

- **Release:** `Bacon-X.Y` (e.g. `Bacon-1.5`, LATEST). Public distribution, **IMMUTABLE once published**. If a public release asset needs correction while code/tag is functionally identical, update the release assets/body per release policy without silently moving the Git tag. Any intentional tag movement requires explicit human approval and documented reason.
- **Physical checkpoint:** `single-*`, `sp404-functional-p3`, `golden-bacon-1.4` etc. Immutable validation markers — never moved.
- **Legacy preservation:** `Bacon-1.5-U2523-legacy`, `main-pre-bacon15-consolidation-20260822` etc. Immutable historical markers. Keep.
- **Build checkpoint:** only for exceptional recovery; not every experiment.

Tags are cheap, preserve history. Do not delete release/checkpoint/legacy tags. Only `ACCIDENTAL` tags are candidates for deletion. Git remains authority for exact mappings; historical tag tables belong in evidence docs if needed, not policy.

## Hygiene

- Release ZIP `LGPT_R36SX_Bacon-X.Y_SD_ROOT.zip` (e.g. `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip`, direct-copy, no wrapper, H38-only APK at root — resolve current concrete release identity from `CURRENT.md` and authoritative release manifest) + `LGPT_R36SX_Bacon-X.Y_Android.apk` (separate Android asset when applicable per authoritative release manifest) + `SHA256SUMS.txt` are the public artifacts (see `docs/ai/RELEASE_CONTRACT.md`).
- `sd_root/` is the canonical public payload (port overlay). TreeFrog vendor base (e.g. `cubegm/cores/libemu_*`) is not duplicated in git; recorded in `PHYSICAL_SD_FINAL_*` manifests.
- Docs: `SINGLE_STARTUP_ACTIONS_CHECKPOINT.md`, `BACON_1_5_RELEASE_MANIFEST.md`, `PHYSICAL_SD_FINAL_*`, `RELEASE_SD_*` document exact validated state.
