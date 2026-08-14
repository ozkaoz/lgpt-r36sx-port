#ifndef _PITCH_ENVELOPE_TOOL_H_
#define _PITCH_ENVELOPE_TOOL_H_

#include <math.h>
#include <stdlib.h>

/*
 * F3-2 (docs/F3_ARCHITECTURE_ES.md): parametros pitch/env y buffer builder
 * del Chopper extraidos del SampleChopperModal como capa pura
 * (header-only).
 *
 * Regla de oro: algoritmos identicos a Bacon 1.2.1 (mismos clamps, misma
 * constante, mismo orden de escrituras).  El DSP puro (resample con
 * interpolacion lineal + envolvente con ataque/sustain/release) se proba
 * contra el golden en tests/host/pitch_tool_host_test.cpp.
 */

#define LGPT_PITCH_MIN_SEMITONES (-12)
#define LGPT_PITCH_MAX_SEMITONES (12)
#define LGPT_PITCH_MIN_ATTACK_MS (0)
#define LGPT_PITCH_MAX_ATTACK_MS (5000)
#define LGPT_PITCH_MIN_SUSTAIN_PERCENT (0)
#define LGPT_PITCH_MAX_SUSTAIN_PERCENT (150)
#define LGPT_PITCH_MIN_RELEASE_MS (0)
#define LGPT_PITCH_MAX_RELEASE_MS (5000)
#define LGPT_PITCH_PARAM_COUNT (6)
#define LGPT_PITCH_MAX_PREVIEW_FRAMES (40000000)
#define LGPT_PITCH_NUDGE_STEP_MS (5)

struct PitchEnvelopeParams {
    int semitones;
    int attackMs;
    int sustainPercent;
    int releaseMs;
    int scope; /* 0 = sample, 1 = chop */

    PitchEnvelopeParams()
        : semitones(0), attackMs(0), sustainPercent(100), releaseMs(0),
          scope(0) {}
};

class PitchEnvelopeTool {
public:
    PitchEnvelopeTool() : editParam_(0) {}

    /* ---- parametros (con los clamps golden) ---- */
    void Reset() {
        params_.semitones = 0;
        params_.attackMs = 0;
        params_.sustainPercent = 100;
        params_.releaseMs = 0;
        params_.scope = 0;
        editParam_ = 0;
    }

    bool HasChange() const {
        return params_.semitones != 0 || params_.attackMs != 0 ||
               params_.releaseMs != 0 || params_.sustainPercent != 100;
    }

    const PitchEnvelopeParams &Params() const { return params_; }

    /* Parametro de edicion activo (0 = Pitch .. 5 = Sample).  Golden:
       selectPitchEditParam clampa a [0, 5]. */
    int EditParam() const { return editParam_; }
    void SetEditParam(int v) {
        editParam_ = ClampInt(v, 0, LGPT_PITCH_PARAM_COUNT - 1);
    }
    void NudgeEditParam(int delta) { SetEditParam(editParam_ + delta); }

    void SetSemitones(int v) {
        params_.semitones = ClampInt(v, LGPT_PITCH_MIN_SEMITONES,
                                     LGPT_PITCH_MAX_SEMITONES);
    }
    void SetAttackMs(int v) {
        params_.attackMs = ClampInt(v, LGPT_PITCH_MIN_ATTACK_MS,
                                    LGPT_PITCH_MAX_ATTACK_MS);
    }
    void SetSustainPercent(int v) {
        params_.sustainPercent = ClampInt(v, LGPT_PITCH_MIN_SUSTAIN_PERCENT,
                                          LGPT_PITCH_MAX_SUSTAIN_PERCENT);
    }
    void SetReleaseMs(int v) {
        params_.releaseMs = ClampInt(v, LGPT_PITCH_MIN_RELEASE_MS,
                                     LGPT_PITCH_MAX_RELEASE_MS);
    }
    void SetScope(int v) { params_.scope = v ? 1 : 0; }

    /* Nudge golden: el paso del envelope es 5 unidades (ms o %); el de
       semitones es 1.  Devuelve el nuevo valor (ya clampeado). */
    int NudgeSemitones(int delta) {
        SetSemitones(params_.semitones + delta);
        return params_.semitones;
    }
    int NudgeAttackMs(int delta) {
        SetAttackMs(params_.attackMs + delta * LGPT_PITCH_NUDGE_STEP_MS);
        return params_.attackMs;
    }
    int NudgeSustainPercent(int delta) {
        SetSustainPercent(params_.sustainPercent +
                          delta * LGPT_PITCH_NUDGE_STEP_MS);
        return params_.sustainPercent;
    }
    int NudgeReleaseMs(int delta) {
        SetReleaseMs(params_.releaseMs + delta * LGPT_PITCH_NUDGE_STEP_MS);
        return params_.releaseMs;
    }
    void ToggleScope() { params_.scope = params_.scope ? 0 : 1; }

    /* ---- DSP puro (golden, sin acceso a SamplePool) ---- */

