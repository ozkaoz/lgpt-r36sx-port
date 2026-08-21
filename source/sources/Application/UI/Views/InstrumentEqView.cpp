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
static const FourCC kSlopeIDs[8] = {SIP_EQS0, SIP_EQS1, SIP_EQS2, SIP_EQS3,
                                    SIP_EQS4, SIP_EQS5, SIP_EQS6, SIP_EQS7};

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
// axis, peak marker).  Glyphs: 0-9 (indices 0..9), '+' (10), '-' (11),
// 'k' (12), '.' (13).
static const unsigned char kTinyGlyphs[14][5] = {
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
    {0x0, 0x0, 0x0, 0x0, 0x2},   // .
};

static int tfTinyIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == '+') return 10;
    if (c == '-') return 11;
    if (c == 'k') return 12;
    if (c == '.') return 13;
    return -1;
}

// BACON_1.5_ANALYZER_PEAK (U2.61): formats the marker frequency for the
// 3x5 font: "85", "1.3k", "20k" ('.' only where it adds precision).
static void tfFormatHz(char *out, size_t n, float hz) {
    if (hz < 1000.0f) {
        snprintf(out, n, "%d", (int)hz);
    } else if (hz < 10000.0f) {
        snprintf(out, n, "%.1fk", hz / 1000.0f);
    } else {
        snprintf(out, n, "%dk", (int)(hz / 1000.0f + 0.5f));
    }
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

// Log frequency mapping of the canvas, 20 Hz .. 20 kHz over x = 6..314
// (full width inside the screen, BACON_1.5_EQ8_NO_FRAME U2.57b).
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
    : View(w, data), instr_(0), selected_(0), bypass_(false),
      peakMarkerOn_(false), peakHz_(0.0f), peakManual_(false) {
    status_[0] = 0;
    for (int i = 0; i < 8; i++) {
        freqHz_[i] = kDefaultFreq8[i];
        gainDb_[i] = 0.0f;
        q_[i] = 1.0f;
        type_[i] = 0;
        bandOn_[i] = true;
        slope_[i] = 1;
    }
    for (int i = 0; i < 308; i++) heldH_[i] = 0.0f;
}

InstrumentEqView::~InstrumentEqView() {
    SpectrumAnalyzer::Get().SetArmed(false);
    SpectrumAnalyzer::Get().SetInstrumentTarget(0);
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
        Variable *vs = instr_->FindVariable(kSlopeIDs[i]);
        if (vf) freqHz_[i] = (float)vf->GetInt() / 100.0f;
        if (vg) gainDb_[i] = (float)vg->GetInt();
        if (vt) type_[i] = vt->GetInt();
        if (vq) q_[i] = (float)vq->GetInt() / 100.0f;
        if (vs) slope_[i] = vs->GetInt();
        if (slope_[i] < 1) slope_[i] = 1;
        // BACON_1.5_EQ8_SLOPE96 (U2.65): 1..8 (12..96 dB/oct)
        if (slope_[i] > 8) slope_[i] = 8;
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
        Variable *vs = instr_->FindVariable(kSlopeIDs[i]);
        if (vf) vf->SetInt((int)(freqHz_[i] * 100.0f + 0.5f));
        if (vg) vg->SetInt((int)gainDb_[i]);
        if (vt) vt->SetInt(type_[i]);
        if (vq) vq->SetInt((int)(q_[i] * 100.0f + 0.5f));
        if (vs) vs->SetInt(slope_[i]);
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
    sprintf(buf, "BAND%1d %s %5.0fHz %+3ddB Q%.2f S%d %s",
            selected_ + 1, kEqTypeNames[type_[selected_]], freqHz_[selected_],
            (int)gainDb_[selected_], q_[selected_], slope_[selected_],
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
    // BACON_1.5_ANALYZER_MIX: the analyzer shows the master mix...
    // BACON_1.5_ANALYZER_INSTRUMENT (U2.59, feedback #12): ...unless the
    // view is editing an instrument: while this view has focus, the tap is
    // redirected to the instrument being edited (its post-EQ dry output),
    // so the spectrum and the drawn EQ curve are the SAME signal -- boosts
    // visibly raise the bars, cuts visibly lower them (the kick's 2500 Hz
    // click finally shows, the snare's low cut finally drops the low bars).
    SpectrumAnalyzer::Get().SetArmed(true);
    SpectrumAnalyzer::Get().SetInstrumentTarget(instr_);
    // BACON_1.5_ANALYZER_PEAKHIST (U2.62): start the historical peak
    // tracking from this focus, so L2+R2 marks the peak of the CURRENT
    // listening session.
    SpectrumAnalyzer::Get().PeakTrackReset();
    for (int i = 0; i < 308; i++) heldH_[i] = 0.0f;
    setStatus(0);
    isDirty_ = true;
}

void InstrumentEqView::LooseFocus() {
    SpectrumAnalyzer::Get().SetArmed(false);
    SpectrumAnalyzer::Get().SetInstrumentTarget(0);
    for (int i = 0; i < 308; i++) heldH_[i] = 0.0f;
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
    snprintf(line, lineSz, "B%1d %s %4.0fHz %+3.1fdB Q%.2f S%d %s",
             selected_ + 1, kEqTypeNames[type_[selected_]],
             freqHz_[selected_], gainDb_[selected_], q_[selected_],
             slope_[selected_],
             bandOn_[selected_] ? "ON" : "OFF");
    if (status_[0]) {
        snprintf(status, statusSz, "%s", status_);
    } else if (bypass_) {
        snprintf(status, statusSz, "EQ BYPASSED");
    } else {
        // BACON_1.5_EQ8_SLOPE48 (U2.64): L2+X mueve Hz de banda 1-8 sin mover peak,
        // R2+X+UP/DN slope 12/24/36/48 (pared vs suave)
        snprintf(status, statusSz,
                 "L/R band  X f/g  Y Q  L2+R2 peak  R2+X slope");
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
    // BACON_1.5_EQ8_WAVE_PURPLE (U2.60, feedback #12): the composite
    // response curve was a SOLID light-blue fill ("tono solido azul claro,
    // casi blanco"); it is now a purple wash behind the spectrum so the two
    // layers are distinguishable ("el morado mas transparente respecto a
    // las barras azules").  U2.61 (feedback #13): the wash drops from 30%
    // to ~15% -- (190,110,220) blended 15/85 over the canvas bg (8,9,22)
    // -> (35,24,52) -- so the blue spectrum bars stay clearly visible
    // THROUGH the curve and the hipass/boost cuts read as real drops in
    // the bars, FabFilter Pro-Q style.  waveTop is the pure purple 1-px
    // edge that keeps the curve line readable on the wash.
    const unsigned short waveC   = tf565(35, 24, 52);
    const unsigned short waveTop = tf565(190, 110, 220);

    // BACON_1.5_EQ8_NO_FRAME (U2.57b, feedback #10): the chopper frame is
    // REMOVED ("quitemos el recuadro rosado, pero mantengamos el estilo
    // full screen"): the EQ8 keeps the U2.53 fullscreen canvas (header 3
    // rows = 24 px, curve canvas y 24..232, axis labels at y 235..239).
    // The three header rows are re-rendered HERE in pixels every frame,
    // directly into the framebuffer AFTER the char screen was blitted
    // (Flush -> PostFlushDraw), instead of living only on the char layer.
    // The char screen has a dirty-cell cache (_preScreen) and the pixel
    // canvas repaints the screen below y=24 on every frame; a header that
    // stayed in char cells could therefore end up erased or out of sync
    // with the canvas repaints (the band/bell/freq menu "behind the
    // screen" report).  Painting the strip background + text in pixels
    // every frame makes the header the last thing drawn, always on top,
    // always current.
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

    // Curve canvas below the pixel header.  BACON_1.5_EQ8_HEADER_CLEAR
    // (U2.59, feedback #12 "el dibujo del EQ tapa las letras del menu"):
    // the canvas top moves DOWN 4 px (cY0 = 28) so the curve, handles,
    // band numbers and spectrum bars can NEVER touch the 3 header rows
    // (y 0..23: EQ8 / Bell / L/R band).  Every canvas element clamps to
    // y >= 28; the header text is the last thing painted each frame and is
    // always fully readable ("las letras deben ser claramente
    // identificables, no superpuestas").
    const int cX0 = 6,   cX1 = 314;
    const int cY0 = 28,  cY1 = 232;      // curve canvas (4 px clear of the header)
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
    // spectrum is drawn INSIDE the canvas as a backdrop (the bars live
    // BEHIND the EQ curve, not in a separate strip below it).
    // BACON_1.5_EQ8_PURPLE_ORDER (U2.62, feedback #14): the bars are drawn
    // AFTER the purple curve wash (and before the 1-px curve edge), so the
    // blue spectrum stays visible THROUGH the purple ("las barras azules
    // deben verse atraves del morado") -- the wash is a backdrop, the bars
    // are the foreground layer, and the curve edge stays the top line.
    // The bar code moved below, between the curve fill and the curve edge.

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

    // dB labels (left margin x=0, fullscreen -- BACON_1.5_EQ8_NO_FRAME
    // U2.57b: no left border column, the labels sit on the screen edge)
    tfTinyText(0, cY0 + 1, "+24", lblC);
    tfTinyText(0, cMid - 4, "0", lblC);
    tfTinyText(0, cY1 - 6, "-24", lblC);

    // BACON_1.5_EQ8_RANGES (U2.65): lineas moradas semitransparentes que
    // marcan Graves (20-250), Medios (250-4000) y Agudos (4000-20000).
    // Solo referencia visual, no afectan el DSP.
    {
        const unsigned short rangeC = tf565(110, 45, 165); // morado semitransparente
        int xLow = freqToX(250);
        int xMid = freqToX(4000);
        // lineas verticales punteadas (1px cada 2px)
        for (int yy = cY0; yy <= cY1; yy += 2) {
            tfFill(xLow, yy, 1, 1, rangeC);
            tfFill(xMid, yy, 1, 1, rangeC);
        }
        // etiquetas pequeñas
        tfFill(xLow - 6, cY0 + 2, 13, 6, bgC);
        tfTinyText(xLow - 5, cY0 + 2, "LOW", rangeC);
        tfFill(xMid - 6, cY0 + 2, 13, 6, bgC);
        tfTinyText(xMid - 5, cY0 + 2, "MID", rangeC);
        int xHigh = freqToX(12000);
        tfFill(xHigh - 6, cY0 + 2, 13, 6, bgC);
        tfTinyText(xHigh - 5, cY0 + 2, "HI", rangeC);
    }

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
            // BACON_1.5_EQ8_HEADER_CLEAR (U2.59): clamp the handle INSIDE the
            // canvas (gyy >= cY0+4 so the 9-px crosshair spans y>=cY0 and
            // can never overlap the 3 header rows above the canvas).
            int gyy = cMid - (int)(gainDb_[b] * pxPerDb);
            if (gyy < cY0 + 4) gyy = cY0 + 4;
            if (gyy > cY1) gyy = cY1;
            unsigned short col = (b == selected_) ? selC : bandC;
            // faint vertical guide from the top of the canvas to the handle
            tfFill(gx, cY0, 1, gyy - cY0 + 1, guideC);
            // crosshair handle
            tfFill(gx - 2, gyy - 4, 5, 2, col);
            tfFill(gx - 2, gyy + 3, 5, 2, col);
            tfFill(gx - 2, gyy - 4, 2, 9, col);
            tfFill(gx + 1, gyy - 4, 2, 9, col);
        }

        // Composite response curve, evaluated from the view coefficients.
        // Bypassed EQ draws a flat 0 dB line.
        // BACON_1.5_EQ8_PURPLE_ORDER (U2.62, feedback #14): the curve is
        // split in TWO passes: the purple WASH (below) comes first, then
        // the spectrum bars, then the 1-px purple edge -- so the blue bars
        // show through the wash and the curve line stays the top layer.
        const double rateD = (double)rate;
        for (int x = cX0; x <= cX1; x += 3) {
            double f = xToFreq(x);
            double db = 0.0;
            for (int b = 0; b < 8; b++) {
                // BACON_1.5_EQ8_0DB_TRANSPARENT: BELL/SHELF at 0 dB transparent,
                // filter types (LP/HP/BP/NOTCH) draw at 0 dB (cut)
                bool isFilter = (type_[b] == 3 || type_[b] == 4 || type_[b] == 5 || type_[b] == 6);
                if (!bandOn_[b] || (!isFilter && gainDb_[b] == 0.0f)) continue;
                fixed f0, f1, f2, fA1, fA2;
                // BACON_1.5_EQ8_WALL (U2.65): LOWPA/HIPAS siempre Butterworth
                // Also mirror InstrumentEq::recomputeBand Q clamping for <80Hz
                float qDraw = q_[b];
                if (freqHz_[b] < 80.0f && slope_[b] > 1) qDraw = 0.70710678f;
                else if (type_[b] == 3 || type_[b] == 4 ||
                         (type_[b] == 0 && freqHz_[b] < 80.0f && slope_[b] > 4) ||
                         ((type_[b] == 1 || type_[b] == 2) && freqHz_[b] < 80.0f && slope_[b] > 4)) qDraw = 0.70710678f;
                FxEngine::eqBiquadCoeffsShift(type_[b], rate, freqHz_[b],
                                         gainDb_[b], qDraw, f0, f1, f2,
                                         fA1, fA2, 24);
                double b0 = (double)f0 / (1<<24), b1 = (double)f1 / (1<<24), b2 = (double)f2 / (1<<24);
                double a1 = (double)fA1 / (1<<24), a2 = (double)fA2 / (1<<24);
                double w = 2.0 * 3.14159265358979323846 * f / rateD;
                double cwv = cos(w), swv = sin(w);
                double reN = b0 + b1 * cwv + b2 * cos(2 * w);
                double imN = b1 * swv + b2 * sin(2 * w);
                double reD = 1.0 + a1 * cwv + a2 * cos(2 * w);
                double imD = a1 * swv + a2 * sin(2 * w);
                double bandDb = 10.0 * log10((reN * reN + imN * imN + 1e-12) /
                                             (reD * reD + imD * imD + 1e-12));
                // BACON_1.5_EQ8_SLOPE96 (U2.65): slope 1..8 = 12..96 dB/oct,
                // todos los tipos incl BELL (campana más pronunciada)
                db += (double)slope_[b] * bandDb;
            }
            if (db > 24.0) db = 24.0;
            if (db < -24.0) db = -24.0;
            int yy = cMid - (int)(db * pxPerDb);
            if (yy < cY0) yy = cY0;
            if (yy > cY1) yy = cY1;
            if (db >= 0.0) {
                tfFill(x, yy, 3, cMid - yy + 1, waveC);
            } else {
                tfFill(x, cMid, 3, yy - cMid + 1, waveC);
            }
        }

        // BACON_1.5_EQ8_SPECTRUM_BLUE (U2.60, feedback #12): the analyzer is
        // BLUE -- (60,120,220) blended 30/70 over the canvas bg -> (24,42,81)
        // with a bright-blue 1-px peak outline; the opaque grid/axis drawn
        // before stay fully readable.
        // BACON_1.5_ANALYZER_SCALE (U2.52.9, feedback #6): with the analyzer
        // fed in DAC counts (see SpectrumAnalyzer::FeedMix), fp2fl() now
        // yields the true -1..1 audio: the bins reflect the REAL dynamics of
        // the mix and a 0 dBFS sine peaks at ~0.25 (Hann window) -> the x4
        // below maps it to a full bar.
        // BACON_1.5_ANALYZER_FINE (U2.61, feedback #13) -> BACON_1.5_ANALYZER_
        // FINER (U2.62, feedback #14): 154 bars at 2 px -> 308 bars at 1 px,
        // edge to edge (308/308 = 1) -- a continuous 1 px spectrum line where
        // the hipass cuts and click harmonics read as real spectral shape.
        // BACON_1.5_EQ8_PURPLE_ORDER (U2.62): the bars draw OVER the curve
        // wash and UNDER the curve edge (see the curve comment above).
        {
            const unsigned short specBlend = tf565(24, 42, 81);
            const unsigned short specTop = tf565(90, 170, 255);
            SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
            // BACON_1.5_ANALYZER_FINE (U2.61) -> FINER (U2.62): a 16384-point
            // FFT every 5 frames (~12 fps at 60 fps UI) costs ~2x the 8192
            // one -- the analyzer stays live, the UI thread stays light.
            static int fftThrottle = 0;
            if ((++fftThrottle % 5) == 0) sp.Compute();
            const fixed *bb = sp.Bins();
            const int n = sp.BinCount();
            int canvasW = cX1 - cX0 + 1;
            // bar width inclusive to cover 6..314 without gap
            // BACON_1.5_ANALYZER_EQUAL (U2.65): peak hold para que todas
            // las barras (graves, medios, agudos) se vean iguales como
            // los graves ("todas las barras deben ser iguales").  Los
            // graves son sostenidos, los agudos son transitorios: sin
            // hold el snare desaparece en 1 frame y parece distinto.
            // Hold con decay 0.92 por frame de UI (~60 fps, vida media
            // ~500 ms) mantiene el pico visible igual que el kick.
            for (int i = 0; i < n; i++) {
                // BACON_1.5_ANALYZER_DB (U2.53, feedback #7): the bars are
                // dB-mapped so height moves with perceived loudness and the
                // EQ curve stays the reference ("+1 dB sube demasiado las
                // barras" on the old linear map).
                // BACON_1.5_EQ8_SPECTRUM_36DB (U2.58, feedback #11): the
                // bars map -36..+4 dB over the canvas (floor -36 dB, 0 dBFS
                // at 90% of the canvas, +4 dB clipped at the top) so the
                // hi-hat highs (real kit: -25..-29 dB) read and move.
                float p = fp2fl(bb[i]);
                float db = (p > 0.0f) ? 20.0f * log10f(p) : -80.0f;
                float frac = (db + 36.0f) / 40.0f;
                if (frac < 0.0f) frac = 0.0f;
                if (frac > 1.0f) frac = 1.0f;
                int h = (int)(frac * (float)(cY1 - cY0));
                if (h < 2) h = 2;
                if (h > cY1 - cY0) h = cY1 - cY0;
                float fcHold = sp.BinFrequency(i);
                int hh = h;
                if (fcHold > 140.0f) {
                    if ((float)h > heldH_[i]) heldH_[i] = (float)h;
                    else heldH_[i] *= 0.92f;
                    hh = (int)(heldH_[i] + 0.5f);
                    if (hh < 2) hh = 2;
                    if (hh > cY1 - cY0) hh = cY1 - cY0;
                } else {
                    heldH_[i] = (float)h;
                }
                int bx = cX0 + (i * canvasW) / n;
                int nextBx = cX0 + ((i + 1) * canvasW) / n;
                int barW = nextBx - bx;
                tfFill(bx, cY1 - hh, barW, hh, specBlend);
                tfFill(bx, cY1 - hh, barW, 1, specTop);
                if (bx > cX0) tfFill(bx - 1, cY1 - hh + 1, 1, 1, specTop);
                if (bx + barW < cX1) tfFill(bx + barW, cY1 - hh + 1, 1, 1, specTop);
            }
        }

        // 1-px purple edge on top of the bars (the curve line).
        for (int x = cX0; x <= cX1; x += 3) {
            double f = xToFreq(x);
            double db = 0.0;
            for (int b = 0; b < 8; b++) {
                bool isFilter2 = (type_[b] == 3 || type_[b] == 4 || type_[b] == 5 || type_[b] == 6);
                if (!bandOn_[b] || (!isFilter2 && gainDb_[b] == 0.0f)) continue;
                fixed f0, f1, f2, fA1, fA2;
                float qDraw2 = q_[b];
                if (freqHz_[b] < 80.0f && slope_[b] > 1) qDraw2 = 0.70710678f;
                else if (type_[b] == 3 || type_[b] == 4 ||
                         (type_[b] == 0 && freqHz_[b] < 80.0f && slope_[b] > 4) ||
                         ((type_[b] == 1 || type_[b] == 2) && freqHz_[b] < 80.0f && slope_[b] > 4)) qDraw2 = 0.70710678f;
                FxEngine::eqBiquadCoeffsShift(type_[b], rate, freqHz_[b],
                                         gainDb_[b], qDraw2, f0, f1, f2,
                                         fA1, fA2, 24);
                double b0 = (double)f0 / (1<<24), b1 = (double)f1 / (1<<24), b2 = (double)f2 / (1<<24);
                double a1 = (double)fA1 / (1<<24), a2 = (double)fA2 / (1<<24);
                double w = 2.0 * 3.14159265358979323846 * f / rateD;
                double cwv = cos(w), swv = sin(w);
                double reN = b0 + b1 * cwv + b2 * cos(2 * w);
                double imN = b1 * swv + b2 * sin(2 * w);
                double reD = 1.0 + a1 * cwv + a2 * cos(2 * w);
                double imD = a1 * swv + a2 * sin(2 * w);
                double bandDb = 10.0 * log10((reN * reN + imN * imN + 1e-12) /
                                             (reD * reD + imD * imD + 1e-12));
                // SLOPE96 (U2.65): 1..8 incl BELL
                db += (double)slope_[b] * bandDb;
            }
            if (db > 24.0) db = 24.0;
            if (db < -24.0) db = -24.0;
            int yy = cMid - (int)(db * pxPerDb);
            if (yy < cY0) yy = cY0;
            if (yy > cY1) yy = cY1;
            tfFill(x + 1, yy, 1, 1, waveTop);
        }

        // Band numbers, drawn AFTER the curve so the response fill can never
        // cover them (BACON_1.5_EQ8_HEADER_CLEAR, U2.59 -- the numbers must
        // stay "claramente identificables, no superpuestas").  Each number
        // gets a small canvas-color clear behind it so the curve/grid below
        // it never bleeds through the glyph.
        // BACON_1.5_EQ8_SLOPE96 (U2.65): slope 1..8 (12..96) incl BELL
        for (int b = 0; b < 8; b++) {
            if (!bandOn_[b]) continue;
            int gx = freqToX(freqHz_[b]);
            int gyy = cMid - (int)(gainDb_[b] * pxPerDb);
            if (gyy < cY0 + 4) gyy = cY0 + 4;
            if (gyy > cY1) gyy = cY1;
            unsigned short col = (b == selected_) ? selC : bandC;
            char num[2] = {(char)('1' + b), 0};
            int ny = gyy - 16;
            if (ny < cY0 + 1) ny = cY0 + 1;
            tfFill(gx - 2, ny, 7, 6, bgC);
            tfTinyText(gx - 1, ny, num, col);
            if (slope_[b] > 1) {
                int sy = gyy + 6;
                if (sy + 6 > cY1 + 1) sy = gyy - 12;
                if (sy < cY0 + 1) sy = cY0 + 1;
                // Slope indicator "S8" (S + slope 2..8) para que no se confunda
                // con el numero de banda (1..8 arriba). Ej: banda 2 con S8
                // significa slope 96 dB/oct, no banda 8.
                tfFill(gx - 4, sy, 11, 6, bgC);
                char sTxt[3] = {'S', (char)('0' + slope_[b]), 0};
                tfTinyText(gx - 3, sy, sTxt, col);
            }
        }
    }

    // BACON_1.5_ANALYZER_PEAK (U2.61, feedback #13) -> BACON_1.5_ANALYZER_
    // PEAKHIST (U2.62, feedback #14): the L2+R2 peak marker -- a yellow
    // 1-px line at the marked frequency with a bright cap and a Hz label,
    // drawn AFTER the curve so the tuned frequency is always on top.  The
    // marker follows the HISTORICAL peak (the loudest spectrum peak since
    // it was armed), refreshed every frame -- the user watches it settle on
    // where the sound's energy is centered.  It NEVER edits the EQ on its
    // own; L2+R2+X moves the selected band to it.  Drawn also while
    // bypassed: it only marks the spectrum.  While the user steps the
    // marker manually (L2+X+L/R) the auto-follow stops (peakManual_).
    if (peakMarkerOn_) {
        if (!peakManual_) {
            float h = SpectrumAnalyzer::Get().PeakFrequencyHistory();
            if (h > 0.0f) peakHz_ = h;
        }
        const unsigned short peakC = tf565(255, 244, 120);
        int px = freqToX(peakHz_);
        if (px < cX0) px = cX0;
        if (px > cX1) px = cX1;
        tfFill(px, cY0, 1, cY1 - cY0 + 1, peakC);
        tfFill(px - 1, cY0, 3, 2, peakC);
        tfFill(px - 1, cY1 - 1, 3, 2, peakC);
        char hzTxt[8];
        tfFormatHz(hzTxt, sizeof(hzTxt), peakHz_);
        int lx = px + 3;
        int tw = (int)strlen(hzTxt) * 4;
        if (lx + tw > 320) lx = px - tw - 3;
        if (lx < 0) lx = 0;
        tfFill(lx, cY0, tw + 1, 6, bgC);
        tfTinyText(lx, cY0, hzTxt, peakC);
    }

    // Frequency axis labels under the canvas.  BACON_1.5_EQ8_AXIS_LABELS
    // (U2.52.9, feedback #6): 11 labels over the 20 Hz..20 kHz log axis
    // (40 80 120 200 500 1k 2k 5k 10k 15k 20k), each centered on its
    // frequency, at the 8 px that remain between the canvas bottom (y=232)
    // and the screen edge (y 235..239, fullscreen -- U2.57b).
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

    // BACON_1.5_ANALYZER_PEAKHIST (U2.62, feedback #14): L2+R2 marks the
    // HISTORICAL peak of the instrument's post-EQ spectrum -- the loudest
    // spectrum peak since the marker was armed, so it shows where the
    // sound's energy is CENTERED instead of where it was the instant the
    // buttons were pressed ("el pico mas alto historico, donde esta el
    // peso del sonido").  The marker NEVER moves the EQ: L2+R2+X snaps the
    // selected band to it.  R2+X+UP/DN toggles the selected band's slope
    // (12/24/36/48 dB/oct, 12 suave -> 48 pared).  L2+X+L/R moves the
    // SELECTED BAND's Hz (1..8) independently, without touching the peak
    // measurement (feedback #14 revisado: L2+X no debe mover el peak).
    // All handled BEFORE the plain X+arrows so the chords are unambiguous.
    bool l2 = (mask & EPBM_L2) != 0;
    bool r2 = (mask & EPBM_R2) != 0;
    if (l2 && r2 && x && !(left || right || up || down)) {
        if (peakMarkerOn_ && peakHz_ > 0.0f) {
            freqHz_[selected_] = peakHz_;
            bandOn_[selected_] = true;
            char buf[88];
            sprintf(buf, "B%1d -> PEAK %5.0fHz", selected_ + 1, peakHz_);
            setStatus(buf);
        } else {
            setStatus("PEAK: mark first (L2+R2)");
        }
        refreshDraw();
        return;
    }
    if (l2 && r2) {
        if (peakMarkerOn_) {
            peakMarkerOn_ = false;
            peakManual_ = false;
            SpectrumAnalyzer::Get().PeakTrackReset();
            setStatus("PEAK OFF");
        } else {
            peakManual_ = false;
            SpectrumAnalyzer::Get().PeakTrackReset();
            peakMarkerOn_ = true;
            if (SpectrumAnalyzer::Get().PeakHasHistory()) {
                peakHz_ = SpectrumAnalyzer::Get().PeakFrequencyHistory();
                char buf[88];
                sprintf(buf, "PEAK %5.0fHz (hist)", peakHz_);
                setStatus(buf);
            } else {
                setStatus("PEAK: listening...");
            }
        }
        refreshDraw();
        return;
    }
    if (l2 && x && (left || right)) {
        // BACON_1.5_EQ8_1HZ (U2.65, feedback #14 revisado): L2+X+L/R
        // moves the SELECTED BAND's Hz by 1 Hz (linear), independent of
        // the peak marker.  Antes movia por indice log (~2 Hz abajo,
        // ~2 kHz arriba), ahora 1 Hz exacto con o sin peak armado.
        freqHz_[selected_] += (left ? -1.0f : 1.0f);
        if (freqHz_[selected_] < 20.0f) freqHz_[selected_] = 20.0f;
        if (freqHz_[selected_] > 20000.0f) freqHz_[selected_] = 20000.0f;
        bandOn_[selected_] = true;
        char buf[88];
        sprintf(buf, "B%1d %5.0fHz %+d dB Q%.2f S%d", selected_ + 1,
                freqHz_[selected_], (int)gainDb_[selected_],
                q_[selected_], slope_[selected_]);
        setStatus(buf);
        refreshDraw();
        return;
    }
    if (r2 && x && (up || down)) {
        // R2+X+UP/DN: slope 1..8 = 12..96 dB/oct (12 pared 96, todos los tipos incl BELL)
        if (up) {
            slope_[selected_] = (slope_[selected_] >= 8) ? 1 : slope_[selected_] + 1;
        } else {
            slope_[selected_] = (slope_[selected_] <= 1) ? 8 : slope_[selected_] - 1;
        }
        char buf[88];
        const char *tag = (slope_[selected_] == 1) ? "suave" :
                          (slope_[selected_] >= 8) ? "pared" :
                          (slope_[selected_] >= 5) ? "fuerte" : "medio";
        sprintf(buf, "B%1d SLOPE %d dB/oct (%s)", selected_ + 1,
                slope_[selected_] * 12, tag);
        setStatus(buf);
        refreshDraw();
        return;
    }

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
    // BACON_1.5_EQ8_FILTER_GAIN_LOCK (U2.65): LP/HP/BP/NOTCH no permiten
    // subir/bajar de 0 dB (solo cortan).  Su ganancia queda fija en 0.
    bool isFilterGainLocked = (type_[selected_] == 3 || type_[selected_] == 4 ||
                               type_[selected_] == 5 || type_[selected_] == 6);
    if (l1 && x && up)    { if (isFilterGainLocked) { setStatus("LP/HP/BP/NOTCH gain locked 0 dB"); refreshDraw(); return; } gainDb_[selected_] += 10.0f; if (gainDb_[selected_] > 24.0f) gainDb_[selected_] = 24.0f; bandOn_[selected_] = true; setStatus("gain +10 dB"); refreshDraw(); return; }
    if (l1 && x && down)  { if (isFilterGainLocked) { setStatus("LP/HP/BP/NOTCH gain locked 0 dB"); refreshDraw(); return; } gainDb_[selected_] -= 10.0f; if (gainDb_[selected_] < -24.0f) gainDb_[selected_] = -24.0f; bandOn_[selected_] = true; setStatus("gain -10 dB"); refreshDraw(); return; }

    if (x && left)  { freqHz_[selected_] = freqFromIndex(indexFromFreq(freqHz_[selected_]) - 1); refreshDraw(); return; }
    if (x && right) { freqHz_[selected_] = freqFromIndex(indexFromFreq(freqHz_[selected_]) + 1); refreshDraw(); return; }
    if (x && up)    { if (isFilterGainLocked) { setStatus("LP/HP/BP/NOTCH gain locked 0 dB"); refreshDraw(); return; } gainDb_[selected_] += 1.0f; if (gainDb_[selected_] > 24.0f) gainDb_[selected_] = 24.0f; bandOn_[selected_] = true; refreshDraw(); return; }
    if (x && down)  { if (isFilterGainLocked) { setStatus("LP/HP/BP/NOTCH gain locked 0 dB"); refreshDraw(); return; } gainDb_[selected_] -= 1.0f; if (gainDb_[selected_] < -24.0f) gainDb_[selected_] = -24.0f; bandOn_[selected_] = true; refreshDraw(); return; }

    if (y && left)  { q_[selected_] *= 1.25f; if (q_[selected_] > 10.0f) q_[selected_] = 10.0f; setStatus("Q wider"); refreshDraw(); return; }
    if (y && right) { q_[selected_] /= 1.25f; if (q_[selected_] < 0.1f) q_[selected_] = 0.1f; setStatus("Q narrower"); refreshDraw(); return; }
    if (y && up) {
        for (int i = 0; i < 8; i++) {
            bool isF = (type_[i] == 3 || type_[i] == 4 || type_[i] == 5 || type_[i] == 6);
            if (isF) continue;
            gainDb_[i] += 1.0f; if (gainDb_[i] > 24.0f) gainDb_[i] = 24.0f;
        }
        setStatus("intensity +1 dB all bands (filters locked)");
        refreshDraw(); return;
    }
    if (y && down) {
        for (int i = 0; i < 8; i++) {
            bool isF = (type_[i] == 3 || type_[i] == 4 || type_[i] == 5 || type_[i] == 6);
            if (isF) continue;
            gainDb_[i] -= 1.0f; if (gainDb_[i] < -24.0f) gainDb_[i] = -24.0f;
        }
        setStatus("intensity -1 dB all bands (filters locked)");
        refreshDraw(); return;
    }

    // BACON_1.5_EQ8_B_ARROWS (U2.65): B+flechas cicla tipos en orden
    // inmediato anterior/siguiente (BELL 0, LOWSH 1, HISHE 2, LOWPA 3,
    // HIPAS 4, NOTCH 5, BANDP 6), no salto aleatorio.
    if (b && (left || right || up || down)) {
        int dir = (left || down) ? -1 : 1;
        type_[selected_] = (type_[selected_] + dir + 7) % 7;
        char buf[88];
        sprintf(buf, "BAND%1d %s %5.0fHz %+3ddB Q%.2f S%d %s",
                selected_ + 1, kEqTypeNames[type_[selected_]], freqHz_[selected_],
                (int)gainDb_[selected_], q_[selected_], slope_[selected_],
                bandOn_[selected_] ? "ON" : "OFF");
        setStatus(buf);
        refreshDraw();
        return;
    }
    if (a) { bandOn_[selected_] = !bandOn_[selected_]; refreshDraw(); return; }
    if (b) { cycleBandType(); return; }

    if (left)  { selected_ = (selected_ + 7) % 8; refreshDraw(); return; }
    if (right) { selected_ = (selected_ + 1) % 8; refreshDraw(); return; }
    if (up)    { setStatus("X freq/gain   Y Q/intensity"); isDirty_ = true; return; }
    if (down)  { setStatus("<L/R> band   A on/off   B type"); isDirty_ = true; return; }
}