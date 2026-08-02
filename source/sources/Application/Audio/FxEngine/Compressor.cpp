#include "Compressor.h"
#include <math.h>

namespace FxEngine {

// Bypass crossfade and GR meter smoothing (one-pole per Process call).
#define FX_COMP_MIX_SMOOTH fl2fp(0.005f)
#define FX_COMP_GR_SMOOTH  fl2fp(0.005f)

Compressor::Compressor()
    : attK_(fl2fp(0.5f)), relK_(fl2fp(0.1f)),
      grMeter_(0), threshDb_(fl2fp(-24.0f)), ratio_(fl2fp(4.0f)),
      kneeDb_(fl2fp(6.0f)), makeupDb_(0),
      attackMs_(15.0f), releaseMs_(200.0f), rate_(44100),
      stereoLink_(true), bypass_(true), softClip_(true), rtViolations_(0) {
    level_[0] = level_[1] = 0;
    recomputeSmoothing();
    recomputeTable();
}

void Compressor::Reset() {
    level_[0] = level_[1] = 0;
    grMeter_ = 0;
    recomputeSmoothing();
    recomputeTable();
}

void Compressor::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) {
        ++rtViolations_;
        return;
    }
    rate_ = rate;
    recomputeSmoothing();
    recomputeTable();
}

void Compressor::SetThresholdDb(fixed db) {
    float d = fp2fl(db);
    if (d < -60.0f) d = -60.0f;
    if (d > 0.0f) d = 0.0f;
    threshDb_ = fl2fp(d);
    recomputeTable();
}

void Compressor::SetRatio(fixed ratio) {
    float r = fp2fl(ratio);
    if (r < 1.0f) r = 1.0f;
    if (r > 20.0f) r = 20.0f;
    ratio_ = fl2fp(r);
    recomputeTable();
}

void Compressor::SetKneeDb(fixed db) {
    float d = fp2fl(db);
    if (d < 0.0f) d = 0.0f;
    if (d > 12.0f) d = 12.0f;
    kneeDb_ = fl2fp(d);
    recomputeTable();
}

void Compressor::SetAttackMs(fixed ms) {
    float m = fp2fl(ms);
    if (m < 0.1f) m = 0.1f;
    if (m > 500.0f) m = 500.0f;
    attackMs_ = m;
    recomputeSmoothing();
}

void Compressor::SetReleaseMs(fixed ms) {
    float m = fp2fl(ms);
    if (m < 1.0f) m = 1.0f;
    if (m > 2000.0f) m = 2000.0f;
    releaseMs_ = m;
    recomputeSmoothing();
}

void Compressor::SetMakeupDb(fixed db) {
    float d = fp2fl(db);
    if (d < 0.0f) d = 0.0f;
    if (d > 24.0f) d = 24.0f;
    makeupDb_ = fl2fp(d);
    recomputeTable();
}

void Compressor::SetStereoLink(bool on) { stereoLink_ = on; }
void Compressor::SetBypass(bool on) { bypass_ = on; }
void Compressor::SetSoftClip(bool on) { softClip_ = on; }

fixed Compressor::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

// Standard cubic soft clip: y = 1.5x - 0.5x^3 for |x| <= 1, else sign(x).
fixed Compressor::cubicClip(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    fixed x2 = fp_mul(x, x);
    fixed x3 = fp_mul(x2, x);   // x^3
    fixed y = fp_mul(x, fl2fp(1.5f)) - fp_mul(x3, fl2fp(0.5f));
    return saturate(y);
}

// One-pole attack/release coefficients (control-rate only).
void Compressor::recomputeSmoothing() {
    // attackMs_ / releaseMs_ set in ctor before recomputeSmoothing()
    if (attackMs_ <= 0.0f) attackMs_ = 0.1f;
    if (releaseMs_ <= 0.0f) releaseMs_ = 1.0f;
    float att = 1.0f - expf(-1000.0f / (attackMs_ * (float)rate_));
    float rel = 1.0f - expf(-1000.0f / (releaseMs_ * (float)rate_));
    attK_ = fl2fp(att);
    relK_ = fl2fp(rel);
}

