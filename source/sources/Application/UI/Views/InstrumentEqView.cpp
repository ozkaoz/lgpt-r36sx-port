#include "InstrumentEqView.h"

#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#include "Application/Audio/InstrumentEq.h"
#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/I_Instrument.h"
#include "Application/Player/Player.h"
#include "Application/UI/Views/ViewData.h"
#include "Application/UI/Views/BaseClasses/ViewEvent.h"
#include "UIFramework/BasicDatas/GUIEvent.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *kEqTypeNames[7] = {
    "BELL", "LOWSH", "HISHE", "LOWPA", "HIPAS", "NOTCH", "BANDP",
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

// ---------------------------------------------------------------------------
// Pixel helpers (PLATFORM_TREEFROG only)
// ---------------------------------------------------------------------------
#if defined(PLATFORM_TREEFROG)

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

// 3x5 pixel font used to label the canvas (band numbers, dB scale, frequency
// axis).  Glyphs: 0-9 (indices 0..9), '+' (10), '-' (11), 'k' (12).
static const unsigned char kTinyGlyphs[13][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7},   // 0
    {0x2, 0x6, 0x2, 0x2, 0x7},   // 1
    {0x7, 0x1, 0x7, 0x4, 0x7},   // 2
    {0x7, 0x1, 0x7, 0x1, 0x7},   // 3
    {0x5, 0x5, 0x7, 0x1, 0x1},   // 4
    {0x7, 0x4, 0x7, 0x1, 0x7},   // 5
    {0x7, 0x4, 0x7, 0x5, 0x7},   // 6
    {0x7, 0x1, 0x2, 0x2, 0x2},   // 7
    {0x7, 0x5, 0x7, 0x5, 0x7},   // 8
    {0x7, 0x5, 0x7, 0x1, 0x7},   // 9
    {0x0, 0x2, 0x7, 0x2, 0x0},   // +
    {0x0, 0x0, 0x7, 0x0, 0x0},   // -
    {0x5, 0x5, 0x6, 0x6, 0x5},   // k
};

static int tfTinyIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == '+') return 10;
    if (c == '-') return 11;
    if (c == 'k') return 12;
    return -1;
}

static void tfTinyText(int x, int y, const char *s, unsigned short c) {
    while (s && *s) {
        int g = tfTinyIndex(*s);
        if (g >= 0) {
            for (int r = 0; r < 5; r++) {
                unsigned char row = kTinyGlyphs[g][r];
                for (int b = 0; b < 3; b++) {
                    if (row & (1 << (2 - b))) tfFill(x + b, y + r, 1, 1, c);
                }
            }
        }
        x += 4;
        s++;
    }
}

#endif  // PLATFORM_TREEFROG

// Log frequency mapping of the canvas, 20 Hz .. 20 kHz over x = 6..314.
// (Only used by the pixel canvas: PLATFORM_TREEFROG.)
#if defined(PLATFORM_TREEFROG)
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
#endif  // PLATFORM_TREEFROG

}  // namespace

// ---------------------------------------------------------------------------
// InstrumentEqView
// ---------------------------------------------------------------------------

InstrumentEqView::InstrumentEqView(GUIWindow &w, ViewData *data)
    : View(w, data), instr_(0), selected_(0), bypass_(false) {
    status_[0] = 0;
    for (int i = 0; i < 8; i++) {
        freqHz_[i] = kDefaultFreq8[i];
        gainDb_[i] = 0.0f;
        q_[i] = 1.0f;
        type_[i] = 0;
        bandOn_[i] = true;
    }
}

InstrumentEqView::~InstrumentEqView() {
    SpectrumAnalyzer::Get().SetArmed(false);
    SpectrumAnalyzer::Get().SetTargetInstrument(0);
}

float InstrumentEqView::freqFromIndex(int idx) const {
    if (idx < 0) idx = 0;
    if (idx > 59) idx = 59;
    return 20.0f * (float)pow(1000.0, idx / 59.0);
}

int InstrumentEqView::indexFromFreq(float hz) const {
    if (hz < 20.4f) return 0;
    if (hz > 19500.0f) return 59;
    double r = log(hz / 20.0) / log(20000.0 / 20.0);
    return (int)(r * 59.0 + 0.5);
}

