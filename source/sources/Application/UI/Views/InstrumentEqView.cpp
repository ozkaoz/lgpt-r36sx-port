#include "InstrumentEqView.h"

#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#include "Application/AppWindow.h"
#include "Application/Audio/EqBiquad.h"
#include "Application/Audio/InstrumentEq.h"
#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/I_Instrument.h"
#include "Application/Mixer/FxPages.h"
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
    // BACON_1.5_ANALYZER_MIX: the spectrum shows the master mix; arming the
    // analyzer here starts the (zero-cost when disarmed) master tap.
    SpectrumAnalyzer::Get().SetArmed(true);
    setStatus(0);
    isDirty_ = true;
}

void InstrumentEqView::LooseFocus() {
    SpectrumAnalyzer::Get().SetArmed(false);
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

    char title[80];
    char line[96];
    char status[96];
    buildHeader(title, sizeof(title), line, sizeof(line), status,
                sizeof(status));

    SetColor(CD_HILITE2);
    DrawString(0, 0, title, props);

    SetColor(CD_NORMAL);
    DrawString(0, 1, line, props);

    // BACON_1.5_EQ8_FULLSCREEN (bacon-1.5, feedback): row 2 carries the
    // bypass/status line; the ENTIRE screen below the header (rows 3+,
    // y >= 24) belongs to the pixel canvas, so no menu text can ever show
    // under or behind the EQ preview.
    DrawString(0, 2, status, props);
}

// BACON_1.5_EQ8_PIXEL_HEADER (U2.53, feedback #7): single source for the
// three header lines, shared by the char screen (DrawView) and the pixel
// overlay (PostFlushDraw).
void InstrumentEqView::buildHeader(char *title, size_t titleSz, char *line,
                                   size_t lineSz, char *status,
                                   size_t statusSz) const {
    const char *name = instr_ ? instr_->GetName() : "--";
    // BACON_1.5_EQ8_VIEW: the instrument id is shown WITHOUT the old 0x3F
    // mask (the full index fits the 16-col title).
    snprintf(title, titleSz, "EQ8 %s INS-%02X", name,
             viewData_->currentInstrument_);
    snprintf(line, lineSz, "B%1d %s %4.0fHz %+3.1fdB Q%.2f %s",
             selected_ + 1, kEqTypeNames[type_[selected_]],
             freqHz_[selected_], gainDb_[selected_], q_[selected_],
             bandOn_[selected_] ? "ON" : "OFF");
    if (status_[0]) {
        snprintf(status, statusSz, "%s", status_);
    } else if (bypass_) {
        snprintf(status, statusSz, "EQ BYPASSED");
    } else {
        snprintf(status, statusSz,
                 "L/R band  X f/g  Y Q  A on/off  B type  SEL bypass");
    }
}

void InstrumentEqView::OnPlayerUpdate(PlayerEventType, unsigned int) {
    isDirty_ = true;
}

