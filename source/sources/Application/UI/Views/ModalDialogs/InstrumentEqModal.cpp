#include "InstrumentEqModal.h"

#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#include "Application/Audio/InstrumentEq.h"
#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/UI/Views/ViewData.h"
#include "UIFramework/BasicDatas/GUIEvent.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Shared constants
// ---------------------------------------------------------------------------
static const char *kEqTypeNames[6] = {
    "BELL", "LOWSH", "HISHE", "LOWPA", "HIPAS", "NOTCH",
};

static const FourCC kFreqIDs[8] = {SIP_EQF0, SIP_EQF1, SIP_EQF2, SIP_EQF3,
                                   SIP_EQF4, SIP_EQF5, SIP_EQF6, SIP_EQF7};
static const FourCC kGainIDs[8] = {SIP_EQG0, SIP_EQG1, SIP_EQG2, SIP_EQG3,
                                   SIP_EQG4, SIP_EQG5, SIP_EQG6, SIP_EQG7};
static const FourCC kTypeIDs[8] = {SIP_EQT0, SIP_EQT1, SIP_EQT2, SIP_EQT3,
                                   SIP_EQT4, SIP_EQT5, SIP_EQT6, SIP_EQT7};
static const FourCC kQIDs[8]    = {SIP_EQ_Q0, SIP_EQ_Q1, SIP_EQ_Q2, SIP_EQ_Q3,
                                   SIP_EQ_Q4, SIP_EQ_Q5, SIP_EQ_Q6, SIP_EQ_Q7};

static const float kDefaultFreq8[8] = {
    80.f, 160.f, 320.f, 640.f, 1250.f, 2500.f, 5000.f, 10000.f,
};

namespace {

// Shared state consumed by the AppWindow flush hook below.
int EQOverlayActive = 0;
int EQOverlayHold = 0;
int EQSelected = 0;
int EQEnable = 1;
int EQType[8]   = {0, 0, 0, 0, 0, 0, 0, 0};
int EQBandOn[8] = {1, 1, 1, 1, 1, 1, 1, 1};
float EQFreq[8] = {80.f, 160.f, 320.f, 640.f, 1250.f, 2500.f, 5000.f, 10000.f};
float EQGain[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float EQQ[8]    = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

}  // namespace

#if defined(PLATFORM_TREEFROG)

namespace {

static unsigned short tf565(unsigned char r, unsigned char g, unsigned char b) {
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void tfFill(int x, int y, int w, int h, unsigned short c) {
    if (w <= 0 || h <= 0) return;
    uint16_t *fb = TreeFrogGetFramebuffer();
    if (!fb) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= 320 || y >= 240) return;
    if (x + w > 320) w = 320 - x;
    if (y + h > 240) h = 240 - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        uint16_t *dst = fb + yy * 320 + x;
        for (int xx = 0; xx < w; xx++) *dst++ = c;
    }
}

// RBJ biquad coefficients, a0 normalized.  (UI/flush path only.)
static void eqCoeffs(double f0, double gDb, double q, int type, int rate,
                     double &b0, double &b1, double &b2,
                     double &a1, double &a2) {
    double w0 = 2.0 * 3.14159265358979323846 * f0 / rate;
    if (w0 < 1e-6) w0 = 1e-6;
    double cw = cos(w0), sw = sin(w0);
    if (gDb > 24.0) gDb = 24.0;
    if (gDb < -24.0) gDb = -24.0;
    double A = pow(10.0, gDb / 40.0);
    double qq = q < 0.1 ? 0.1 : q;
    double alpha = sw / (2.0 * qq);
    double a0;
    switch (type) {
    case 3: b0 = (1.0 - cw) / 2.0; b1 = 1.0 - cw; b2 = b0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha; break;
    case 4: b0 = (1.0 + cw) / 2.0; b1 = -(1.0 + cw); b2 = b0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha; break;
    case 5: b0 = 1.0; b1 = -2.0 * cw; b2 = 1.0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha; break;
    case 1: case 2: {
        double S = q; if (S < 0.5) S = 0.5; if (S > 2.0) S = 2.0;
        double sqA = sqrt(A);
        double asx = (sw / 2.0) * sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        if (type == 1) {
            b0 = A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * sqA * asx);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sqA * asx);
            a0 = (A + 1.0) + (A - 1.0) * cw + 2.0 * sqA * asx;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            a2 = (A + 1.0) + (A - 1.0) * cw - 2.0 * sqA * asx;
        } else {
            b0 = A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * sqA * asx);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2 = A * ((A + 1.0) + (A - 1.0) * cw - 2.0 * sqA * asx);
            a0 = (A + 1.0) - (A - 1.0) * cw + 2.0 * sqA * asx;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
            a2 = (A + 1.0) - (A - 1.0) * cw - 2.0 * sqA * asx;
        }
        break;
    }
    case 0:
    default:
        b0 = 1.0 + alpha * A; b1 = -2.0 * cw; b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A; a1 = -2.0 * cw; a2 = 1.0 - alpha / A;
        break;
    }
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;
}

