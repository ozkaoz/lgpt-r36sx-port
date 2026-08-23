# VALIDATION — Change-Class Gates

Detailed procedures for `AGENTS.md` Section 4. Never infer physical results.

---

## Change Classes

### CLASS A — Context / Documentation

**Scope:** `AGENTS.md`, `CURRENT.md`, `CONTEXT_MAP.md`, `DECISIONS.md`, README text, agent definitions.

| Gate | Required | Command |
|------|----------|---------|
| Static | review diff + `test_agent_context_contract.py` | `python3 tests/test_agent_context_contract.py` |
| Build | not required unless docs change release/runtime material | — |
| Host tests | relevant host tests if tooling touched | `bash scripts/audit.sh` subset |
| Physical | NOT REQUIRED | — |
| Checkpoint eligible | `STATIC PASS` + `HOST PASS` (if applicable) |  |

### CLASS B — Host Tooling / Test Infra

**Scope:** non-deployment audit tooling, host-only tests, context tests.

| Gate | Required |
|------|----------|
| Static | `bash -n` scripts, diff review |
| Host tests | relevant host tests |
| Physical | only if tooling changes deployed artifact |
| Checkpoint | `STATIC PASS` + `HOST PASS` |

### CLASS C — Runtime

**Scope:** C/C++, audio, input, DSP, TreeFrog, Mixer/EQ/Analyzer/Pitch/Chopper, filesystem runtime.

| Gate | Required |
|------|----------|
| Static | diff, refs, `bash -n`, `host_syntax_check.sh` |
| Build | clean build in WSL (`BUILD_TREEFROG_R36SX_*.sh`), record SHA/size/warnings HEAD |
| Host tests | full relevant suite (`tests/run_host_*`, `audit.sh`) |
| Physical | **deployment candidate + physical R36SX** (boot, audio L/R, UI, controls, affected feature) |
| Checkpoint | only after `PHYSICAL PASS` on R36SX |

### CLASS D — Deployment / Bootstrap

**Scope:** launcher, OTG setup, `sd_root` persistent baseline (`lgpt/otg/*`), TreeFrogUI integration.

| Gate | Required |
|------|----------|
| Package | `test_release_audio_bootstrap.py` + `verify_copy_root_layout.sh` + `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt` consistency |
| Physical | **physical deployment PASS + clean-install implications** (fresh SD) |
| Checkpoint | only after `PHYSICAL PASS` |

### CLASS E — Release

**Scope:** public ZIP, `SHA256SUMS`, `RELEASE_SD_INCLUDED_FILES.txt`, install contract.

| Gate | Required |
|------|----------|
| Deterministic build | `scripts/build_copy_root_release.py` with `FIXED_TIME` |
| Static package | `unzip -t`, `test_release_audio_bootstrap.py`, `MANIFEST_CONSISTENT=YES` |
| Physical | **TRUE clean install** `Stock OS + TreeFrogUI + ZIP contents` with `POST_INSTALL_MANUAL_FIXES=0` |
| Matrix | `LOCAL PASS` + `WINDOWS DETECT+PLAYBACK+RECORD PASS` + `SP404 DETECT+PLAYBACK PASS` + `ANDROID BRIDGE+PLAYBACK+RECORD PASS` (no `Runtime is not ready`) |
| Download-back | publish then download asset, verify `REMOTE_SHA == LOCAL_SHA` (`REMOTE_IDENTICAL=YES`) |
| Checkpoint | only after `CLEAN-INSTALL PHYSICAL PASS` + `DOWNLOAD-BACK PASS` |

---

## Labels (use exactly)

```
STATIC PASS / FAIL
HOST PASS / FAIL
PACKAGING PASS / FAIL
PHYSICAL PASS / FAIL
CLEAN-INSTALL PHYSICAL PASS / FAIL
DOWNLOAD-BACK PASS / FAIL
```

Do NOT use `DONE / VERIFIED / VALIDATED` without class-appropriate gate.
Do NOT blend gates: a runtime fix cannot be checkpointed on `HOST PASS` alone.

---

## Checkpoint Eligibility (replaces blanket rule)

Previously overly broad `NO CHECKPOINT BEFORE SD PASS` becomes:

```
CONTEXT/DOCS CHECKPOINT  → context/static validation (CLASS A)
HOST TOOLING CHECKPOINT  → static + host tests (CLASS B)
RUNTIME CHECKPOINT       → PHYSICAL PASS (CLASS C)
DEPLOYMENT CHECKPOINT    → physical deployment PASS (CLASS D)
RELEASE CHECKPOINT       → clean-install + download-back gate (CLASS E)
```

---

## Procedure Checklist

Before any `PHYSICAL PASS` claim, record: `HEAD`, build SHA/size, `sd_root` SHA, SD mount health, test steps, expected vs actual, log/photo.
Before any `RELEASE GOLDEN` claim, record: ZIP SHA/size/filecount, `SHA256SUMS.txt` match, included-files match, `REMOTE_DOWNLOAD_SHA`, `REMOTE_IDENTICAL`, `UNZIP_TEST_REMOTE`, `BOOTSTRAP_TEST_REMOTE`.

---

## Handoff Evidence

Each class must produce:

```
CHANGE_CLASS=  FILES_CHANGED=  HEAD=  CHECKS_RUN=  PHYSICAL_EVIDENCE=  RELEASE_EVIDENCE=  BLOCKER=  NEXT_EXACT_ACTION=  STOP_CONDITION=
```

Raw logs in dedicated evidence files, not CURRENT.md append.