    /*
     * Golden applyEnvelopeToBuffer: aplica ataque lineal (0..1), sustain
     * constante y release lineal sobre cada canal, saturando a int16.
     * Devuelve false si los argumentos son invalidos.
     */
    static bool ApplyEnvelope(short *samples, int frames, int channels,
                              int rate, int attackMs, int sustainPercent,
                              int releaseMs) {
        if (!samples || frames <= 0 || channels <= 0 || rate <= 0)
            return false;
        attackMs = ClampInt(attackMs, LGPT_PITCH_MIN_ATTACK_MS,
                            LGPT_PITCH_MAX_ATTACK_MS);
        releaseMs = ClampInt(releaseMs, LGPT_PITCH_MIN_RELEASE_MS,
                             LGPT_PITCH_MAX_RELEASE_MS);
        sustainPercent = ClampInt(sustainPercent,
                                  LGPT_PITCH_MIN_SUSTAIN_PERCENT,
                                  LGPT_PITCH_MAX_SUSTAIN_PERCENT);

        int attackFrames = (int)(((long long)attackMs * (long long)rate) /
                                 1000LL);
        int releaseFrames = (int)(((long long)releaseMs * (long long)rate) /
                                  1000LL);
        if (attackFrames > frames) attackFrames = frames;
        if (releaseFrames > frames) releaseFrames = frames;

        double sustain = ((double)sustainPercent) / 100.0;
        for (int i = 0; i < frames; i++) {
            double gain = sustain;
            if (attackFrames > 0 && i < attackFrames) {
                gain *= (double)i / (double)attackFrames;
            }
            if (releaseFrames > 0 && i >= frames - releaseFrames) {
                int remain = frames - 1 - i;
                double rel = (remain <= 0) ? 0.0
                                           : ((double)remain /
                                              (double)releaseFrames);
                if (rel < 0.0) rel = 0.0;
                if (rel > 1.0) rel = 1.0;
                gain *= rel;
            }
            for (int ch = 0; ch < channels; ch++) {
                int v = (int)((double)samples[i * channels + ch] * gain);
                if (v < -32768) v = -32768;
                if (v > 32767) v = 32767;
                samples[i * channels + ch] = (short)v;
            }
        }
        return true;
    }

    /*
     * Golden resample lineal (buildPitchEnvelopeBufferFromRange): cambia
     * el pitch por razon 2^(semitones/12), con interpolacion lineal,
     * saturacion int16 y envolvente opcional en un solo paso.  El
     * buffer de salida se asigna con malloc y el llamador lo libera.
     * Devuelve false si los argumentos son invalidos o el tamaño de
     * salida excede LGPT_PITCH_MAX_PREVIEW_FRAMES.
     */
    static bool BuildPitchedRange(const short *src, int srcSize,
                                  int channels, int startFrame, int endFrame,
                                  int semitones, int attackMs,
                                  int sustainPercent, int releaseMs, int rate,
                                  short **outSamples, int *outFrames) {
        if (outSamples) *outSamples = 0;
        if (outFrames) *outFrames = 0;
        if (!src || srcSize <= 1 || channels <= 0) return false;
        if (semitones < LGPT_PITCH_MIN_SEMITONES)
            semitones = LGPT_PITCH_MIN_SEMITONES;
        if (semitones > LGPT_PITCH_MAX_SEMITONES)
            semitones = LGPT_PITCH_MAX_SEMITONES;

        if (startFrame < 0) startFrame = 0;
        if (endFrame > srcSize - 1) endFrame = srcSize - 1;
        if (endFrame < startFrame) endFrame = startFrame;
        int rangeFrames = endFrame - startFrame + 1;
        if (rangeFrames <= 1) return false;

        double ratio = pow(2.0, ((double)semitones) / 12.0);
        if (ratio <= 0.0) return false;
        int nextSize = (int)(((double)rangeFrames / ratio) + 0.5);
        if (nextSize < 2) nextSize = 2;
        if (nextSize > LGPT_PITCH_MAX_PREVIEW_FRAMES) return false;

        short *pitched = (short *)malloc((size_t)nextSize * (size_t)channels *
                                         sizeof(short));
        if (!pitched) return false;

        for (int i = 0; i < nextSize; i++) {
            double srcPos = (double)i * ratio;
            int idx = (int)srcPos;
            double frac = srcPos - (double)idx;
            if (idx < 0) idx = 0;
            if (idx >= rangeFrames - 1) {
                idx = rangeFrames - 1;
                frac = 0.0;
            }
            int idx2 = idx + 1;
            if (idx2 >= rangeFrames) idx2 = rangeFrames - 1;
            int srcIdx = startFrame + idx;
            int srcIdx2 = startFrame + idx2;
            for (int ch = 0; ch < channels; ch++) {
                int a = src[srcIdx * channels + ch];
                int b = src[srcIdx2 * channels + ch];
                int v = (int)((double)a + ((double)(b - a) * frac));
                if (v < -32768) v = -32768;
                if (v > 32767) v = 32767;
                pitched[i * channels + ch] = (short)v;
            }
        }

        /* Envolvente en el buffer ya resampleado (golden:
           applyEnvelopeToBuffer despues del resample, con el rate del
           source). */
        ApplyEnvelope(pitched, nextSize, channels, rate, attackMs,
                      sustainPercent, releaseMs);

        if (outSamples) *outSamples = pitched;
        if (outFrames) *outFrames = nextSize;
        return true;
    }

    /* Nombre del parametro de edicion (golden selectPitchEditParam): 0
       Pitch, 1 Attack, 2 Sustain, 3 Release, 4 Scope, 5 Sample. */
    static const char *ParamName(int param) {
        switch (param) {
            case 1: return "Attack";
            case 2: return "Sustain";
            case 3: return "Release";
            case 4: return "Scope";
            case 5: return "Sample";
            default: return "Pitch";
        }
    }

private:
    static int ClampInt(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    PitchEnvelopeParams params_;
    int editParam_;
};

#endif