static int freqToX(double f) {
    if (f < 20.0) f = 20.0;
    if (f > 20000.0) f = 20000.0;
    double r = (log10(f) - log10(20.0)) / (log10(20000.0) - log10(20.0));
    return 6 + (int)(r * 308.0);
}

static double xToFreq(int x) {
    double r = (double)(x - 6) / 308.0;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    return 20.0 * pow(1000.0, r);
}

}  // namespace

// ---------------------------------------------------------------------------
// AppWindow::Flush hook.  PLATFORM_TREEFROG only.
// ---------------------------------------------------------------------------
extern "C" void TreeFrogInstrumentEqOverlayDraw(void) {
    if (!EQOverlayActive || EQOverlayHold) return;

    const unsigned short bgC    = tf565(8, 9, 22);
    const unsigned short border = tf565(63, 95, 191);
    const unsigned short axisC  = tf565(86, 92, 120);
    const unsigned short gridC  = tf565(34, 38, 60);
    const unsigned short bandC  = tf565(150, 185, 235);
    const unsigned short selC   = tf565(255, 244, 120);
    const unsigned short specC  = tf565(90, 190, 130);

    const int cX0 = 6,   cX1 = 314;
    const int cY0 = 64,  cY1 = 160;      // curve canvas
    const int cMid = (cY0 + cY1) / 2;    // 0 dB

    const unsigned short bandColor = bandC;
    const unsigned short selColor = selC;

    // Curve canvas background + border
    tfFill(cX0 - 2, cY0 - 2, cX1 - cX0 + 5, cY1 - cY0 + 5, bgC);
    tfFill(cX0 - 2, cY0 - 2, cX1 - cX0 + 5, 1, border);
    tfFill(cX0 - 2, cY1 + 2, cX1 - cX0 + 5, 1, border);
    tfFill(cX0 - 2, cY0 - 2, 1, cY1 - cY0 + 5, border);
    tfFill(cX1 + 2, cY0 - 2, 1, cY1 - cY0 + 5, border);

    tfFill(cX0, cMid, cX1 - cX0, 1, axisC);
    for (int g = -24; g <= 24; g += 12) {
        int yy = cMid - (cY1 - cY0) * g / 48;
        tfFill(cX0, yy, cX1 - cX0 + 1, 1, gridC);
    }

    // Composite response curve
    const double rate = 48000.0;
    for (int x = cX0; x <= cX1; x += 3) {
        double f = xToFreq(x);
        double db = 0.0;
        for (int b = 0; b < 8; b++) {
            if (!EQBandOn[b]) continue;
            double b0, b1, b2, a1, a2;
            eqCoeffs(EQFreq[b], EQGain[b], EQQ[b], EQType[b], 48000, b0, b1, b2, a1, a2);
            double w = 2.0 * 3.14159265358979323846 * f / rate;
            double cwv = cos(w), swv = sin(w);
            double reN = b0 + b1 * cwv + b2 * cos(2 * w);
            double imN = b1 * swv + b2 * sin(2 * w);
            double reD = 1.0 + a1 * cwv + a2 * cos(2 * w);
            double imD = a1 * swv + a2 * sin(2 * w);
            db += 10.0 * log10((reN * reN + imN * imN + 1e-12) /
                               (reD * reD + imD * imD + 1e-12));
        }
        if (db > 24.0) db = 24.0;
        if (db < -24.0) db = -24.0;
        int yy = cMid + (int)(db / 24.0 * (cY1 - cY0) / 2.0);
        if (yy < cY0) yy = cY0;
        if (yy > cY1) yy = cY1;
        if (db >= 0.0) {
            tfFill(x, yy, 3, cMid - yy + 1, bandColor);
        } else {
            tfFill(x, cMid, 3, yy - cMid + 1, bandColor);
        }
        tfFill(x + 1, yy, 1, 1, selColor);
    }

    // Band handles
    for (int b2 = 0; b2 < 8; b2++) {
        if (!EQBandOn[b2]) continue;
        int x = freqToX(EQFreq[b2]);
        int yy = cMid - (int)((EQGain[b2] / 24.0) * (cY1 - cY0) / 2.0);
        if (yy < cY0) yy = cY0;
        if (yy > cY1) yy = cY1;
        unsigned short col = (b2 == EQSelected) ? selC : bandC;
        tfFill(x - 2, yy - 4, 5, 2, col);
        tfFill(x - 2, yy + 3, 5, 2, col);
        tfFill(x - 2, yy - 4, 2, 9, col);
        tfFill(x + 1, yy - 4, 2, 9, col);
    }

    // Live spectrum (20 bars)
    const int sY0 = 188, sY1 = 224;
    tfFill(cX0 - 2, sY0 - 2, cX1 - cX0 + 5, sY1 - sY0 + 5, bgC);
    tfFill(cX0 - 2, sY0 - 2, cX1 - cX0 + 5, 1, border);

    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    sp.Compute();
    const fixed *bb = sp.Bins();
    const int n = sp.BinCount();
    int bw = (cX1 - cX0) / n;
    for (int i = 0; i < n; i++) {
        int h = (int)(fp2fl(bb[i]) * 34.0f);
        if (h < 2) h = 2;
        tfFill(cX0 + i * bw, sY1 - h, bw - 1, h, specC);
    }
}

