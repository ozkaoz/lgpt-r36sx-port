# DECISIONS — Durable Technical Decisions

**Last reviewed:** 2026-08-23
**Policy:** Only durable architecture/project decisions. Operational events (push, copy to SD, one-off build PASS) belong in Git/evidence, not here.

| Field | Meaning |
|-------|---------|
| ID | `DEC-YYYY-MM-DD-NN` unique |
| Status | `ACTIVE` / `SUPERSEDED` / `DEPRECATED` |
| Scope | Subsystem / area |

---

## DEC-2026-08-21-01 — No gain table indexed by dB+80

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** Audio / DSP (InstrumentEq, ParametricEQ)

**Context:** Audit for supposed BUG1 (`idx = dB+80` gain table) across full source tree and history U2.52..U2.71.
**Decision:** No such table exists. Gain is `powf(10, dB/20)` or linear-indexed compressor table, clamped -24..+24 dB.
**Reason:** Exhaustive grep and history review found no `eqGainTable` / `dB+80` indexer.
**Consequences:** BUG1 was non-existent; do not add dB-indexed table.
**Evidence:** `InstrumentEq.cpp:156-159`, `ParametricEQ.cpp:108-110`, `Compressor.cpp:190-207`, `FxPages.h:442` is VU only.
**Related:** `source/sources/Application/Audio/InstrumentEq.cpp`, `EqBiquad.h`

---

## DEC-2026-08-21-02 — SDL2 driver uses SDL1.2 legacy API

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** Audio drivers (SDL/SDL2)

**Context:** Both `SDL` and `SDL2` adapters use `SDL_OpenAudio`/`SDL_PauseAudio` (SDL1.2) not `SDL_OpenAudioDevice`.
**Decision:** Document as known debt; do not treat as modern SDL2 without migration.
**Reason:** Partial SDL2 migration; functional on R36SX via legacy path.
**Consequences:** May have issues on modern SDL2 hosts; requires explicit migration before stable host release.
**Evidence:** `SDLAudioDriver.cpp:65` `SDL_OpenAudio`, no `SDL_InitSubSystem(SDL_INIT_AUDIO)`, SDL1 callback signature.
**Related:** `source/sources/Adapters/SDL/Audio/SDLAudioDriver.cpp`, `source/sources/Adapters/SDL2/Audio/SDLAudioDriver.cpp`

---

## DEC-2026-08-21-03 — EQ <80 Hz: Q=0.707 for slope>1 on all filter types

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** InstrumentEq / EqBiquad

**Context:** High slopes (S8 =96dB/oct) with Q=1 at <80 Hz caused +48 dB resonance on BELL/shelves.
**Decision:** For `hz <80 && slope>1` force `Q=0.707` (Butterworth) for ALL types including BELL/LOW_SHELF/HIGH_SHELF.
**Reason:** Stability and flat wall response at sub-80 Hz; measured BELL 45 Hz lvl6 Q1 S8 → 48 dB, with fix → ~6 dB flat.
**Consequences:** S8 wall at 40 Hz flat; BELL <80 loses selectivity but gains stability.
**Evidence:** `InstrumentEq.cpp:384-394` `qForDsp=0.707...`, `EqBiquad.h:61-64` double precision.
**Related:** `source/sources/Application/Audio/InstrumentEq.cpp`, `source/sources/Application/Audio/EqBiquad.h`

---

## DEC-2026-08-21-04 — Analyzer: Blackman window + hold >140 Hz

**Date:** 2026-08-21
**Status:** SUPERSEDED by DEC-2026-08-21-31 and DEC-2026-08-21-32
**Scope:** SpectrumAnalyzer / InstrumentEqView

**Context:** Hann -31 dB sidelobes caused hihat to light false bass; hold on bass kept kick tails.
**Decision:** FFT Hann→Blackman (-67 dB, `a0=0.42 a1=0.5 a2=0.08`), visual hold only `fcHold>140 Hz`, uniform `visGain`.
**Reason:** Eliminate spectral leakage diagonal; refine later with exclusive bins / stereo power.
**Consequences:** Superseded by exact Hz mapping and stereo power decisions; Blackman and >140 Hz hold preserved.
**Evidence:** `SpectrumAnalyzer.cpp:141`, `InstrumentEqView.cpp:634` (pre-fix baseline).
**Related:** `source/sources/Application/Audio/SpectrumAnalyzer.cpp`

---

