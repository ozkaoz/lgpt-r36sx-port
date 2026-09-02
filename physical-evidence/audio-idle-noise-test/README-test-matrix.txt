TreeFrogUI Audio Idle Noise - Diagnostic Test Matrix
====================================================
This package contains a temporary diagnostic picoarch for R36SX V2.6 idle audio hiss investigation.
It is NOT a permanent fix and must not be committed or pushed.

Files:
- picoarch (diagnostic, SHA 7cdff12d..., 416K, built from 543699b + diagnostic modes, FN preserved)
- audio_diag_mode.txt.example (example mode file, copy to /mnt/sdcard/cubegm/audio_diag_mode.txt and edit)
- This README

Production rollback binary (current validated FN picoarch, not diagnostic):
- D:\R36SX\fn-feature-test\v1015-plus-fn\cubegm\picoarch (SHA a61786c6..., 414K) — also at /home/dafunknoise/sf3000-work/TreeFrogUI_picoarch-fn/picoarch
- Also at D:\R36SX\safe-shutdown\safe_storage_probe etc., but for audio, rollback is to a617...

Usage:
1. Backup current production picoarch from SD: G:\cubegm\picoarch -> D:\R36SX\audio-idle-noise-test\picoarch.production.backup (verify SHA a617...)
2. Copy diagnostic picoarch to SD: D:\R36SX\audio-idle-noise-test\picoarch -> G:\cubegm\picoarch (verify SHA 7cdff12...)
3. For each test, create/edit G:\cubegm\audio_diag_mode.txt with one of: NORMAL, NO_INIT, INIT_DEINIT, ZERO_PCM, LSB8_ALT, LSB8_PRNG (exact, uppercase, no extra spaces)
4. Boot R36SX V2.6, wait for TreeFrogUI, listen for hiss with ear close to speaker, note observations per matrix below, then power off via physical button (or via safe-shutdown method if available), reinsert SD, check G:\cubegm\audio_diag_mode.txt still correct, and check logs if any.

Test Matrix (to be filled by human observation, do not fabricate):
---------------------------------------------------------------
TEST NORMAL
  HISS_IDLE= (YES/NO, description)
  HISS_VOLUME_0= (if volume 0)
  HISS_VOLUME_LOW=
  HISS_VOLUME_HIGH=

TEST NO_INIT
  TREEFROGUI_BOOT= (PASS/FAIL)
  HISS_IDLE= (expected NO if driver is cause)

TEST INIT_DEINIT
  HISS_BEFORE_DEINIT= (first 3s)
  HISS_AFTER_DEINIT= (after deinit, should be NO if driver is cause)
  INIT_POP= (YES/NO, click at init)
  DEINIT_POP= (YES/NO)

TEST ZERO_PCM
  HISS_ZERO_PCM= (during 5s zero feed)
  HISS_AFTER_ZERO_FEED= (after)

TEST LSB8_ALT
  LSB8_AUDIBLE= (YES/NO)
  LSB8_RESEMBLES_IDLE= (YES/NO)
  LSB8_LOUDER_THAN_IDLE= (YES/NO)

TEST LSB8_PRNG
  LSB8_AUDIBLE=
  LSB8_RESEMBLES_IDLE=
  LSB8_LOUDER_THAN_IDLE=

Notes:
- All tests are on same audio thread, no second driver owner, diagnostic mode read once at startup and fixed until restart.
- NORMAL must be byte-for-byte production behavior when mode file is NORMAL or missing.
- Do not modify stock OS files, do not patch driver_r36sx.so, do not change zhijack.sh.
- After each test, power off via physical button after a few seconds, reinsert SD, and proceed to next mode.