#endif  // PLATFORM_TREEFROG

// ---------------------------------------------------------------------------
// InstrumentEqModal
// ---------------------------------------------------------------------------

float InstrumentEqModal::freqFromIndex(int idx) const {
    if (idx < 0) idx = 0;
    if (idx > 59) idx = 59;
    return 20.0f * (float)pow(1000.0, idx / 59.0);
}

int InstrumentEqModal::indexFromFreq(float hz) const {
    if (hz < 20.4f) return 0;
    if (hz > 19500.0f) return 59;
    double r = log(hz / 20.0) / log(20000.0 / 20.0);
    return (int)(r * 59.0 + 0.5);
}

InstrumentEqModal::InstrumentEqModal(View &view, int instrumentIndex)
    : ModalView(view), instrumentIndex_(instrumentIndex), instr_(0),
      selected_(0), bypass_(false), suspended_(false) {
    ViewData *vd = viewData_;
    InstrumentBank *bank = vd->project_->GetInstrumentBank();
    I_Instrument *inst = bank->GetInstrument(instrumentIndex);
    if (inst && inst->GetType() == IT_SAMPLE) instr_ = (SampleInstrument *)inst;

    status_[0] = 0;
    for (int i = 0; i < 8; i++) {
        freqHz_[i] = kDefaultFreq8[i];
        gainDb_[i] = 0.0f;
        q_[i] = 1.0f;
        type_[i] = 0;
        bandOn_[i] = true;
    }
    loadFromInstrument();
    SpectrumAnalyzer::Get().SetArmed(true);
    publishToOverlay();
    SetWindow(36, 24);
}

InstrumentEqModal::~InstrumentEqModal() {
    SpectrumAnalyzer::Get().SetArmed(false);
#if defined(PLATFORM_TREEFROG)
    EQOverlayActive = 0;
    EQOverlayHold = 0;
#endif
}