## DEC-2026-08-21-05 — Startup version string Bacon 1.5

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** UI / Versioning

**Context:** Legacy `Piggy build %s.%s.%s` leaked into R36SX build strings.
**Decision:** `NullView.cpp:22`, `AppWindow.cpp:1430` → `"LGPT R36SX - Bacon 1.5"`, `Project.h:23` `PROJECT_RELEASE "5"`, `BUILD_COUNT "0-bacon15"`.
**Reason:** Unified Bacon-1.5 branding; old Piggy string remains orphan in .rodata.
**Consequences:** Visible version on device is `LGPT R36SX - Bacon 1.5`.
**Evidence:** `NullView.cpp:22`, `AppWindow.cpp:1430`, `Project.h:23`.
**Related:** `source/sources/Application/Views/NullView.cpp`, `source/sources/Application/AppWindow.cpp`

---

## DEC-2026-08-21-30 — EQ8 sub-80 Hz fix: Q24 round, shelf NaN guard, UI/DSP coherence

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** EqBiquad / InstrumentEq / InstrumentEqView

**Context:** Q15 truncation caused LPF20 `b=0` (-96 dB), HPF20 err 1.8 dB, shelf `sqrt(neg)` NaN, UI Q15 vs DSP Q24 diverged 6 dB.
**Decision:** `coeffFromDouble` Q24 round-to-nearest + int32 saturate, clamp shelf `arg<0→0`, `GetBandCoeffs` round `(v+256)>>9`, View `eqBiquadCoeffsShift 24` with mirrored `qDraw`. Global `FIXED_SHIFT=15` kept; local precision Q24.
**Reason:** Q23 failed HPF20 0.15>0.10 and overflow; Q24 minimal passing with margin (8.2G vs 16G/33G for Q25/26) per `eq_study4.py`.
**Consequences:** HPF/LPF 20-100 Butterworth ` -3.01±0.10`, shelves NaN-free, UI=DSP ±0.2 dB, no float hot path.
**Evidence:** `eq_sub80_host_test` PASS `HPF20 -3.086 err -0.076`, `eq8_struct` 109 PASS, build `46c4714...` SD PASS 2026-08-21 13:45.
**Related:** `EqBiquad.h`, `InstrumentEq.cpp/h`, `InstrumentEqView.cpp`

---

## DEC-2026-08-21-31 — Analyzer: exclusive bins, correct Hz mapping, Blackman scale

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** SpectrumAnalyzer / InstrumentEqView