void InstrumentEqView::loadFromInstrument() {
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    int index = viewData_->currentInstrument_;
    if (index < 0 || index >= MAX_INSTRUMENT_COUNT) index = 0;
    instr_ = bank->GetInstrument(index);
    if (!instr_ || (instr_->GetType() != IT_SAMPLE &&
                    instr_->GetType() != IT_SYNTH &&
                    instr_->GetType() != IT_PIANO)) {
        instr_ = 0;
    }
    if (!instr_) return;

    Variable *ven = instr_->FindVariable(SIP_EQEN);
    Variable *vm = instr_->FindVariable(SIP_EQMASK);
    bypass_ = (ven && ven->GetInt() < 1);
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

void InstrumentEqView::writeToInstrument() {
    if (!instr_) return;
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

void InstrumentEqView::setStatus(const char *msg) {
    if (msg) strncpy(status_, msg, sizeof(status_) - 1);
    else status_[0] = 0;
    status_[sizeof(status_) - 1] = 0;
}

void InstrumentEqView::cycleBandType() {
    type_[selected_] = (type_[selected_] + 1) % 7;
    char buf[88];
    sprintf(buf, "BAND%1d %s %5.0fHz %+3ddB Q%.2f %s",
            selected_ + 1, kEqTypeNames[type_[selected_]], freqHz_[selected_],
            (int)gainDb_[selected_], q_[selected_],
            bandOn_[selected_] ? "ON" : "OFF");
    setStatus(buf);
    refreshDraw();
}

void InstrumentEqView::refreshDraw() {
    writeToInstrument();
    isDirty_ = true;
}

void InstrumentEqView::OnFocus() {
    loadFromInstrument();
    SpectrumAnalyzer::Get().SetTargetInstrument(instr_);
    SpectrumAnalyzer::Get().SetArmed(true);
    setStatus(0);
    isDirty_ = true;
}

void InstrumentEqView::LooseFocus() {
    SpectrumAnalyzer::Get().SetArmed(false);
    SpectrumAnalyzer::Get().SetTargetInstrument(0);
    View::LooseFocus();
}

void InstrumentEqView::OnFrameUpdate(unsigned long frameClock) {
    // Keep the status readout current (analyzer bins update in PostFlushDraw;
    // a dirty flag here lets the frame loop repaint at UI cadence).
    (void)frameClock;
}

void InstrumentEqView::DrawView() {
    Clear();
    View::EnableNotification();

    GUITextProperties props;
    props.invert_ = false;

    SetColor(CD_HILITE2);
    char title[80];
    const char *name = instr_ ? instr_->GetName() : "--";
    // BACON_1.5_EQ8_VIEW: the instrument id is shown WITHOUT the old 0x3F
    // mask (the full index fits the 16-col title).
    sprintf(title, "EQ8 %s INS-%02X", name, viewData_->currentInstrument_);
    DrawString(0, 0, title, props);

    SetColor(CD_NORMAL);
    char line[96];
    sprintf(line, "B%1d %s %4.0fHz %+3.1fdB Q%.2f %s",
            selected_ + 1, kEqTypeNames[type_[selected_]],
            freqHz_[selected_], gainDb_[selected_], q_[selected_],
            bandOn_[selected_] ? "ON" : "OFF");
    DrawString(0, 1, line, props);
    if (bypass_) DrawString(0, 2, "EQ BYPASSED", props);

    props.invert_ = true;
    DrawString(0, 3, "< > band   X freq/gain   Y Q", props);
    DrawString(0, 4, "A on/off  B type  SEL bypass", props);
    DrawString(0, 5, "START play/stop  R+START stop  R+B exit", props);
    props.invert_ = false;

    if (status_[0]) DrawString(0, 6, status_, props);
}

void InstrumentEqView::OnPlayerUpdate(PlayerEventType, unsigned int) {
    isDirty_ = true;
}

void InstrumentEqView::PostFlushDraw() {
#if defined(PLATFORM_TREEFROG)
    if (!instr_) return;

    const unsigned short bgC    = tf565(8, 9, 22);
    const unsigned short border = tf565(63, 95, 191);
    const unsigned short axisC  = tf565(86, 92, 120);
    const unsigned short gridC  = tf565(34, 38, 60);
    const unsigned short bandC  = tf565(150, 185, 235);
    const unsigned short selC   = tf565(255, 244, 120);
    const unsigned short specC  = tf565(90, 190, 130);
    const unsigned short lblC   = tf565(170, 178, 205);
    const unsigned short guideC = tf565(46, 52, 80);

    // Fullscreen canvas below the char header (rows 0..6 => 56 px).
    const int cX0 = 6,   cX1 = 314;
    const int cY0 = 40,  cY1 = 172;      // curve canvas
    const int cMid = (cY0 + cY1) / 2;    // 0 dB
    const double pxPerDb = (double)(cY1 - cY0) / 24.0;  // +/-12 dB visible

    // BACON_1.5_EQ8_VIEW: single source of truth = the DSP module of the
    // instrument being edited.  Its (smoothed) coefficients ARE what the
    // audio path applies right now.
    FxEngine::InstrumentEq *eq = instr_->GetInstrumentEq();
    int rate = eq ? eq->GetSampleRate() : 48000;

    // Canvas background + border
    tfFill(cX0 - 2, cY0 - 2, cX1 - cX0 + 5, cY1 - cY0 + 5, bgC);
    tfFill(cX0 - 2, cY0 - 2, cX1 - cX0 + 5, 1, border);
    tfFill(cX0 - 2, cY1 + 2, cX1 - cX0 + 5, 1, border);
    tfFill(cX0 - 2, cY0 - 2, 1, cY1 - cY0 + 5, border);
    tfFill(cX1 + 2, cY0 - 2, 1, cY1 - cY0 + 5, border);

    // dB grid: 0 dB axis + +/-6/+/-12 lines
    tfFill(cX0, cMid, cX1 - cX0, 1, axisC);
    for (int g = -12; g <= 12; g += 6) {
        int yy = cMid - (int)(g * pxPerDb);
        tfFill(cX0, yy, cX1 - cX0 + 1, 1, gridC);
    }

    // dB labels (left margin, outside the canvas)
    tfTinyText(0, cY0 - 1, "+12", lblC);
    tfTinyText(0, cMid - 4, "0", lblC);
    tfTinyText(0, cY1 - 6, "-12", lblC);

    if (!eq) {
        // No instrument EQ (should not happen: OnFocus filters the types).
        tfTinyText(cX0, cMid - 4, "no EQ", lblC);
        return;
    }

    // Band vertical guide lines + handles (before the curve so the response
    // stays on top).  Live values come from the DSP readbacks.
    for (int b = 0; b < 8; b++) {
        if (!eq->GetBandEnabled(b)) continue;
        if (bypass_) continue;
        int gx = freqToX(fp2fl(eq->GetBandFreq(b)));
        int gyy = cMid - (int)(fp2fl(eq->GetBandGainDb(b)) * pxPerDb);
        if (gyy < cY0) gyy = cY0;
        if (gyy > cY1) gyy = cY1;
        unsigned short col = (b == selected_) ? selC : bandC;
        // faint vertical guide from the top of the canvas to the handle
        tfFill(gx, cY0, 1, gyy - cY0 + 1, guideC);
        // crosshair handle
        tfFill(gx - 2, gyy - 4, 5, 2, col);
        tfFill(gx - 2, gyy + 3, 5, 2, col);
        tfFill(gx - 2, gyy - 4, 2, 9, col);
        tfFill(gx + 1, gyy - 4, 2, 9, col);
        // band number just above the handle
        char num[2] = {(char)('1' + b), 0};
        int ny = gyy - 16;
        if (ny < cY0 - 8) ny = cY0 - 8;
        tfTinyText(gx - 1, ny, num, col);
    }

    // Composite response curve, evaluated from the DSP's ACTUAL coefficients
    // (the same ones Process() applies to the signal).  Bypassed EQ draws a
    // flat 0 dB line.
    const double rateD = (double)rate;
    for (int x = cX0; x <= cX1; x += 3) {
        double f = xToFreq(x);
        double db = 0.0;
        if (bypass_) {
            db = 0.0;
        } else {
            for (int b = 0; b < 8; b++) {
                if (!eq->GetBandEnabled(b)) continue;
                fixed f0, f1, f2, fA1, fA2;
                eq->GetBandCoeffs(b, &f0, &f1, &f2, &fA1, &fA2);
                double b0 = fp2fl(f0), b1 = fp2fl(f1), b2 = fp2fl(f2);
                double a1 = fp2fl(fA1), a2 = fp2fl(fA2);
                double w = 2.0 * 3.14159265358979323846 * f / rateD;
                double cwv = cos(w), swv = sin(w);
                double reN = b0 + b1 * cwv + b2 * cos(2 * w);
                double imN = b1 * swv + b2 * sin(2 * w);
                double reD = 1.0 + a1 * cwv + a2 * cos(2 * w);
                double imD = a1 * swv + a2 * sin(2 * w);
                db += 10.0 * log10((reN * reN + imN * imN + 1e-12) /
                                   (reD * reD + imD * imD + 1e-12));
            }
        }
        if (db > 24.0) db = 24.0;
        if (db < -24.0) db = -24.0;
        int yy = cMid - (int)(db * pxPerDb);
        if (yy < cY0) yy = cY0;
        if (yy > cY1) yy = cY1;
        if (db >= 0.0) {
            tfFill(x, yy, 3, cMid - yy + 1, bandC);
        } else {
            tfFill(x, cMid, 3, yy - cMid + 1, bandC);
        }
        tfFill(x + 1, yy, 1, 1, selC);
    }

    // Frequency axis labels under the canvas
    tfTinyText(freqToX(100) - 3, cY1 + 3, "100", lblC);
    tfTinyText(freqToX(1000) - 2, cY1 + 3, "1k", lblC);
    tfTinyText(freqToX(10000) - 5, cY1 + 3, "10k", lblC);

    // Live spectrum (targeted analyzer tap)
    const int sY0 = 184, sY1 = 214;
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
#endif
}

void InstrumentEqView::ProcessButtonMask(unsigned short mask, bool pressed) {
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
    bool start = (mask & EPBM_START) != 0;
    bool select= (mask & EPBM_SELECT)!= 0;

    // R+B: back to the instrument screen.
    if (r1 && b) {
        ViewType vt = VT_INSTRUMENT;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
        return;
    }

    // BACON_1.5_START_EQ8 (bacon-1.5, item 3): START toggles playback with
    // the SAME Player::OnStartButton() contract as InstrumentView; R+START
    // stops (MixerView R+START semantics).
    if (start) {
        Player *player = Player::GetInstance();
        if (player) {
            player->OnStartButton(PM_PHRASE, viewData_->songX_, r1,
                                  viewData_->chainRow_);
        }
        return;
    }

    if (!instr_) return;

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