void InstrumentEqModal::loadFromInstrument() {
    if (!instr_) return;
    Variable *ven = instr_->FindVariable(SIP_EQEN);
    Variable *vm = instr_->FindVariable(SIP_EQMASK);
    if (ven) bypass_ = (ven->GetInt() < 1);
    int mask = vm ? vm->GetInt() : 0xFF;
    for (int i = 0; i < 8; i++) {
        Variable *vf = instr_->FindVariable(kFreqIDs[i]);
        Variable *vg = instr_->FindVariable(kGainIDs[i]);
        Variable *vt = instr_->FindVariable(kTypeIDs[i]);
        Variable *vq = instr_->FindVariable(kQIDs[i]);
        if (vf) freqHz_[i] = (float)vf->GetInt() / 100.0f;
        if (vg) gainDb_[i] = (float)vg->GetInt();
        if (vt) type_[i] = vt->GetInt();
        if (vq) q_[i] = (float)vq->GetInt() / 100.0f;
        bandOn_[i] = ((mask >> i) & 1) != 0;
    }
}

void InstrumentEqModal::publishToOverlay() {
    if (instr_) {
        Variable *ven = instr_->FindVariable(SIP_EQEN);
        Variable *vm = instr_->FindVariable(SIP_EQMASK);
        if (ven) ven->SetInt(bypass_ ? 0 : 1);
        int mask = 0;
        for (int i = 0; i < 8; i++) if (bandOn_[i]) mask |= (1 << i);
        if (vm) vm->SetInt(mask);
        for (int i = 0; i < 8; i++) {
            Variable *vf = instr_->FindVariable(kFreqIDs[i]);
            Variable *vg = instr_->FindVariable(kGainIDs[i]);
            Variable *vt = instr_->FindVariable(kTypeIDs[i]);
            Variable *vq = instr_->FindVariable(kQIDs[i]);
            if (vf) vf->SetInt((int)(freqHz_[i] * 100.0f + 0.5f));
            if (vg) vg->SetInt((int)gainDb_[i]);
            if (vt) vt->SetInt(type_[i]);
            if (vq) vq->SetInt((int)(q_[i] * 100.0f + 0.5f));
        }
    }
    EQSelected = selected_;
    EQEnable = bypass_ ? 0 : 1;
    for (int i = 0; i < 8; i++) {
        EQType[i] = type_[i];
        EQBandOn[i] = bandOn_[i] ? 1 : 0;
        EQGain[i] = gainDb_[i];
        EQFreq[i] = freqHz_[i];
        EQQ[i] = q_[i];
    }
    EQOverlayActive = 1;
    EQOverlayHold = suspended_ ? 1 : 0;
}

void InstrumentEqModal::clearOverlay() {
    EQOverlayActive = 0;
    EQOverlayHold = 0;
}

void InstrumentEqModal::setStatus(const char *msg) {
    if (msg) strncpy(status_, msg, sizeof(status_) - 1);
    else status_[0] = 0;
    status_[sizeof(status_) - 1] = 0;
}

void InstrumentEqModal::refreshDraw() {
    publishToOverlay();
    isDirty_ = true;
}

void InstrumentEqModal::cycleBandType() {
    type_[selected_] = (type_[selected_] + 1) % 6;
    char buf[88];
    sprintf(buf, "BAND%1d %s %5.0fHz %+3ddB Q%.2f %s",
            selected_ + 1, kEqTypeNames[type_[selected_]], freqHz_[selected_],
            (int)gainDb_[selected_], q_[selected_],
            bandOn_[selected_] ? "ON" : "OFF");
    setStatus(buf);
    refreshDraw();
}

void InstrumentEqModal::DrawView() {
    GUITextProperties props;
    props.invert_ = false;

    SetColor(CD_HILITE2);
    char title[64];
    sprintf(title, "INSTR EQ8  INS-%02X", instrumentIndex_ & 0x3F);
    DrawString(1, 0, title, props);

    SetColor(CD_NORMAL);
    char line[96];
    sprintf(line, "BYPASS:%s   %d: %s   %s",
            bypass_ ? "Y" : "N",
            selected_ + 1, kEqTypeNames[type_[selected_]],
            bandOn_[selected_] ? "on" : "off");
    DrawString(1, 1, line, props);

    sprintf(line, "%5.0f Hz  %+3.1f dB  Q %.2f",
            freqHz_[selected_], gainDb_[selected_], q_[selected_]);
    DrawString(1, 3, line, props);

    props.invert_ = true;
    DrawString(1, 4, "< > band   X freq/gain", props);
    DrawString(1, 5, "A on/off   B TYPE     Y outer", props);
    DrawString(1, 6, "SELECT bypass   R+B EXIT", props);
    props.invert_ = false;

    if (status_[0]) DrawString(1, 8, status_, props);
}