**Context:** Overlapped ±30%→±10% windows made 1 kHz tone light 770-1430 Hz; `visGain` tilted treble; Hann/Blackman compensation miscalibrated; ring leaked across instruments.
**Decision:** Exclusive intervals `sqrt(f[i]*f[i+1])`, `lo=ceil(edgeLow/hzPerBin)` `hi=ceil(edgeHigh/hzPerBin)-1`, power-interpolated sub-bin, `BinFrequency(i)`, peak `7..6826` parabolic once per `Compute()` (`peakHz_`), Blackman `ampScale=2/sum` (0 dBFS→1.0, no visGain/*4), lazy window, ring+mean copy, `clearCapture()` idempotent, `heldH_[308]` member reset on focus.
**Reason:** One tone → one pixel; uniform -90..0 dB without treble tilt; clean instrument switch.
**Consequences:** Replaces visGain/windows of DEC-04; preserves Blackman and hold>140.
**Evidence:** `spectrum_analyzer 50 PASS` (984 Hz 0.9999 width1, sweep ±1px), `analyzer_target 1781 PASS`, build `c43006a` SD PASS.
**Related:** `SpectrumAnalyzer.cpp/h`, `InstrumentEqView.cpp/h`

---

## DEC-2026-08-21-32 — Analyzer final: stereo power, DisplayPeak tilt, TreeFrog auto

**Date:** 2026-08-21
**Status:** ACTIVE
**Scope:** SpectrumAnalyzer / InstrumentEqView / Android payload

**Context:** Stereo hihat cancelled by mono sum; tilt on floor created silence diagonal; Peak used raw mag vs display; hold static; TreeFrog required manual select.
**Decision:** Stereo `ringL/R` + `power=0.5*(|L|²+|R|²)` `amp=sqrt(power)*2/sum`, `DisplayPeakFrequency()` on 308 bins with `tilt 4.5*log2(fc/1000)` floor -90 (gate before tilt), `exp(-dt/300)` hold 100ms/release 300ms, `canvasW=309` `bx=(i*canvasW)/n`, `hat_probe` 308 bins Tests A-E, Android `r36s_aoa_*_h36` + APKs, TreeFrog auto `lgpt_libretro.so`.
**Reason:** Antiphase no longer cancels; diagonal silence fixed; peak matches display; Android payload deterministic.
**Consequences:** Replaces mono windows of DEC-31; preserves Q24 EQ8.
**Evidence:** `analyzer_h1_stereo 6 PASS` (in-phase 0.144 antiphase 0.144), `hat_probe A-E PASS`, build `66c966d...` R36SX F1-F7 PASS.
**Related:** `SpectrumAnalyzer.cpp/h`, `InstrumentEqView.cpp/h`, `scripts/install.sh`, `verify.sh`

---

## DEC-2026-08-23-01 — Multi-agent context architecture V2

**Date:** 2026-08-23
**Status:** ACTIVE
**Scope:** AI infrastructure / docs

**Context:** AGENTS v1.1 (334 lines) hardcoded `feature/bacon-1.5-fx`, WSL path `/home/dafunknoise/lgpt-repo`, machine `/mnt/g`, stale branch/HEAD; CURRENT was 258-line append-only changelog; DECISIONS stored push/SD events; no `docs/ai`, no preflight, no scoped agents, no OpenCode roles.
**Decision:** Constitution `AGENTS.md v2.0` (150-220 lines, invariants only), `CURRENT.md` concise snapshot with `must verify` stamp, `CONTEXT_MAP.md` stable router, `docs/ai/VALIDATION.md` + `RELEASE_CONTRACT.md`, `scripts/agent_preflight.sh` + `tests/test_agent_context_contract.py`, scoped `device/`, `TREEFROG/`, `tests/` AGENTS, `.opencode/agents/{audit,implement,review,release}.md`, lazy loading, compact handoff.
**Reason:** Prevent context drift, mutable duplication, machine-specific authority, and legacy U2523 being mistaken for Bacon-1.5.
**Consequences:** Agents load only relevant context; mutable state lives in Git/CURRENT cache; `scripts/install.sh`/`verify.sh` labeled LEGACY U2523.
**Evidence:** `tests/test_agent_context_contract.py PASS`, `bash -n scripts/agent_preflight.sh PASS`, `git diff -- sd_root` NO CHANGES, core 46bd84 unchanged.
**Related:** `AGENTS.md`, `CURRENT.md`, `CONTEXT_MAP.md`, `docs/ai/*`, `scripts/agent_preflight.sh`

---

## DEC-2026-08-23-02 — Golden Bootstrap clean-install release closure

**Date:** 2026-08-23
**Status:** ACTIVE
**Scope:** Release / OTG / deployment

**Context:** Persistent audio setup was missing from ZIP, requiring manual sentinel creation after install.
**Decision:** Release ZIP `C5C77A0212...` (7138546, 56 files) includes persistent baseline: `enable_lgpt_uac2_bridge` (empty), `audio_usb_profile STEREO_48K`, `audio_driver_mode/policy LOCAL_CONSOLE`, `active_audio_branch audio_driver_local_console`, `branches/audio_driver_local_console/MODE LOCAL_CONSOLE`. Core `46bd84` unchanged.
**Reason:** `Stock OS + TreeFrogUI + ZIP contents = fully functional PORT` with `POST_INSTALL_MANUAL_FIXES=0` proven by staged Golden Bootstrap.
**Consequences:** `WORKS ON DEV SD != RELEASE PACKAGE COMPLETE` is now enforced; `sd_root` is canonical payload source.
**Evidence:** `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md PASS`, `docs/BACON_1_5_RELEASE_MANIFEST.md`, commit `4429d4e`, merge `b616a5b`.
**Related:** `sd_root/lgpt/otg/*`, `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt`, `docs/RELEASE_SD_INCLUDED_FILES.txt`

---

## DEC-2026-08-23-03 — Persistent vs volatile packaging

**Date:** 2026-08-23
**Status:** ACTIVE
**Scope:** Release / OTG / sd_root

**Context:** Volatile runtime state was at risk of being packaged as if it were install baseline.
**Decision:** Persistent (may be packaged when required for deterministic install): files in DEC-2026-08-23-02. Volatile (MUST NOT be packaged): FIFO, PID, daemon_pid/version, capture_abi, setup_result, sp404_card, aoa state, device detection state, /tmp, runtime logs.
**Reason:** Golden Bootstrap proves persistent setup is legitimate install content while volatile is runtime-only.
**Consequences:** `tests/test_release_audio_bootstrap.py` enforces no volatile under `lgpt/otg/` (except `bin/`).
**Evidence:** `tests/test_release_audio_bootstrap.py PASS` (sentinel empty, STEREO_48K, LOCAL_CONSOLE, no volatile).
**Related:** `sd_root/lgpt/otg/`, `tests/test_release_audio_bootstrap.py`

---

## DEC-2026-08-23-04 — Release publish + download-back identity

**Date:** 2026-08-23
**Status:** ACTIVE
**Scope:** Release pipeline

**Context:** Publishing without download-back could ship a different artifact than validated.
**Decision:** `ONE ARTIFACT NAME = ONE AUTHORITATIVE SHA` across GitHub body, SHA256SUMS, manifest, included-files, downloaded asset. After publish: `DOWNLOAD-BACK REQUIRED` and `REMOTE_SHA == LOCAL_SHA`.
**Reason:** Guarantees release golden = physical golden.
**Consequences:** Historical SHAs must be marked historical (see `BACON_1_5_RELEASE_MANIFEST.md`); new releases must pass download-back gate before being called golden.
**Evidence:** `REMOTE_DOWNLOAD_SHA=C5C77A...` `REMOTE_IDENTICAL=YES` `UNZIP_TEST_REMOTE PASS` `BOOTSTRAP_TEST_REMOTE PASS` `MANIFEST_CONSISTENT=YES`.
**Related:** `docs/BACON_1_5_RELEASE_MANIFEST.md`, `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt`

---

## DEC-2026-08-23-05 — SD filesystem health before runtime blame

**Date:** 2026-08-23
**Status:** ACTIVE
**Scope:** Device / filesystem / diagnostics

**Context:** Dirty exFAT caused `/mnt/sdcard` read-only, producing false USB/bootstrap failures.
**Decision:** Before blaming runtime, verify SD `mounted && healthy && writable && not read-only`. Diagnostic layers: `Detection != Runtime READY != PCM flow != physical PASS`. Repair requires explicit user authorization.
**Reason:** Filesystem failure mimics bootstrap/USB failure; distinction avoids false fixes.
**Consequences:** Agents must probe mount/options/write-probe (via `agent_preflight.sh --sd`) before kernel/audio changes.
**Evidence:** `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md` notes exFAT repaired Healthy/SD_WRITEABLE=YES before PASS.
**Related:** `device/otg_u241_common.sh`, `device/lgpt_launcher_u241.sh`, `scripts/agent_preflight.sh`

---

## DEC-2026-08-23-06 — Kernel module lifecycle (CONFIG_MODULE_UNLOAD=n)

**Date:** 2026-08-23
**Status:** ACTIVE
**Scope:** Device / kernel / audio

**Context:** Platform showed `CONFIG_MODULE_UNLOAD=n`; replacing loaded ALSA families mid-session may be impossible.
**Decision:** Agents must verify `CONFIG_MODULE_UNLOAD` before assuming hot module replace; do not hardcode experimental strict-family switch without evidence.
**Reason:** Shared ALSA modules (`snd`, `snd-pcm`, etc.) may be persistent; false assumption breaks audio host.
**Consequences:** Any family switch requires evidence and physical validation; default is shared/persistent lifecycle.
**Evidence:** `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md` H37 apply `76B50C` shared ALSA note, `CONFIG_MODULE_UNLOAD=n`.
**Related:** `device/otg_h37_apply_driver_mode.sh`, `device/AGENTS.md`

---

### Removed / migrated (not durable — history stays in Git)

- Operational pushes `999A2B27`, `3423e35`, `bdbda77`, `f3273f6`, `588270c`, `c74bd86`, `21bee8d`, `8cc0a47`, `10C9B608`, `38F8CF02`, `E9B23E36`, `DBAD57A7` etc. (DEC-2026-08-21-13..27, DEC-2026-08-21-29) — Git log is authority.
- One-off SD states `DBAD57A7` (DEC-2026-08-21-11,21,27) — manifest/SHA256SUMS is authority.
- Duplicate analyzer/BELL entries merged into DEC-04 SUPERSEDED path.
- Infrastructure checkpoint `628484c` docs-only — superseded by DEC-2026-08-23-01.