void InstrumentEqView::PostFlushDraw() {
#if defined(PLATFORM_TREEFROG)
    // BACON_1.5_EQ8_HELP_ON_TOP (U2.52.9, feedback #6): AppWindow::Flush
    // calls PostFlushDraw on the current view every frame even while a
    // modal is up, and this canvas repaints the whole screen below the
    // char header -- so the SELECT+R1 help overlay (a centered modal
    // window) was drawn first and instantly erased by the canvas below
    // it ("el menu de ayuda se ve por debajo de la pantalla del EQ").
    // Same guard as MixerView::PostFlushDraw: no canvas work under a
    // modal.
    if (GetModal()) return;
    if (!instr_) return;

    const unsigned short bgC    = tf565(8, 9, 22);
    const unsigned short border = tf565(63, 95, 191);
    const unsigned short axisC  = tf565(86, 92, 120);
    const unsigned short gridC  = tf565(34, 38, 60);
    const unsigned short bandC  = tf565(150, 185, 235);
    const unsigned short selC   = tf565(255, 244, 120);
    const unsigned short lblC   = tf565(170, 178, 205);
    const unsigned short guideC = tf565(46, 52, 80);

    // BACON_1.5_EQ8_PIXEL_HEADER (U2.53, feedback #7): the three header
    // rows are re-rendered HERE in pixels every frame, directly into the
    // framebuffer AFTER the char screen was blitted (Flush -> PostFlushDraw),
    // instead of living only on the char layer.  The char screen has a
    // dirty-cell cache (_preScreen) and the pixel canvas repaints the screen
    // below y=24 on every frame; a header that stayed in char cells could
    // therefore end up erased or out of sync with the canvas repaints (the
    // band/bell/freq menu "behind the screen" report).  Painting the strip
    // background + text in pixels every frame makes the header the last
    // thing drawn, always on top, always current.
    {
        const AppWindow *app = (const AppWindow *)&w_;
        tfFill(0, 0, 320, 24, bgC);
        char title[80];
        char line[96];
        char status[96];
        buildHeader(title, sizeof(title), line, sizeof(line), status,
                    sizeof(status));
        TreeFrogDrawText8(title, 0, 0, app->ResolveColor565(CD_HILITE2));
        TreeFrogDrawText8(line, 0, 8, app->ResolveColor565(CD_NORMAL));
        TreeFrogDrawText8(status, 0, 16, app->ResolveColor565(CD_NORMAL));
    }

    // BACON_1.5_EQ8_FULLSCREEN (bacon-1.5, feedback): the canvas owns the
    // full screen below the char header.  The header is 3 rows = 24 px;
    // everything below is repainted every frame with the background color
    // first, so stale pixels or leftover glyphs from other views can never
    // survive under/behind the EQ preview.
    // BACON_1.5_EQ8_FULLSCREEN_EXT (U2.52.9, feedback #6): the canvas
    // bottom moves from row 19 (y=156, "la mitad inferior de la pantalla
    // era un recuadro negro vacio") down to y=232, leaving only the last
    // 8 px for the frequency axis labels; the curve/spectrum now fill the
    // whole screen under the header.
    const int headerH = 24;

    // Full-screen background below the header (rows 3+ => y >= 24).
    tfFill(0, headerH, 320, 240 - headerH, bgC);

    // Curve canvas below the pixel header (y 24..232, only the 8 px of the
    // frequency axis labels remain below it).
    const int cX0 = 6,   cX1 = 314;
    const int cY0 = 24,  cY1 = 232;      // curve canvas
    const int cMid = (cY0 + cY1) / 2;    // 0 dB
    // BACON_1.5_EQ8_24DB (U2.52.5, feedback): the gain range is +/-24 dB,
    // so the canvas maps the FULL range (pxPerDb over 48 dB).  Previously
    // +/-12 dB made handles/curve leave the screen for gains beyond +12.
    const double pxPerDb = (double)(cY1 - cY0) / 48.0;

    // The DSP module is only used for the sample rate; the curve/handles
    // come from the view state below (BACON_1.5_EQ8_LIVE_CURVE).
    FxEngine::InstrumentEq *eq = instr_->GetInstrumentEq();
    int rate = eq ? eq->GetSampleRate() : 48000;

    // Canvas background + border
    tfFill(cX0 - 2, cY0 - 2, cX1 - cX0 + 5, cY1 - cY0 + 5, bgC);

    // BACON_1.5_EQ8_SPECTRUM_BACKDROP (U2.52.8, feedback (F)): the live
    // spectrum is drawn INSIDE the canvas as a 30% opacity backdrop (the
    // bars live BEHIND the EQ curve, not in a separate strip below it).
    // specC (90,190,130) blended 30/70 over bgC (8,9,22) -> (33,63,54).
    // The opaque grid/axis/curve drawn afterwards stay fully readable.
    // BACON_1.5_ANALYZER_SCALE (U2.52.9, feedback #6): with the analyzer
    // fed in DAC counts (see SpectrumAnalyzer::FeedMix), fp2fl() now yields
    // the true -1..1 audio: the bins reflect the REAL dynamics of the mix
    // (a kick lights the lows, a hat the highs) and a 0 dBFS sine peaks at
    // ~0.25 (Hann window) -> the x4 below maps it to a full bar.
    // BACON_1.5_EQ8_SPECTRUM_BARS (U2.52.9, feedback #6): the 24 log bins
    // render as thin 3-px bars with gaps plus a 1-px outline on top of each
    // bar, so the spectrum reads as a filled line tracing the wave shape
    // (FabFilter Pro-Q style) instead of chunky blocks.
    {
        const unsigned short specBlend = tf565(33, 63, 54);
        const unsigned short specTop = tf565(63, 132, 92);
        SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
        sp.Compute();
        const fixed *bb = sp.Bins();
        const int n = sp.BinCount();
        int bw = (cX1 - cX0) / n;
        int barW = 3;
        for (int i = 0; i < n; i++) {
            // BACON_1.5_ANALYZER_20HZ: a 0 dBFS sine peaks around 0.25 in
            // the normalized FFT bins (Hann window), so scale x4 to make a
            // full bar mean 0 dBFS.
            // BACON_1.5_ANALYZER_DB (U2.53, feedback #7): the bars used to
            // be LINEAR (fp2fl(bb)*4 over the canvas), so a +1 dB EQ boost
            // on loud low content moved the low bars by ~12% of the canvas
            // ("+1 dB sube demasiado las barras").  The height is now the
            // mixVULevel dB mapping (-24..+3 dB over the canvas, the same
            // scale as the mixer bars), so bar height moves with perceived
            // loudness and the EQ curve stays the reference.
            int h = (int)(mixVULevel(fp2fl(bb[i]) * 4.0f) *
                          (float)(cY1 - cY0));
            if (h < 2) h = 2;
            if (h > cY1 - cY0) h = cY1 - cY0;
            int bx = cX0 + i * bw + (bw - barW) / 2;
            tfFill(bx, cY1 - h, barW, h, specBlend);
            tfFill(bx, cY1 - h, barW, 1, specTop);
        }
    }

    // BACON_1.5_EQ8_PIXEL_HEADER (U2.53, feedback #7): the top border moves
    // from cY0-2 (which clipped the last 2 px of the header strip) to the
    // canvas edge y=24; the +24 dB grid line is dropped because the border
    // now marks the +24 dB top edge.
    tfFill(cX0 - 2, cY0, cX1 - cX0 + 5, 1, border);
    tfFill(cX0 - 2, cY1 + 2, cX1 - cX0 + 5, 1, border);
    tfFill(cX0 - 2, cY0, 1, cY1 - cY0 + 3, border);
    tfFill(cX1 + 2, cY0, 1, cY1 - cY0 + 3, border);

    // dB grid: 0 dB axis + +/-12 lines (+24 is the canvas top border)
    tfFill(cX0, cMid, cX1 - cX0, 1, axisC);
    for (int g = -24; g < 24; g += 12) {
        int yy = cMid - (int)(g * pxPerDb);
        tfFill(cX0, yy, cX1 - cX0 + 1, 1, gridC);
    }

    // dB labels (left margin, inside the canvas top)
    tfTinyText(0, cY0 + 1, "+24", lblC);
    tfTinyText(0, cMid - 4, "0", lblC);
    tfTinyText(0, cY1 - 6, "-24", lblC);

    // BACON_1.5_EQ8_LIVE_CURVE (U2.52.5, feedback): the band handles and
    // the composite response are computed from the VIEW state
    // (freqHz_/gainDb_/type_/q_/bandOn_) through the same eqBiquadCoeffs()
    // the DSP uses, NOT from the DSP readbacks: the DSP only updates while
    // audio renders (preview/song), so reading it froze the canvas during
    // editing and could show a type/shape that was never assigned (stale
    // coefficients).  The view state is the DSP smoothing target, so the
    // drawn curve equals the converged DSP response exactly.
    if (!bypass_) {
        // Band vertical guide lines + handles
        for (int b = 0; b < 8; b++) {
            if (!bandOn_[b]) continue;
            int gx = freqToX(freqHz_[b]);
            int gyy = cMid - (int)(gainDb_[b] * pxPerDb);
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
            // band number just above the handle, always INSIDE the canvas
            // (a +24 dB handle would otherwise push it onto the char header)
            char num[2] = {(char)('1' + b), 0};
            int ny = gyy - 16;
            if (ny < cY0 + 1) ny = cY0 + 1;
            tfTinyText(gx - 1, ny, num, col);
        }

        // Composite response curve, evaluated from the view coefficients.
        // Bypassed EQ draws a flat 0 dB line.
        const double rateD = (double)rate;
        for (int x = cX0; x <= cX1; x += 3) {
            double f = xToFreq(x);
            double db = 0.0;
            for (int b = 0; b < 8; b++) {
                // BACON_1.5_EQ8_0DB_TRANSPARENT: a 0 dB band is transparent
                // in the DSP, so the drawn response skips it too (what you
                // see == what you hear).
                if (!bandOn_[b] || gainDb_[b] == 0.0f) continue;
                fixed f0, f1, f2, fA1, fA2;
                FxEngine::eqBiquadCoeffs(type_[b], rate, freqHz_[b],
                                         gainDb_[b], q_[b], f0, f1, f2,
                                         fA1, fA2);
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
    }

    // Frequency axis labels under the canvas.  BACON_1.5_EQ8_AXIS_LABELS
    // (U2.52.9, feedback #6): 11 labels over the 20 Hz..20 kHz log axis
    // (40 80 120 200 500 1k 2k 5k 10k 15k 20k), each centered on its
    // frequency, at the 8 px that remain below the canvas (y 235..239).
    {
        static const char *kAxisLabels[11] = {
            "40", "80", "120", "200", "500",
            "1k", "2k", "5k", "10k", "15k", "20k",
        };
        static const float kAxisFreq[11] = {
            40, 80, 120, 200, 500,
            1000, 2000, 5000, 10000, 15000, 20000,
        };
        for (int i = 0; i < 11; i++) {
            int lx = freqToX(kAxisFreq[i]) - (int)strlen(kAxisLabels[i]) * 2;
            if (lx < 0) lx = 0;
            if (lx + (int)strlen(kAxisLabels[i]) * 4 > 320)
                lx = 320 - (int)strlen(kAxisLabels[i]) * 4;
            tfTinyText(lx, cY1 + 3, kAxisLabels[i], lblC);
        }
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
    bool l1    = (mask & EPBM_L)     != 0;
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

    // BACON_1.5_EQ8_A_B_DEFAULT (bacon-1.5, feedback): A+B resets the
    // SELECTED band to its default (default frequency, gain 0, Q 1.00,
    // BELL, enabled), the same reset semantics as the form fields.
    if (a && b &&
        !(left || right || up || down || x || y || r1 || select || start)) {
        freqHz_[selected_] = kDefaultFreq8[selected_];
        gainDb_[selected_] = 0.0f;
        q_[selected_] = 1.0f;
        type_[selected_] = 0;
        bandOn_[selected_] = true;
        char buf[88];
        sprintf(buf, "BAND%1d default %5.0fHz 0dB Q1.00 BELL ON",
                selected_ + 1, freqHz_[selected_]);
        setStatus(buf);
        refreshDraw();
        return;
    }

    if (select) {
        bypass_ = !bypass_;
        setStatus(bypass_ ? "EQ BYPASSED" : "EQ ON");
        refreshDraw();
        return;
    }

    // BACON_1.5_EQ8_FAST_COARSE (U2.52.9, feedback #6): L1+X+arrows = fast
    // displacement like the rest of the port (L1 = coarse): L1+X+L/R jumps
    // the selected band ~1 octave (6 of the 59 log steps), L1+X+UP/DN steps
    // the gain by 10 dB (band turns on, same as X+UP/DN).  No conflict with
    // the global undo (L1+X alone, without arrows).
    if (l1 && x && left)  { freqHz_[selected_] = freqFromIndex(indexFromFreq(freqHz_[selected_]) - 6); setStatus("freq -1 oct"); refreshDraw(); return; }
    if (l1 && x && right) { freqHz_[selected_] = freqFromIndex(indexFromFreq(freqHz_[selected_]) + 6); setStatus("freq +1 oct"); refreshDraw(); return; }
    if (l1 && x && up)    { gainDb_[selected_] += 10.0f; if (gainDb_[selected_] > 24.0f) gainDb_[selected_] = 24.0f; bandOn_[selected_] = true; setStatus("gain +10 dB"); refreshDraw(); return; }
    if (l1 && x && down)  { gainDb_[selected_] -= 10.0f; if (gainDb_[selected_] < -24.0f) gainDb_[selected_] = -24.0f; bandOn_[selected_] = true; setStatus("gain -10 dB"); refreshDraw(); return; }

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