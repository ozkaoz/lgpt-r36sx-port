#include "InstrumentEq.h"

#include <math.h>

static const float kMinGainDb = -24.0f;
static const float kMaxGainDb = 24.0f;

namespace FxEngine {

float InstrumentEq::DefaultBandHz(int band) {
    static const float kBandHz[kNumBands] = {
        80.0f, 160.0f, 320.0f, 640.0f, 1250.0f, 2500.0f, 5000.0f, 10000.0f,
    };
    if (band < 0) band = 0;
    if (band >= kNumBands) band = kNumBands - 1;
    return kBandHz[band];
}

InstrumentEq::InstrumentEq()
    : rate_(44100), bypass_(false), flat_(true), edited_(false), rtViolations_(0) {
    Reset();
}

void InstrumentEq::Reset() {
    ResetChannelState();
    bypass_ = false;
    for (int b = 0; b < kNumBands; b++) {
        bandCfg_[b].b0 = i2fp(1);
        bandCfg_[b].b1 = 0;
        bandCfg_[b].b2 = 0;
        bandCfg_[b].a1 = 0;
        bandCfg_[b].a2 = 0;
        bandCfg_[b].hz = fl2fp(DefaultBandHz(b));
        bandCfg_[b].db = 0;
        bandCfg_[b].q = fl2fp(1.0f);
        bandCfg_[b].enabled = false;
        bandCfg_[b].type = TYPE_BELL;
    }
    refreshFlat();
}

void InstrumentEq::ResetChannelState() {
    for (int c = 0; c < kMaxChannels; c++) {
        ChanState &s = state_[c];
        s.s1L = 0; s.s2L = 0;
        s.s1R = 0; s.s2R = 0;
    }
}

void InstrumentEq::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) { rtViolations_++; return; }
    rate_ = rate;
    for (int b = 0; b < kNumBands; b++) recomputeBand(b);
}

void InstrumentEq::SetBypass(bool on) { bypass_ = on; edited_ = true; refreshFlat(); }

void InstrumentEq::SetBandEnabled(int band, bool on) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    bandCfg_[band].enabled = on; edited_ = true; refreshFlat();
}

void InstrumentEq::SetBandType(int band, BandType t) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    if (t < 0 || t >= (BandType)kTypeCount) { rtViolations_++; return; }
    bandCfg_[band].type = t; edited_ = true; recomputeBand(band); refreshFlat();
}

void InstrumentEq::SetBandFreq(int band, fixed hz) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    float f = fp2fl(hz);
    if (f < 20.0f) f = 20.0f;
    if (f > 20000.0f) f = 20000.0f;
    bandCfg_[band].hz = fl2fp(f); edited_ = true; recomputeBand(band); refreshFlat();
}

void InstrumentEq::SetBandGainDb(int band, fixed db) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    float g = fp2fl(db);
    if (g < kMinGainDb) g = kMinGainDb;
    if (g > kMaxGainDb) g = kMaxGainDb;
    bandCfg_[band].db = fl2fp(g); edited_ = true; recomputeBand(band); refreshFlat();
}

void InstrumentEq::SetBandQ(int band, fixed q) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    float qf = fp2fl(q);
    if (qf < 0.1f) qf = 0.1f;
    if (qf > 10.0f) qf = 10.0f;
    bandCfg_[band].q = fl2fp(qf); edited_ = true; recomputeBand(band); refreshFlat();
}

void InstrumentEq::SetAllFlat() {
    for (int b = 0; b < kNumBands; b++) {
        bandCfg_[b].enabled = false;
        bandCfg_[b].type = TYPE_BELL;
        bandCfg_[b].db = 0;
        bandCfg_[b].q = fl2fp(1.0f);
    }
    edited_ = true;
    refreshFlat();
}

void InstrumentEq::refreshFlat() {
    if (bypass_) { flat_ = true; return; }
    bool any = false;
    for (int b = 0; b < kNumBands; b++) {
        if (!bandCfg_[b].enabled) continue;
        if (bandCfg_[b].db != 0 || (int)bandCfg_[b].type != TYPE_BELL) { any = true; break; }
    }
    flat_ = !any;
}