// Precompute level -> gain / GR tables (control-rate only).
// Level index i in [0, kTableSize): level = i / kTableSize (Q15 [0,1]).
// dB gain computer: g_dB = -20*log10(level/levelRef) ... standard soft knee:
//   over = 20*log10(level) - threshold
//   if   over <  -knee/2 : gain = 1 (0 dB)
//   elif over >  +knee/2 : gain = threshold - over*(1/ratio-1)... (standard)
//   else (soft knee)     : polynomial interpolation
void Compressor::recomputeTable() {
    float thr = fp2fl(threshDb_);
    float ratio = fp2fl(ratio_);
    if (ratio < 1.0f) ratio = 1.0f;
    float knee = fp2fl(kneeDb_);
    float makeup = fp2fl(makeupDb_);

    for (int i = 0; i < kTableSize; i++) {
        float level = (float)(i + 1) / (float)kTableSize;  // avoid 0 log
        if (level < 1e-4f) level = 1e-4f;
        float levelDb = 20.0f * log10f(level);
        float over = levelDb - thr;
        float gr = 0.0f;

        if (knee <= 0.0f) {
            if (over > 0.0f) gr = over * (1.0f / ratio - 1.0f);
        } else {
            if (over > knee) {
                gr = over * (1.0f / ratio - 1.0f);
            } else if (over > 0.0f) {
                // soft knee: ((over + knee)^2) / (2*knee) * (1/R - 1)
                float t = over + knee;
                gr = (t * t) / (2.0f * knee) * (1.0f / ratio - 1.0f);
            }
        }

        float gainDb = gr + makeup;
        float gain = powf(10.0f, gainDb / 20.0f);
        if (gain > 4.0f) gain = 4.0f;   // safety (won't happen at defaults)
        gainTable_[i] = fl2fp(gain);
        grTable_[i] = fl2fp(gr);
    }
}

void Compressor::Process(const fixed *in, fixed *out, int frames) {
    if (frames <= 0 || !in || !out) {
        ++rtViolations_;
        return;
    }

    int idx = 0;
    for (int i = 0; i < frames; i++) {
        fixed xL = in[idx];
        fixed xR = in[idx + 1];

        // Detector: stereo link = max(|L|,|R|), else per-channel.
        fixed detL = (xL > 0) ? xL : -xL;
        fixed detR = (xR > 0) ? xR : -xR;
        fixed det = detL;
        if (stereoLink_) {
            if (detR > det) det = detR;
        } else {
            // process both channels separately; keep det = detL for now and
            // handle R below with its own envelope/table lookup.
            det = detL;
        }

        // Table index from envelope level (Q15 [0,1]).
        int idxL = (int)(level_[0] >> (15 - kTableBits));
        if (idxL < 0) idxL = 0;
        if (idxL >= kTableSize) idxL = kTableSize - 1;
        int idxR = idxL;
        if (!stereoLink_) {
            idxR = (int)(level_[1] >> (15 - kTableBits));
            if (idxR < 0) idxR = 0;
            if (idxR >= kTableSize) idxR = kTableSize - 1;
        }

        fixed gainL = gainTable_[idxL];
        fixed gainR = (stereoLink_) ? gainL : gainTable_[idxR];

        // Update envelopes (attack = rise, release = fall).
        if (stereoLink_) {
            if (det > level_[0]) {
                level_[0] = level_[0] + fp_mul(attK_, det - level_[0]);
            } else {
                level_[0] = level_[0] + fp_mul(relK_, det - level_[0]);
            }
            level_[1] = level_[0];
        } else {
            if (det > level_[0]) {
                level_[0] = level_[0] + fp_mul(attK_, det - level_[0]);
            } else {
                level_[0] = level_[0] + fp_mul(relK_, det - level_[0]);
            }
            if (detR > level_[1]) {
                level_[1] = level_[1] + fp_mul(attK_, detR - level_[1]);
            } else {
                level_[1] = level_[1] + fp_mul(relK_, detR - level_[1]);
            }
        }

        // Bypass crossfade toward unity gain (smoothed per sample).
        fixed unity = i2fp(1);
        if (bypass_) { gainL = unity; gainR = unity; }
        fixed outL = fp_mul(xL, gainL);
        fixed outR = fp_mul(xR, gainR);
        if (softClip_) { outL = cubicClip(outL); outR = cubicClip(outR); }

        out[idx] = saturate(outL);
        out[idx + 1] = saturate(outR);
        idx += 2;
    }

    // GR meter: smooth the reduction of the linked channel toward the meter.
    int g = (int)(level_[0] >> (15 - kTableBits));
    if (g < 0) g = 0;
    if (g >= kTableSize) g = kTableSize - 1;
    fixed targetGr = bypass_ ? 0 : grTable_[g];
    grMeter_ = grMeter_ + fp_mul(FX_COMP_GR_SMOOTH, targetGr - grMeter_);
}

} // namespace FxEngine