void InstrumentEqModal::OnPlayerUpdate(PlayerEventType, unsigned int) {
    isDirty_ = true;
}

void InstrumentEqModal::OnFocus() {
    isDirty_ = true;
}

void InstrumentEqModal::OnSuspend() {
    suspended_ = true;
    EQOverlayHold = 1;
}

void InstrumentEqModal::OnRestore() {
    suspended_ = false;
    EQOverlayHold = 0;
    EQOverlayActive = 1;
}

void InstrumentEqModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;

    bool left  = (mask & EPBM_LEFT)  != 0;
    bool right = (mask & EPBM_RIGHT) != 0;
    bool up    = (mask & EPBM_UP)    != 0;
    bool down  = (mask & EPBM_DOWN)  != 0;
    bool a     = (mask & EPBM_A)     != 0;
    bool b     = (mask & EPBM_B)     != 0;
    bool x     = (mask & EPBM_X)     != 0;
    bool y     = (mask & EPBM_Y)     != 0;
    bool r1    = (mask & EPBM_R)     != 0;
    bool select= (mask & EPBM_SELECT)!= 0;

    if (r1 && b) {
        SpectrumAnalyzer::Get().SetArmed(false);
        EQOverlayActive = 0;
        EndModal(0);
        return;
    }

    if (select) {
        bypass_ = !bypass_;
        setStatus(bypass_ ? "EQ BYPASSED" : "EQ ON");
        refreshDraw();
        return;
    }

    if (x && left)  { freqHz_[selected_] = freqFromIndex(indexFromFreq(freqHz_[selected_]) - 1); refreshDraw(); return; }
    if (x && right) { freqHz_[selected_] = freqFromIndex(indexFromFreq(freqHz_[selected_]) + 1); refreshDraw(); return; }
    if (x && up)    { gainDb_[selected_] += 1.0f; if (gainDb_[selected_] > 24.0f) gainDb_[selected_] = 24.0f; bandOn_[selected_] = true; refreshDraw(); return; }
    if (x && down)  { gainDb_[selected_] -= 1.0f; if (gainDb_[selected_] < -24.0f) gainDb_[selected_] = -24.0f; bandOn_[selected_] = true; refreshDraw(); return; }

    if (y && left)  { q_[selected_] *= 1.25f; if (q_[selected_] > 10.0f) q_[selected_] = 10.0f; setStatus("Q wider"); refreshDraw(); return; }
    if (y && right) { q_[selected_] /= 1.25f; if (q_[selected_] < 0.1f) q_[selected_] = 0.1f; setStatus("Q narrower"); refreshDraw(); return; }
    if (y && up) {
        for (int i = 0; i < 8; i++) { gainDb_[i] += 1.0f; if (gainDb_[i] > 24.0f) gainDb_[i] = 24.0f; }
        setStatus("intensity +1 dB all bands");
        refreshDraw(); return;
    }
    if (y && down) {
        for (int i = 0; i < 8; i++) { gainDb_[i] -= 1.0f; if (gainDb_[i] < -24.0f) gainDb_[i] = -24.0f; }
        setStatus("intensity -1 dB all bands");
        refreshDraw(); return;
    }

    if (a) { bandOn_[selected_] = !bandOn_[selected_]; refreshDraw(); return; }
    if (b) { cycleBandType(); return; }

    if (left)  { selected_ = (selected_ + 7) % 8; refreshDraw(); return; }
    if (right) { selected_ = (selected_ + 1) % 8; refreshDraw(); return; }
    if (up)    { setStatus("X freq/gain   Y Q/intensity"); isDirty_ = true; return; }
    if (down)  { setStatus("<L/R> band   A on/off   B type"); isDirty_ = true; return; }
}