void InstrumentEq::recomputeBand(int band) {
    if (rate_ <= 0) return;
    BandCfg &bg = bandCfg_[band];
    float f0 = fp2fl(bg.hz);
    float lvl = fp2fl(bg.db);
    float qv = fp2fl(bg.q);
    if (qv < 0.1f) qv = 0.1f;

    float w0 = 2.0f * 3.14159265f * f0 / (float)rate_;
    if (w0 > 3.14159265f * 0.95f) w0 = 3.14159265f * 0.95f;
    if (w0 < 1e-6f) w0 = 1e-6f;
    float cw = cosf(w0);
    float sw = sinf(w0);
    float A = powf(10.0f, lvl / 40.0f);
    float alpha = sw / (2.0f * qv);

    float b0, b1, b2, a0, a1, a2;
    switch (bg.type) {
    case TYPE_LOW_SHELF: {
        float S = qv; if (S < 0.5f) S = 0.5f; if (S > 2.0f) S = 2.0f;
        float sqA = sqrtf(A);
        float ac = (sw / 2.0f) * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
        b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqA * ac);
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
        b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqA * ac);
        a0 = (A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqA * ac;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
        a2 = (A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqA * ac;
        break;
    }
    case TYPE_HIGH_SHELF: {
        float S = qv; if (S < 0.5f) S = 0.5f; if (S > 2.0f) S = 2.0f;
        float sqA = sqrtf(A);
        float as = (sw / 2.0f) * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqA * as);
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqA * as);
        a0 = (A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqA * as;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
        a2 = (A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqA * as;
        break;
    }
    case TYPE_HIGH_PASS:
        b0 = (1.0f + cw) / 2.0f;
        b1 = -(1.0f + cw);
        b2 = b0;
        a0 = 1.0f + alpha;
        a1 = -2.0f * cw;
        a2 = 1.0f - alpha;
        break;
    case TYPE_NOTCH:
        b0 = 1.0f;
        b1 = -2.0f * cw;
        b2 = 1.0f;
        a0 = 1.0f + alpha;
        a1 = -2.0f * cw;
        a2 = 1.0f - alpha;
        break;
    case TYPE_BELL:
    default:
        b0 = 1.0f + alpha * A;
        b1 = -2.0f * cw;
        b2 = 1.0f - alpha * A;
        a0 = 1.0f + alpha / A;
        a1 = -2.0f * cw;
        a2 = 1.0f - alpha / A;
        break;
    }
    if (a0 != 0.0f) {
        bg.b0 = fl2fp(b0 / a0);
        bg.b1 = fl2fp(b1 / a0);
        bg.b2 = fl2fp(b2 / a0);
        bg.a1 = fl2fp(a1 / a0);
        bg.a2 = fl2fp(a2 / a0);
    } else {
        bg.b0 = i2fp(1); bg.b1 = 0; bg.b2 = 0; bg.a1 = 0; bg.a2 = 0;
    }
}

fixed InstrumentEq::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

void InstrumentEq::Process(int channel, fixed *buffer, int frames) {
    if (frames <= 0 || !buffer) { rtViolations_++; return; }
    if (flat_) return;  // zero cost
    if (channel < 0 || channel >= kMaxChannels) { rtViolations_++; return; }

    ChanState &st = state_[channel];
    int idx = 0;
    for (int i = 0; i < frames; i++) {
        fixed xL = buffer[idx];
        fixed xR = buffer[idx + 1];
        for (int b = 0; b < kNumBands; b++) {
            const BandCfg &bg = bandCfg_[b];
            if (!bg.enabled) continue;
            fixed tL = fp_mul(bg.b0, xL) + st.s1L;
            st.s1L = fp_mul(bg.b1, xL) - fp_mul(bg.a1, tL) + st.s2L;
            st.s2L = fp_mul(bg.b2, xL) - fp_mul(bg.a2, tL);
            fixed tR = fp_mul(bg.b0, xR) + st.s1R;
            st.s1R = fp_mul(bg.b1, xR) - fp_mul(bg.a1, tR) + st.s2R;
            st.s2R = fp_mul(bg.b2, xR) - fp_mul(bg.a2, tR);
            xL = tL;
            xR = tR;
        }
        buffer[idx] = saturate(xL);
        buffer[idx + 1] = saturate(xR);
        idx += 2;
    }
}

} // namespace FxEngine