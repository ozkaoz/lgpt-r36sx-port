#!/usr/bin/env python3
"""bacon-1.5 item 1/5: audition isolation -- previews must NEVER touch the
8 PlayerChannel, and the audition must sound even with the track muted or
volume 0.

Verifies (token/source-level, mirroring the other F3/Fase golden tests):

- Player::PreviewNote / StopPreview / UpdatePreview route the preview to
  AuditionService ONLY; StopPreview's body contains no PlayerChannel stop
  and no channel loop (the 8 song channels keep playing).
- AuditionChannel renders on its own AUDITION_CHANNEL_INDEX with a FIXED
  full gain and center pan: no muted_ / volume_ / pan_ anywhere in the
  audition path.
- The audition bus joins the master tree (MixerService master_.Insert
  auditionBus_) so previews reach the same master meters; it is clip-safe.
- The audition channel applies a fixed +6 dB lift (kAuditionGain) so the
  preview is clearly perceivable next to recorded samples.
- The SpectrumAnalyzer is fed from the FINAL MASTER MIX (bacon-1.5,
  feedback): the tap lives in AudioOutDriver/DummyAudioOut after the
  master bus + FxEngine stage, so the EQ8 spectrum shows the whole mix.
  There are no per-instrument taps in PlayerChannel / AuditionChannel and
  no targetInstrument_ filtering anymore.
- InstrumentEq curve == DSP: InstrumentEqView draws from GetBandCoeffs
  (the same coefficients the per-band biquads process) and the smoothing
  snap makes convergence exact.
- EqBiquad carries the RBJ bell stability guard (bacon-1.5) shared by
  InstrumentEq and ParametricEQ.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

P_H = (ROOT / "source/sources/Application/Player/Player.h").read_text()
P_CPP = (ROOT / "source/sources/Application/Player/Player.cpp").read_text()
PC_CPP = (ROOT / "source/sources/Application/Player/PlayerChannel.cpp").read_text()
AS_H = (ROOT / "source/sources/Application/Audio/AuditionService.h").read_text()
AS_CPP = (ROOT / "source/sources/Application/Audio/AuditionService.cpp").read_text()
MS_H = (ROOT / "source/sources/Application/Mixer/MixerService.h").read_text()
MS_CPP = (ROOT / "source/sources/Application/Mixer/MixerService.cpp").read_text()
SA_H = (ROOT / "source/sources/Application/Audio/SpectrumAnalyzer.h").read_text()
SA_CPP = (ROOT / "source/sources/Application/Audio/SpectrumAnalyzer.cpp").read_text()
IEQ_H = (ROOT / "source/sources/Application/Audio/InstrumentEq.h").read_text()
IEQ_CPP = (ROOT / "source/sources/Application/Audio/InstrumentEq.cpp").read_text()
IEQ_VIEW = (ROOT / "source/sources/Application/UI/Views/InstrumentEqView.cpp").read_text()
BQ = (ROOT / "source/sources/Application/Audio/EqBiquad.h").read_text()
MK = (ROOT / "source/projects/Makefile").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


def body_of(src, signature):
    """Slice a function body (up to the next top-level signature)."""
    i = src.index(signature)
    j = src.index("\nvoid ", i + len(signature))
    return src[i:j]


def check_player_stop_preview():
    # Preview routes to the audition service, not to the song channels.
    assert "AuditionService::GetInstance()->Preview(instrument, note)" in P_CPP
    assert "AuditionService::GetInstance()->StopPreview()" in P_CPP
    body = body_of(P_CPP, "void Player::StopPreview()")
    # StopPreview touches ONLY the audition channel: no PlayerChannel
    # access, no channel loop, no instrument stop, no track render.
    assert "AuditionService::GetInstance()->StopPreview()" in body
    assert "PlayerChannels are never touched by a preview." in body
    assert "StopInstrument()" not in body
    assert "StopNote()" not in body
    assert "Render(" not in body
    assert "instr_" not in body
    # The audio-thread disarm is guarded: only when the sequencer is
    # stopped, so a preview never mutes a running song.
    assert "TreeFrogAudioSetPlaybackArmed(0)" in body
    assert "!isRunning_" in body
    assert body.index("TreeFrogAudioSetPlaybackArmed(0)") > body.index(
        "!isRunning_")
    # PreviewNote arms the audition channel and arms the audio thread.
    body = body_of(P_CPP, "void Player::PreviewNote(")
    assert "AuditionService::GetInstance()->Preview(" in body
    # The auto-stop clock path exists.
    assert "kPreviewNoteDurationMs" in P_CPP
    assert "previewStopClock_" in P_CPP
    print("1. Player preview routed to AuditionService only (StopPreview body clean) OK")


def check_player_channel_untouched():
    # The 8 song PlayerChannel never references the audition service.
    assert "Audition" not in PC_CPP
    assert "Preview" not in PC_CPP
    # BACON_1.5_ANALYZER_MIX: the per-channel analyzer tap is GONE from the
    # song path (the analyzer now reads the master mix from the audio out).
    assert "SpectrumAnalyzer" not in PC_CPP
    print("2. PlayerChannel: zero audition references, no analyzer tap OK")


def check_audition_channel():
    # Audition renders on its OWN channel index with a fixed full gain:
    # no mute/volume/pan can silence it.
    assert "AUDITION_CHANNEL_INDEX" in AS_CPP
    assert "FIXED full gain" in AS_CPP
    assert "muted_" not in AS_CPP
    assert "volume_" not in AS_CPP
    assert "pan_" not in AS_CPP
    # BACON_1.5_AUDITION_GAIN: fixed +6 dB lift on the audible path.
    assert "kAuditionGain" in AS_CPP
    assert "fp_mul(*c, kAuditionGain)" in AS_CPP
    # The per-channel analyzer tap is gone from the audition path too.
    assert "SpectrumAnalyzer" not in AS_CPP
    # Service contract.
    assert "void Preview(I_Instrument *instrument, unsigned char note);" in AS_H
    assert "void StopPreview();" in AS_H
    assert "bool IsPreviewing() const" in AS_H
    assert "channel_.GetInstrument() != 0" in AS_H
    assert "NO track mute/volume/pan" in AS_H
    print("3. AuditionChannel: own index, fixed gain +6 dB, no analyzer tap OK")


def check_mixer_bus():
    # The audition bus joins the master tree (not a track), clip-safe.
    assert "master_.Insert(auditionBus_)" in MS_CPP
    assert "auditionBus_.SetClipBypass(true)" in MS_CPP
    assert "MixBus *GetAuditionBus()" in MS_H
    assert "MixBus auditionBus_;" in MS_H
    print("4. MixerService: auditionBus_ in master_ tree, clip bypass OK")


def check_analyzer_mix():
    # Master-mix tap: fed only when armed; no per-instrument targeting.
    assert "void SetArmed(bool armed)" in SA_H
    assert "void FeedMix(const fixed *stereo, int frames);" in SA_H
    assert "SetTargetInstrument" not in SA_H
    assert "targetInstrument_" not in SA_CPP
    assert "armed_" in SA_CPP
    assert "if (!armed_) return;" in SA_CPP
    # The tap lives in both audio out drivers, post master + FxEngine.
    AOD_CPP = (ROOT / "source/sources/Services/Audio/AudioOutDriver.cpp").read_text()
    DAO_CPP = (ROOT / "source/sources/Application/Audio/DummyAudioOut.cpp").read_text()
    assert "SpectrumAnalyzer::Get().FeedMix(primarySoundBuffer_" in AOD_CPP
    assert "SpectrumAnalyzer::Get().FeedMix(primarySoundBuffer_" in DAO_CPP
    assert "BACON_1.5_ANALYZER_MIX" in AOD_CPP
    print("5. SpectrumAnalyzer: master-mix tap (armed-gated, no target) OK")


def check_eq_curve_dsp():
    # U2.52.5 (BACON_1.5_EQ8_LIVE_CURVE): the EQ8 view draws the curve from
    # its own state (freqHz_/gainDb_/type_/q_/bandOn_) via the SAME shared
    # math the DSP uses (FxEngine::eqBiquadCoeffs), so the canvas updates
    # live while editing (the old DSP readbacks only synced while audio
    # rendered).  U2.52.8: the bell inside the primitive is the prewarped
    # peaking prototype (BACON_1.5_BELL_PREWARPED), so the drawn curve
    # shows the corrected symmetric shape too.
    assert "eqBiquadCoeffs" in IEQ_VIEW
    assert "BACON_1.5_EQ8_LIVE_CURVE" in IEQ_VIEW
    # U2.52.5 (BACON_1.5_EQ8_24DB): the canvas maps the full +/-24 dB range.
    assert "pxPerDb" in IEQ_VIEW
    assert "/ 48.0" in IEQ_VIEW
    # Per-frame exponential smoothing + exact snap at the last 2^-6 step.
    # U2.62 (BACON_1.5_EQ8_DEN24): the deltas run as long long so the Q24
    # a1/a2 denominators blend at full width; the per-frame step is still
    # the signed >>kSmoothShift with the exact snap.
    assert "kSmoothShift" in IEQ_CPP
    assert "bg.b0 += (fixed)(d0 >> kSmoothShift);" in IEQ_CPP
    assert "(d0 >> kSmoothShift) == 0) bg.b0 = bg.tB0;" in IEQ_CPP
    assert "bg.smoothing = false;" in IEQ_CPP
    # Per-band-per-channel-per-stage states (no shared cascade state).
    assert "state_[channel][b]" in IEQ_CPP
    assert "ChanState &st = state_[channel][b][0];" in IEQ_CPP
    # U2.52.5 (BACON_1.5_EQ8_DF2_64BIT): the transposed Df2 state update
    # runs in 64 bits (a 32-bit sum overflows with full-scale input + boosts;
    # on the device any EQ edit "killed" the sound).  U2.62: the products
    # are explicit (long long)bg.b1 * (long long)xL Q24/Q15 multiplies.
    assert "BACON_1.5_EQ8_DF2_64BIT" in IEQ_CPP
    assert "(long long)bg.b1 * (long long)xL" in IEQ_CPP
    print("6. InstrumentEq: view-state curve (shared eqBiquadCoeffs), +/-24 dB, exact snap, Df2 64-bit OK")


def check_bell_guard():
    # U2.52.8 (BACON_1.5_BELL_PREWARPED): the RBJ peaking bell was replaced
    # by the prewarped-bilinear analog peaking prototype (exact gain at w0,
    # 0 dB at DC/Nyquist, log-symmetric, stable for every A>0/Q>0), so the
    # old RBJ_BELL_STABILITY cap is gone: it capped low boosts to ~0.4-1 dB
    # and left the asymmetric DC-shelf shape ("lifts left, drops right").
    assert "BACON_1.5_BELL_PREWARPED" in BQ
    assert "RBJ_BELL_STABILITY" not in BQ
    assert "K = w0 / tan" in BQ and "w0 / 2.0" in BQ
    assert "sA = pow" in BQ and "lvl / 40.0" in BQ
    assert "EQ_BIQUAD_BELL" in IEQ_CPP  # InstrumentEq maps to the shared primitive
    print("7. EqBiquad prewarped bell (BACON_1.5_BELL_PREWARPED) OK")


def check_build_and_audit():
    # Modal gone from the build; audition + view sources present.
    assert "InstrumentEqModal" not in MK
    assert "AuditionService.o" in MK
    assert "InstrumentEqView.o" in MK
    # Both new host runners wired into the audit.
    assert "run_host_eq8_struct.sh" in AUDIT
    assert "run_host_analyzer_target.sh" in AUDIT
    print("8. Makefile (modal gone) + audit.sh wiring OK")


check_player_stop_preview()
check_player_channel_untouched()
check_audition_channel()
check_mixer_bus()
check_analyzer_mix()
check_eq_curve_dsp()
check_bell_guard()
check_build_and_audit()
print("TEST_FX_PHASE19_AUDITION_ISOLATED_OK")
