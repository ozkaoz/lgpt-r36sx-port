#!/usr/bin/env bash
# U2.10 Graphical Chopper session-saved cuts for sample navigation and phrase export.
# Requires validated U2.9 Chopper Sample Navigation / Phrase Export tree.
# Writes only SampleChopperModal.h/.cpp and rebuilds.
# Apply-time does not touch SONG/projects data. Runtime keeps chop boundaries while the project session is open; R1+A writes phrase rows and cloned chop instruments by explicit user action.

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

if ! grep -q "TreeFrogChopperOverlayDraw" sources/Application/AppWindow.cpp; then
  echo "ERROR: AppWindow post-flush chopper overlay hook not found."
  echo "Apply/restore validated Chop Base U2.4.3 first."
  exit 3
fi

if ! grep -q "SampleChopperModal" sources/Application/Views/InstrumentView.cpp; then
  echo "ERROR: InstrumentView chopper entry not found."
  echo "Apply/restore validated Chop Base U2.4.3 first."
  exit 4
fi

if ! grep -q "StartStreamingRangeAt" sources/Application/Player/Player.h; then
  echo "ERROR: U2.8 range audition API not found."
  echo "Apply validated U2.8/U2.9 first, then apply U2.10."
  exit 5
fi

BACKUP="_backup_before_u2_10_chopper_saved_cuts_phrase_export_$STAMP.tar.gz"
tar -czf "$BACKUP"   sources/Application/Views/ModalDialogs/SampleChopperModal.h   sources/Application/Views/ModalDialogs/SampleChopperModal.cpp   sources/Application/AppWindow.cpp   sources/Application/Views/InstrumentView.cpp   projects/Makefile

echo "Backup written: $SRC/$BACKUP"

cat > sources/Application/Views/ModalDialogs/SampleChopperModal.h <<'__LGPT_U210_H__'
#ifndef _SAMPLE_CHOPPER_MODAL_H_
#define _SAMPLE_CHOPPER_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "UIFramework/Framework/GUITextProperties.h"
#include <string>

class SampleChopperModal: public ModalView {
public:
    SampleChopperModal(View &view,
                       int instrumentIndex,
                       int sampleIndex,
                       const char *sampleName,
                       int sampleSize);
    virtual ~SampleChopperModal();

    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);

private:
    enum {
        SCREEN_W = 40,
        SCREEN_H = 30,
        WAVE_X = 16,
        WAVE_Y = 72,
        WAVE_W = 288,
        WAVE_H = 88,
        MAX_COLUMNS = 288,
        MIN_ZOOM_PERCENT = 5,
        MAX_ZOOM_PERCENT = 100,
        MAX_CHOP_BOUNDARIES = 33,
        MAX_CHOPS = 32
    };

    int instrumentIndex_;
    int sampleIndex_;
    int sampleSize_;
    int sourceSize_;
    int sourceChannels_;
    int sourceRate_;
    int cursorFrame_;
    int viewStartFrame_;
    int zoomPercent_;
    bool hasWaveform_;
    bool playbackTriggered_;
    bool chopsInitialized_;
    bool trimMode_;
    int selectedChop_;
    int boundaryCount_;
    int boundaries_[MAX_CHOP_BOUNDARIES];
    std::string sampleName_;
    std::string samplePath_;
    char statusMessage_[64];
    int minColumn_[MAX_COLUMNS];
    int maxColumn_[MAX_COLUMNS];

    bool hasAssignedSample() const;
    int clampInt(int value, int minValue, int maxValue) const;
    int getZoomFactor() const;
    int getViewFrameCount() const;
    int getCursorFrame() const;
    int frameToPixel(int frame) const;
    int pixelToFrame(int px) const;
    void clampViewStart();
    void centerViewOnCursor();
    void ensureCursorVisible();
    void nudgeCursorPixels(int deltaPx);
    void nudgeZoomPercent(int deltaPercent);
    void resetChopState();
    bool restoreChopStateForCurrentSample();
    void saveChopStateForCurrentSample();
    void loadSampleByIndex(int index, const char *reason);
    void selectSample(int delta);
    void prepareWaveformPreview();
    void initializeChopsIfNeeded();
    void addChopAtCursor();
    void deleteSelectedChop();
    void sortBoundaries();
    int findBoundaryIndex(int frame) const;
    void selectChop(int delta);
    bool hasUserChops() const;
    int selectedChopStartFrame() const;
    int selectedChopEndFrame() const;
    int getFrameStepForEdit() const;
    void toggleTrimMode();
    void nudgeSelectedStart(int deltaFrames);
    void nudgeSelectedEnd(int deltaFrames);
    void publishOverlayState();
    void clearOverlayState();
    void playFullSample();
    void playFromFrame(int frame, const char *label);
    void playFrameRange(int startFrame, int endFrame, const char *label);
    void playSelectedChop();
    void exportChopsToPhrase();
    bool ensureCurrentPhraseSlot();
    bool configureChopInstrument(int instrumentIndex, int startFrame, int endFrame);
    void stopSamplePreview();
    void setStatus(const char *message);
    void drawStringAbs(int x, int y, const char *txt, GUITextProperties &props);
    void clearTextScreen();
    void drawTopBar(GUITextProperties &props);
    void drawFrame(GUITextProperties &props);
    void drawSampleInfo(GUITextProperties &props);
    void drawEmptyWaveformText(GUITextProperties &props);
    void drawControls(GUITextProperties &props);
};

#endif


__LGPT_U210_H__

cat > sources/Application/Views/ModalDialogs/SampleChopperModal.cpp <<'__LGPT_U210_CPP__'
#include "SampleChopperModal.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SoundSource.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Player/Player.h"
#include "System/FileSystem/FileSystem.h"
#if defined(PLATFORM_TREEFROG)
#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


/* U2.10: lightweight in-session chop persistence.
   This deliberately avoids changing LGPT project serialization while the chopper UI is still evolving.
   Boundaries are keyed by project pointer + sample index/name/size, so closing and reopening the modal
   during the same project session restores cuts and allows later phrase export. */
static const int LGPT_CHOPPER_SAVED_STATE_COUNT = 128;
static const int LGPT_CHOPPER_SAVED_BOUNDARIES = 33;

struct LGPTChopperSavedState {
    bool used;
    const void *projectKey;
    int sampleIndex;
    int sourceSize;
    int selectedChop;
    int boundaryCount;
    std::string sampleName;
    int boundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];

    LGPTChopperSavedState()
        : used(false), projectKey(0), sampleIndex(-1), sourceSize(0),
          selectedChop(0), boundaryCount(0), sampleName("") {
        for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) boundaries[i] = 0;
    }
};

static LGPTChopperSavedState g_lgptChopperSavedStates[LGPT_CHOPPER_SAVED_STATE_COUNT];

static int lgptFindChopperSavedState(const void *projectKey,
                                     int sampleIndex,
                                     const std::string &sampleName,
                                     int sourceSize) {
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used &&
            s.projectKey == projectKey &&
            s.sampleIndex == sampleIndex &&
            s.sourceSize == sourceSize &&
            s.sampleName == sampleName) return i;
    }
    return -1;
}

static int lgptFindChopperFreeSavedStateSlot() {
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        if (!g_lgptChopperSavedStates[i].used) return i;
    }
    return 0; /* deterministic eviction; enough for the current R36SX test path */
}

#if defined(PLATFORM_TREEFROG)
static const int TF_W = 320;
static const int TF_H = 240;
static const int TF_WAVE_X = 16;
static const int TF_WAVE_Y = 72;
static const int TF_WAVE_W = 288;
static const int TF_WAVE_H = 88;
static const int TF_MAX_COLUMNS = 288;
static const int TF_MAX_CHOP_MARKERS = 31;
static volatile int g_chopperOverlayActive = 0;
static int g_chopperCursorPx = 0;
static int g_chopperMinColumn[TF_MAX_COLUMNS];
static int g_chopperMaxColumn[TF_MAX_COLUMNS];
static int g_chopperMarkerCount = 0;
static int g_chopperMarkerPx[TF_MAX_CHOP_MARKERS];
static int g_chopperSelectedStartPx = -1;
static int g_chopperSelectedEndPx = -1;
static int g_chopperSelectedRangeStartPx = -1;
static int g_chopperSelectedRangeEndPx = -1;
static int g_chopperTrimMode = 0;

static unsigned short tf_rgb565(unsigned char r, unsigned char g, unsigned char b) {
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void tf_rect(int x, int y, int w, int h, unsigned short color) {
    if (w <= 0 || h <= 0) return;
    uint16_t *fb = TreeFrogGetFramebuffer();
    if (!fb) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > TF_W) w = TF_W - x;
    if (y + h > TF_H) h = TF_H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        uint16_t *dst = fb + yy * TF_W + x;
        for (int xx = 0; xx < w; xx++) *dst++ = color;
    }
}

static void tf_vline(int x, int y0, int y1, unsigned short color) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    tf_rect(x, y0, 1, y1 - y0 + 1, color);
}

extern "C" void TreeFrogChopperOverlayDraw(void) {
    if (!g_chopperOverlayActive) return;

    const unsigned short bg       = tf_rgb565(4, 3, 8);
    const unsigned short border   = tf_rgb565(232, 76, 212);
    const unsigned short axis     = tf_rgb565(90, 90, 100);
    const unsigned short waveDim  = tf_rgb565(52, 78, 96);
    const unsigned short waveMid  = tf_rgb565(150, 185, 210);
    const unsigned short waveHot  = tf_rgb565(240, 248, 255);
    const unsigned short cursor   = tf_rgb565(255, 0, 0);
    const unsigned short marker   = tf_rgb565(255, 212, 255);
    const unsigned short chop     = tf_rgb565(0, 255, 190);
    const unsigned short selected = tf_rgb565(255, 226, 0);
    const unsigned short selectedBg = tf_rgb565(76, 54, 8);
    const unsigned short trimSelected = tf_rgb565(255, 128, 24);

    tf_rect(TF_WAVE_X - 2, TF_WAVE_Y - 2, TF_WAVE_W + 4, TF_WAVE_H + 4, border);
    tf_rect(TF_WAVE_X, TF_WAVE_Y, TF_WAVE_W, TF_WAVE_H, bg);

    if (g_chopperSelectedRangeStartPx >= 0 && g_chopperSelectedRangeEndPx >= 0) {
        int sx = g_chopperSelectedRangeStartPx;
        int ex = g_chopperSelectedRangeEndPx;
        if (sx > ex) { int t = sx; sx = ex; ex = t; }
        if (sx < 0) sx = 0;
        if (ex >= TF_WAVE_W) ex = TF_WAVE_W - 1;
        if (ex >= sx) {
            tf_rect(TF_WAVE_X + sx, TF_WAVE_Y + 1, ex - sx + 1, TF_WAVE_H - 2, selectedBg);
            /* Thin highlight rails: stronger contrast without hiding the waveform. */
            tf_rect(TF_WAVE_X + sx, TF_WAVE_Y + 1, ex - sx + 1, 1, selected);
            tf_rect(TF_WAVE_X + sx, TF_WAVE_Y + TF_WAVE_H - 2, ex - sx + 1, 1, selected);
        }
    }

    int center = TF_WAVE_Y + TF_WAVE_H / 2;
    int half = TF_WAVE_H / 2 - 5;
    if (half < 2) half = 2;
    tf_rect(TF_WAVE_X, center, TF_WAVE_W, 1, axis);

    for (int col = 0; col < TF_WAVE_W; col++) {
        int mn = g_chopperMinColumn[col];
        int mx = g_chopperMaxColumn[col];
        int amp = mx;
        if (-mn > amp) amp = -mn;
        if (amp <= 0) continue;

        int top = center - ((mx * half) / 32768);
        int bot = center - ((mn * half) / 32768);
        if (top > bot) { int t = top; top = bot; bot = t; }
        if (top < TF_WAVE_Y + 1) top = TF_WAVE_Y + 1;
        if (bot > TF_WAVE_Y + TF_WAVE_H - 2) bot = TF_WAVE_Y + TF_WAVE_H - 2;
        if (top == bot) {
            if (top > TF_WAVE_Y + 1) top--;
            if (bot < TF_WAVE_Y + TF_WAVE_H - 2) bot++;
        }

        unsigned short c = waveDim;
        if (amp > 18000) c = waveHot;
        else if (amp > 2800) c = waveMid;
        tf_vline(TF_WAVE_X + col, top, bot, c);
    }

    for (int i = 0; i < g_chopperMarkerCount; i++) {
        int px = g_chopperMarkerPx[i];
        if (px >= 0 && px < TF_WAVE_W) {
            int x = TF_WAVE_X + px;
            tf_vline(x, TF_WAVE_Y - 2, TF_WAVE_Y + TF_WAVE_H + 1, chop);
        }
    }

    if (g_chopperSelectedStartPx >= 0 && g_chopperSelectedStartPx < TF_WAVE_W) {
        tf_vline(TF_WAVE_X + g_chopperSelectedStartPx, TF_WAVE_Y - 3, TF_WAVE_Y + TF_WAVE_H + 2, g_chopperTrimMode ? trimSelected : selected);
    }
    if (g_chopperSelectedEndPx >= 0 && g_chopperSelectedEndPx < TF_WAVE_W) {
        tf_vline(TF_WAVE_X + g_chopperSelectedEndPx, TF_WAVE_Y - 3, TF_WAVE_Y + TF_WAVE_H + 2, g_chopperTrimMode ? trimSelected : selected);
    }

    int cx = TF_WAVE_X + g_chopperCursorPx;
    if (cx < TF_WAVE_X) cx = TF_WAVE_X;
    if (cx >= TF_WAVE_X + TF_WAVE_W) cx = TF_WAVE_X + TF_WAVE_W - 1;
    tf_vline(cx, TF_WAVE_Y - 4, TF_WAVE_Y + TF_WAVE_H + 4, cursor);
    tf_rect(cx - 2, TF_WAVE_Y - 4, 5, 1, marker);
    tf_rect(cx - 2, TF_WAVE_Y + TF_WAVE_H + 3, 5, 1, marker);
}
#else
extern "C" void TreeFrogChopperOverlayDraw(void) {}
#endif

SampleChopperModal::SampleChopperModal(View &view,
                                       int instrumentIndex,
                                       int sampleIndex,
                                       const char *sampleName,
                                       int sampleSize)
    : ModalView(view),
      instrumentIndex_(instrumentIndex),
      sampleIndex_(sampleIndex),
      sampleSize_(sampleSize),
      sourceSize_(0),
      sourceChannels_(0),
      sourceRate_(0),
      cursorFrame_(0),
      viewStartFrame_(0),
      zoomPercent_(MIN_ZOOM_PERCENT),
      hasWaveform_(false),
      playbackTriggered_(false),
      chopsInitialized_(false),
      trimMode_(false),
      selectedChop_(0),
      boundaryCount_(0),
      sampleName_(sampleName ? sampleName : "") {
    statusMessage_[0] = 0;
    if (hasAssignedSample()) {
        samplePath_ = "samples:";
        samplePath_ += sampleName_;
    }
    for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
    for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) boundaries_[b] = 0;
    prepareWaveformPreview();
    publishOverlayState();
}

SampleChopperModal::~SampleChopperModal() {
    saveChopStateForCurrentSample();
    stopSamplePreview();
    clearOverlayState();
}

bool SampleChopperModal::hasAssignedSample() const { return (sampleIndex_ >= 0) && (!sampleName_.empty()); }
void SampleChopperModal::OnFocus() { publishOverlayState(); isDirty_ = true; }
void SampleChopperModal::OnPlayerUpdate(PlayerEventType, unsigned int) {}
int SampleChopperModal::clampInt(int value, int minValue, int maxValue) const { if (value < minValue) return minValue; if (value > maxValue) return maxValue; return value; }

int SampleChopperModal::getZoomFactor() const {
    int z = zoomPercent_ / 5;
    if (z < 1) z = 1;
    return z;
}

void SampleChopperModal::setStatus(const char *message) {
    if (!message) statusMessage_[0] = 0;
    else { snprintf(statusMessage_, sizeof(statusMessage_), "%s", message); statusMessage_[sizeof(statusMessage_) - 1] = 0; }
    isDirty_ = true;
}

int SampleChopperModal::getViewFrameCount() const {
    if (sourceSize_ <= 0) return 0;
    int frames = sourceSize_ / getZoomFactor();
    if (frames < WAVE_W) frames = WAVE_W;
    if (frames > sourceSize_) frames = sourceSize_;
    return frames;
}

void SampleChopperModal::clampViewStart() {
    if (sourceSize_ <= 0) { viewStartFrame_ = 0; return; }
    int viewFrames = getViewFrameCount();
    int maxStart = sourceSize_ - viewFrames;
    if (maxStart < 0) maxStart = 0;
    viewStartFrame_ = clampInt(viewStartFrame_, 0, maxStart);
}

void SampleChopperModal::centerViewOnCursor() {
    if (sourceSize_ <= 0) { viewStartFrame_ = 0; return; }
    int viewFrames = getViewFrameCount();
    viewStartFrame_ = cursorFrame_ - viewFrames / 2;
    clampViewStart();
}

void SampleChopperModal::ensureCursorVisible() {
    if (sourceSize_ <= 0) return;
    int viewFrames = getViewFrameCount();
    if (cursorFrame_ < viewStartFrame_) viewStartFrame_ = cursorFrame_;
    if (cursorFrame_ >= viewStartFrame_ + viewFrames) viewStartFrame_ = cursorFrame_ - viewFrames + 1;
    clampViewStart();
}

int SampleChopperModal::getCursorFrame() const { return cursorFrame_; }

int SampleChopperModal::frameToPixel(int frame) const {
    int viewFrames = getViewFrameCount();
    if (viewFrames <= 1) return -1;
    if (frame < viewStartFrame_) return -1;
    if (frame > viewStartFrame_ + viewFrames - 1) return -1;
    long long rel = (long long)(frame - viewStartFrame_) * (long long)(WAVE_W - 1);
    rel /= (long long)(viewFrames - 1);
    return clampInt((int)rel, 0, WAVE_W - 1);
}

int SampleChopperModal::pixelToFrame(int px) const {
    if (sourceSize_ <= 0) return 0;
    int viewFrames = getViewFrameCount();
    if (viewFrames <= 1) return viewStartFrame_;
    px = clampInt(px, 0, WAVE_W - 1);
    long long frame = (long long)viewStartFrame_ + ((long long)px * (long long)(viewFrames - 1)) / (long long)(WAVE_W - 1);
    if (frame < 0) frame = 0;
    if (frame >= sourceSize_) frame = sourceSize_ - 1;
    return (int)frame;
}

void SampleChopperModal::nudgeCursorPixels(int deltaPx) {
    if (sourceSize_ <= 0) return;
    int viewFrames = getViewFrameCount();
    int step = viewFrames / WAVE_W;
    if (step < 1) step = 1;
    long long deltaFrames = (long long)step * (long long)deltaPx;
    long long next = (long long)cursorFrame_ + deltaFrames;
    if (next < 0) next = 0;
    if (next >= sourceSize_) next = sourceSize_ - 1;
    cursorFrame_ = (int)next;
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    isDirty_ = true;
}

void SampleChopperModal::nudgeZoomPercent(int deltaPercent) {
    int oldZoom = zoomPercent_;
    zoomPercent_ = clampInt(zoomPercent_ + deltaPercent, MIN_ZOOM_PERCENT, MAX_ZOOM_PERCENT);
    if (zoomPercent_ != oldZoom) {
        centerViewOnCursor();
        prepareWaveformPreview();
        publishOverlayState();
        char msg[64]; snprintf(msg, sizeof(msg), "Zoom %d%%", zoomPercent_); setStatus(msg);
    } else {
        isDirty_ = true;
    }
}


void SampleChopperModal::resetChopState() {
    chopsInitialized_ = false;
    trimMode_ = false;
    selectedChop_ = 0;
    boundaryCount_ = 0;
    for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) boundaries_[b] = 0;
}

bool SampleChopperModal::restoreChopStateForCurrentSample() {
    if (!hasAssignedSample() || sourceSize_ <= 1) {
        resetChopState();
        return false;
    }

    const void *projectKey = (viewData_ && viewData_->project_) ? (const void *)viewData_->project_ : 0;
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex_, sampleName_, sourceSize_);
    if (slot < 0) {
        resetChopState();
        return false;
    }

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    int count = saved.boundaryCount;
    if (count < 2 || count > MAX_CHOP_BOUNDARIES) {
        resetChopState();
        return false;
    }

    int previous = -1;
    for (int i = 0; i < count; i++) {
        int value = saved.boundaries[i];
        if (i == 0) value = 0;
        if (i == count - 1) value = sourceSize_ - 1;
        if (value < 0 || value >= sourceSize_ || value <= previous) {
            resetChopState();
            return false;
        }
        boundaries_[i] = value;
        previous = value;
    }
    for (int i = count; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;

    boundaryCount_ = count;
    selectedChop_ = clampInt(saved.selectedChop, 0, boundaryCount_ - 2);
    cursorFrame_ = boundaries_[selectedChop_];
    viewStartFrame_ = 0;
    trimMode_ = false;
    chopsInitialized_ = true;
    return true;
}

void SampleChopperModal::saveChopStateForCurrentSample() {
    if (!hasAssignedSample() || sourceSize_ <= 1 || !chopsInitialized_ || boundaryCount_ < 2) return;

    const void *projectKey = (viewData_ && viewData_->project_) ? (const void *)viewData_->project_ : 0;
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex_, sampleName_, sourceSize_);
    if (slot < 0) slot = lgptFindChopperFreeSavedStateSlot();

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    saved.used = true;
    saved.projectKey = projectKey;
    saved.sampleIndex = sampleIndex_;
    saved.sourceSize = sourceSize_;
    saved.sampleName = sampleName_;
    saved.boundaryCount = clampInt(boundaryCount_, 2, MAX_CHOP_BOUNDARIES);
    saved.selectedChop = clampInt(selectedChop_, 0, saved.boundaryCount - 2);
    for (int i = 0; i < saved.boundaryCount; i++) saved.boundaries[i] = boundaries_[i];
    for (int i = saved.boundaryCount; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) saved.boundaries[i] = 0;
}

void SampleChopperModal::loadSampleByIndex(int index, const char *reason) {
    SamplePool *pool = SamplePool::GetInstance();
    int count = pool ? pool->GetNameListSize() : 0;
    saveChopStateForCurrentSample();
    stopSamplePreview();

    if (count <= 0) {
        sampleIndex_ = NO_SAMPLE;
        sampleSize_ = 0;
        sourceSize_ = 0;
        sourceChannels_ = 0;
        sourceRate_ = 0;
        cursorFrame_ = 0;
        viewStartFrame_ = 0;
        hasWaveform_ = false;
        chopsInitialized_ = false;
        trimMode_ = false;
        selectedChop_ = 0;
        boundaryCount_ = 0;
        sampleName_.clear();
        samplePath_.clear();
        for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
        for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) boundaries_[b] = 0;
        publishOverlayState();
        setStatus("No samples in pool");
        return;
    }

    while (index < 0) index += count;
    index %= count;

    sampleIndex_ = index;
    char **names = pool->GetNameList();
    sampleName_ = (names && names[index]) ? names[index] : "";
    samplePath_.clear();
    if (!sampleName_.empty()) {
        samplePath_ = "samples:";
        samplePath_ += sampleName_;
    }

    SoundSource *source = pool->GetSource(index);
    sourceSize_ = source ? source->GetSize(-1) : 0;
    sourceChannels_ = source ? source->GetChannelCount(-1) : 0;
    sourceRate_ = source ? source->GetSampleRate(-1) : 0;
    sampleSize_ = sourceSize_;

    cursorFrame_ = 0;
    viewStartFrame_ = 0;
    zoomPercent_ = MIN_ZOOM_PERCENT;
    hasWaveform_ = false;
    chopsInitialized_ = false;
    trimMode_ = false;
    selectedChop_ = 0;
    boundaryCount_ = 0;
    for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
    for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) boundaries_[b] = 0;

    InstrumentBank *bank = viewData_ && viewData_->project_ ? viewData_->project_->GetInstrumentBank() : 0;
    if (bank && instrumentIndex_ >= 0 && instrumentIndex_ < MAX_SAMPLEINSTRUMENT_COUNT) {
        I_Instrument *instr = bank->GetInstrument(instrumentIndex_);
        if (instr && instr->GetType() == IT_SAMPLE) {
            ((SampleInstrument *)instr)->AssignSample(sampleIndex_);
        }
    }

    prepareWaveformPreview();
    publishOverlayState();

    char msg[64];
    snprintf(msg, sizeof(msg), "%s sample %02X", reason ? reason : "Select", sampleIndex_);
    setStatus(msg);
}

void SampleChopperModal::selectSample(int delta) {
    if (trimMode_) {
        setStatus("Exit trim to change sample");
        return;
    }
    loadSampleByIndex(sampleIndex_ + delta, delta > 0 ? "Next" : "Prev");
}

void SampleChopperModal::sortBoundaries() {
    for (int i = 0; i < boundaryCount_ - 1; i++) {
        for (int j = i + 1; j < boundaryCount_; j++) {
            if (boundaries_[j] < boundaries_[i]) { int t = boundaries_[i]; boundaries_[i] = boundaries_[j]; boundaries_[j] = t; }
        }
    }
}

int SampleChopperModal::findBoundaryIndex(int frame) const {
    for (int i = 0; i < boundaryCount_; i++) if (boundaries_[i] == frame) return i;
    return -1;
}

void SampleChopperModal::initializeChopsIfNeeded() {
    if (chopsInitialized_ || sourceSize_ <= 1) return;
    boundaryCount_ = 2;
    boundaries_[0] = 0;
    boundaries_[1] = sourceSize_ - 1;
    selectedChop_ = 0;
    chopsInitialized_ = true;
}

void SampleChopperModal::addChopAtCursor() {
    if (sourceSize_ <= 1) { setStatus("No sample to chop"); return; }
    initializeChopsIfNeeded();
    if (boundaryCount_ >= MAX_CHOP_BOUNDARIES) { setStatus("Max chops reached"); return; }
    int frame = getCursorFrame();
    if (frame <= 0 || frame >= sourceSize_ - 1) { setStatus("Cannot chop at edge"); return; }
    for (int i = 0; i < boundaryCount_; i++) {
        if (abs(boundaries_[i] - frame) <= 1) { setStatus("Chop already exists"); return; }
    }
    boundaries_[boundaryCount_++] = frame;
    sortBoundaries();
    int idx = findBoundaryIndex(frame);
    if (idx > 0) selectedChop_ = idx - 1;
    if (selectedChop_ < 0) selectedChop_ = 0;
    if (selectedChop_ > boundaryCount_ - 2) selectedChop_ = boundaryCount_ - 2;
    saveChopStateForCurrentSample();
    publishOverlayState();
    char msg[64]; snprintf(msg, sizeof(msg), "Chop %02d at %d", selectedChop_, frame); setStatus(msg);
}

void SampleChopperModal::deleteSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { setStatus("No chop to delete"); return; }

    /* Chops are stored as boundaries. Deleting a chop removes one internal boundary
       and merges the selected region with a neighbor. Edge boundaries 0/end are never removed. */
    int removeIdx = (selectedChop_ > 0) ? selectedChop_ : 1;
    if (removeIdx <= 0 || removeIdx >= boundaryCount_ - 1) { setStatus("Cannot delete edge"); return; }

    for (int i = removeIdx; i < boundaryCount_ - 1; i++) boundaries_[i] = boundaries_[i + 1];
    boundaryCount_--;
    if (boundaryCount_ < 2) {
        boundaryCount_ = 2;
        boundaries_[0] = 0;
        boundaries_[1] = sourceSize_ > 0 ? sourceSize_ - 1 : 0;
    }
    if (selectedChop_ > boundaryCount_ - 2) selectedChop_ = boundaryCount_ - 2;
    if (selectedChop_ < 0) selectedChop_ = 0;
    trimMode_ = false;
    cursorFrame_ = selectedChopStartFrame();
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Deleted cut");
}

void SampleChopperModal::selectChop(int delta) {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { setStatus("No user chops"); return; }
    int maxChop = boundaryCount_ - 2;
    selectedChop_ = clampInt(selectedChop_ + delta, 0, maxChop);
    cursorFrame_ = boundaries_[selectedChop_];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    char msg[64]; snprintf(msg, sizeof(msg), "Selected chop %02d", selectedChop_); setStatus(msg);
}

bool SampleChopperModal::hasUserChops() const {
    return (boundaryCount_ > 2);
}

int SampleChopperModal::selectedChopStartFrame() const {
    if (boundaryCount_ < 2) return 0;
    int idx = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    return boundaries_[idx];
}

int SampleChopperModal::selectedChopEndFrame() const {
    if (boundaryCount_ < 2) return sourceSize_ > 0 ? sourceSize_ - 1 : 0;
    int idx = clampInt(selectedChop_ + 1, 1, boundaryCount_ - 1);
    return boundaries_[idx];
}

int SampleChopperModal::getFrameStepForEdit() const {
    int viewFrames = getViewFrameCount();
    int step = viewFrames / WAVE_W;
    if (step < 1) step = 1;
    return step;
}

void SampleChopperModal::toggleTrimMode() {
    if (!hasUserChops()) { setStatus("No chop to trim"); return; }
    trimMode_ = !trimMode_;
    cursorFrame_ = selectedChopStartFrame();
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus(trimMode_ ? "Trim: A+LR start B+LR end" : "Trim mode off");
}

void SampleChopperModal::nudgeSelectedStart(int deltaFrames) {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { setStatus("No chop to trim"); return; }
    int idx = selectedChop_;
    int minFrame = (idx == 0) ? 0 : boundaries_[idx - 1] + 1;
    int maxFrame = boundaries_[idx + 1] - 1;
    boundaries_[idx] = clampInt(boundaries_[idx] + deltaFrames, minFrame, maxFrame);
    cursorFrame_ = boundaries_[idx];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Adjusted chop start");
}

void SampleChopperModal::nudgeSelectedEnd(int deltaFrames) {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { setStatus("No chop to trim"); return; }
    int idx = selectedChop_ + 1;
    int minFrame = boundaries_[idx - 1] + 1;
    int maxFrame = (idx == boundaryCount_ - 1) ? (sourceSize_ - 1) : (boundaries_[idx + 1] - 1);
    boundaries_[idx] = clampInt(boundaries_[idx] + deltaFrames, minFrame, maxFrame);
    cursorFrame_ = boundaries_[idx];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Adjusted chop end");
}

void SampleChopperModal::prepareWaveformPreview() {
    hasWaveform_ = false;
    if (!chopsInitialized_) { sourceSize_ = 0; sourceChannels_ = 0; sourceRate_ = 0; }
    for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
    if (!hasAssignedSample()) return;
    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) return;
    int size = source->GetSize(-1);
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    void *buffer = source->GetSampleBuffer(-1);
    if ((size <= 0) || (channels <= 0) || (!buffer)) return;
    sourceSize_ = size; sourceChannels_ = channels; sourceRate_ = rate;
    if (!chopsInitialized_) {
        if (!restoreChopStateForCurrentSample()) initializeChopsIfNeeded();
    }
    cursorFrame_ = clampInt(cursorFrame_, 0, sourceSize_ - 1);
    clampViewStart();
    short *samples = (short *)buffer;
    int viewFrames = getViewFrameCount();
    if (viewFrames <= 0) return;
    if (viewStartFrame_ + viewFrames > sourceSize_) viewFrames = sourceSize_ - viewStartFrame_;
    for (int col = 0; col < WAVE_W; col++) {
        int start = viewStartFrame_ + (col * viewFrames) / WAVE_W;
        int end = viewStartFrame_ + ((col + 1) * viewFrames) / WAVE_W;
        if (end <= start) end = start + 1;
        if (start < 0) start = 0;
        if (end > size) end = size;
        int minValue = 32767;
        int maxValue = -32768;
        for (int i = start; i < end; i++) { int value = samples[i * channels]; if (value < minValue) minValue = value; if (value > maxValue) maxValue = value; }
        if (minValue == 32767 && maxValue == -32768) { minValue = 0; maxValue = 0; }
        minColumn_[col] = minValue; maxColumn_[col] = maxValue;
    }
    hasWaveform_ = true;
}

void SampleChopperModal::publishOverlayState() {
#if defined(PLATFORM_TREEFROG)
    g_chopperOverlayActive = hasWaveform_ ? 1 : 0;
    g_chopperCursorPx = frameToPixel(cursorFrame_);
    if (g_chopperCursorPx < 0) g_chopperCursorPx = 0;
    for (int i = 0; i < MAX_COLUMNS; i++) { g_chopperMinColumn[i] = minColumn_[i]; g_chopperMaxColumn[i] = maxColumn_[i]; }
    g_chopperMarkerCount = 0;
    for (int b = 1; b < boundaryCount_ - 1 && g_chopperMarkerCount < TF_MAX_CHOP_MARKERS; b++) {
        int px = frameToPixel(boundaries_[b]);
        if (px >= 0) g_chopperMarkerPx[g_chopperMarkerCount++] = px;
    }
    g_chopperSelectedStartPx = -1;
    g_chopperSelectedEndPx = -1;
    g_chopperSelectedRangeStartPx = -1;
    g_chopperSelectedRangeEndPx = -1;
    g_chopperTrimMode = trimMode_ ? 1 : 0;
    if (hasUserChops() && selectedChop_ >= 0 && selectedChop_ <= boundaryCount_ - 2) {
        int startFrame = boundaries_[selectedChop_];
        int endFrame = boundaries_[selectedChop_ + 1];
        int viewFrames = getViewFrameCount();
        int viewEnd = viewStartFrame_ + viewFrames - 1;
        g_chopperSelectedStartPx = frameToPixel(startFrame);
        g_chopperSelectedEndPx = frameToPixel(endFrame);
        int clipStart = startFrame < viewStartFrame_ ? viewStartFrame_ : startFrame;
        int clipEnd = endFrame > viewEnd ? viewEnd : endFrame;
        if (clipStart <= clipEnd) {
            g_chopperSelectedRangeStartPx = frameToPixel(clipStart);
            g_chopperSelectedRangeEndPx = frameToPixel(clipEnd);
        }
    }
#endif
}

void SampleChopperModal::clearOverlayState() {
#if defined(PLATFORM_TREEFROG)
    g_chopperOverlayActive = 0;
    g_chopperSelectedRangeStartPx = -1;
    g_chopperSelectedRangeEndPx = -1;
    g_chopperTrimMode = 0;
#endif
}

void SampleChopperModal::playFromFrame(int frame, const char *label) {
    if (!hasAssignedSample() || samplePath_.empty()) { setStatus("No sample to play"); return; }
    if (sourceSize_ > 0) frame = clampInt(frame, 0, sourceSize_ - 1);
    else frame = 0;
    Path path(samplePath_.c_str());
    Player::GetInstance()->StopStreaming();
    Player::GetInstance()->StartStreamingAt(path, frame);
    playbackTriggered_ = true;
    char msg[64]; snprintf(msg, sizeof(msg), "%s %d", label ? label : "Play", frame); setStatus(msg);
}

void SampleChopperModal::playFrameRange(int startFrame, int endFrame, const char *label) {
    if (!hasAssignedSample() || samplePath_.empty()) { setStatus("No sample to play"); return; }
    if (sourceSize_ > 0) {
        startFrame = clampInt(startFrame, 0, sourceSize_ - 1);
        endFrame = clampInt(endFrame, 0, sourceSize_ - 1);
    } else {
        startFrame = 0;
        endFrame = 0;
    }
    if (endFrame < startFrame) endFrame = startFrame;
    Path path(samplePath_.c_str());
    Player::GetInstance()->StopStreaming();
    Player::GetInstance()->StartStreamingRangeAt(path, startFrame, endFrame);
    playbackTriggered_ = true;
    char msg[64]; snprintf(msg, sizeof(msg), "%s %d-%d", label ? label : "Play", startFrame, endFrame); setStatus(msg);
}

void SampleChopperModal::playFullSample() {
    playFromFrame(0, "Play full");
}

void SampleChopperModal::playSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { playFullSample(); return; }
    playFrameRange(selectedChopStartFrame(), selectedChopEndFrame(), "Play chop");
}

bool SampleChopperModal::ensureCurrentPhraseSlot() {
    if (!viewData_ || !viewData_->song_ || !viewData_->song_->phrase_) {
        setStatus("No phrase model");
        return false;
    }

    int phraseIndex = viewData_->currentPhrase_;
    if (phraseIndex < 0 || phraseIndex >= PHRASE_COUNT || phraseIndex == 0xFE || phraseIndex == 0xFF) {
        unsigned short next = viewData_->song_->phrase_->GetNext();
        if (next == NO_MORE_PHRASE) {
            setStatus("No free phrase");
            return false;
        }
        phraseIndex = (int)next;
        viewData_->currentPhrase_ = phraseIndex;
    }

    viewData_->song_->phrase_->SetUsed((unsigned char)phraseIndex);

    unsigned char *chainSlot = viewData_->GetCurrentChainPointer();
    if (chainSlot && *chainSlot == 0xFF) {
        *chainSlot = (unsigned char)phraseIndex;
    }
    return true;
}

bool SampleChopperModal::configureChopInstrument(int instrumentIndex, int startFrame, int endFrame) {
    if (!viewData_ || !viewData_->project_) return false;
    if (instrumentIndex < 0 || instrumentIndex >= MAX_SAMPLEINSTRUMENT_COUNT) return false;

    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (!bank) return false;
    I_Instrument *raw = bank->GetInstrument(instrumentIndex);
    if (!raw || raw->GetType() != IT_SAMPLE) return false;

    SampleInstrument *instr = (SampleInstrument *)raw;
    instr->AssignSample(sampleIndex_);

    int start = clampInt(startFrame, 0, sourceSize_ > 0 ? sourceSize_ - 1 : 0);
    int endInclusive = clampInt(endFrame, start, sourceSize_ > 0 ? sourceSize_ - 1 : start);
    int endExclusive = endInclusive + 1;
    if (sourceSize_ > 0 && endExclusive > sourceSize_) endExclusive = sourceSize_;
    if (endExclusive <= start) endExclusive = start + 1;

    Variable *v = instr->FindVariable(SIP_START);
    if (v) v->SetInt(start);
    v = instr->FindVariable(SIP_LOOPSTART);
    if (v) v->SetInt(start);
    v = instr->FindVariable(SIP_END);
    if (v) v->SetInt(endExclusive);
    v = instr->FindVariable(SIP_LOOPMODE);
    if (v) v->SetInt(SILM_ONESHOT);
    v = instr->FindVariable(SIP_SLICES);
    if (v) v->SetInt(1);
    return true;
}

void SampleChopperModal::exportChopsToPhrase() {
    if (!hasAssignedSample() || sourceSize_ <= 1) {
        setStatus("No sample to export");
        return;
    }
    initializeChopsIfNeeded();
    if (!hasUserChops()) {
        setStatus("Add cuts first");
        return;
    }
    saveChopStateForCurrentSample();
    if (!ensureCurrentPhraseSlot()) return;

    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (!bank) {
        setStatus("No instrument bank");
        return;
    }

    Phrase *phrase = viewData_->song_->phrase_;
    int phraseIndex = viewData_->currentPhrase_;
    int rowStart = viewData_->phraseCurPos_;
    if (rowStart < 0 || rowStart > 15) rowStart = 0;

    int chopCount = boundaryCount_ - 1;
    int maxRows = 16 - rowStart;
    if (chopCount > maxRows) chopCount = maxRows;
    if (chopCount <= 0) {
        setStatus("Phrase row full");
        return;
    }

    int exported = 0;
    for (int i = 0; i < chopCount; i++) {
        unsigned short nextInstr = bank->Clone((unsigned short)instrumentIndex_);
        if (nextInstr == NO_MORE_INSTRUMENT) break;
        if (!configureChopInstrument((int)nextInstr, boundaries_[i], boundaries_[i + 1])) break;

        int row = rowStart + i;
        int offset = 16 * phraseIndex + row;
        phrase->note_[offset] = 60;
        phrase->instr_[offset] = (unsigned char)nextInstr;
        phrase->cmd1_[offset] = I_CMD_NONE;
        phrase->param1_[offset] = 0;
        phrase->cmd2_[offset] = I_CMD_NONE;
        phrase->param2_[offset] = 0;
        exported++;
    }

    if (exported <= 0) {
        setStatus("No free instruments");
        return;
    }

    isDirty_ = true;
    char msg[64];
    snprintf(msg, sizeof(msg), "Export %02d chops P%02X R%02X", exported, phraseIndex, rowStart);
    setStatus(msg);
}

void SampleChopperModal::stopSamplePreview() { if (playbackTriggered_) { Player::GetInstance()->StopStreaming(); playbackTriggered_ = false; } }
void SampleChopperModal::drawStringAbs(int x, int y, const char *txt, GUITextProperties &props) { View::DrawString(x, y, txt, props); }
void SampleChopperModal::clearTextScreen() { View::ClearRect(0, 0, SCREEN_W, SCREEN_H); }
void SampleChopperModal::drawTopBar(GUITextProperties &props) { props.invert_ = true; SetColor(CD_HILITE1); drawStringAbs(0, 0, " P G  SCPI  M TT       CHOPPER       ", props); props.invert_ = false; }

void SampleChopperModal::drawFrame(GUITextProperties &props) {
    SetColor(CD_BORDER);
    drawStringAbs(0, 1,  "+--------------------------------------+", props);
    for (int y = 2; y < 22; y++) { drawStringAbs(0, y, "|", props); drawStringAbs(39, y, "|", props); }
    drawStringAbs(0, 22, "+--------------------------------------+", props);
    SetColor(CD_HILITE2);
    drawStringAbs(2, 2, "Graphical Chopper U2.10", props);
}

void SampleChopperModal::drawSampleInfo(GUITextProperties &props) {
    char buffer[96];
    SetColor(CD_NORMAL);
    snprintf(buffer, sizeof(buffer), "Inst:%02X Smpl:%02X Zoom:%03d%%", instrumentIndex_, sampleIndex_ < 0 ? 0 : sampleIndex_, zoomPercent_);
    buffer[37] = 0; drawStringAbs(2, 4, buffer, props);
    if (!hasAssignedSample()) { drawStringAbs(2, 5, "No sample assigned", props); drawStringAbs(2, 6, "No chop actions", props); }
    else { std::string name = sampleName_; if (name.size() > 31) name = name.substr(0, 31); snprintf(buffer, sizeof(buffer), "Name:%s", name.c_str()); buffer[37] = 0; drawStringAbs(2, 5, buffer, props); snprintf(buffer, sizeof(buffer), "Frame:%d/%d Chop:%02d/%02d%s", cursorFrame_, sourceSize_ > 0 ? sourceSize_ - 1 : 0, hasUserChops() ? selectedChop_ : 0, hasUserChops() ? (boundaryCount_ - 1) : 0, trimMode_ ? " TRIM" : ""); buffer[37] = 0; drawStringAbs(2, 6, buffer, props); }
    if (statusMessage_[0]) { SetColor(CD_HILITE1); drawStringAbs(2, 21, statusMessage_, props); }
}

void SampleChopperModal::drawEmptyWaveformText(GUITextProperties &props) { SetColor(CD_HILITE1); drawStringAbs(2, 13, "----------- no sample loaded -----------", props); }

void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    drawStringAbs(0, 24, "R1+LR sample  L1+LR fast cursor", props);
    drawStringAbs(0, 25, "A cut Y del B play R1+A export", props);
    drawStringAbs(0, 26, "R2+LR chop  R2+A full  L2+B stop", props);
    SetColor(CD_HILITE1);
    drawStringAbs(0, 28, trimMode_ ? "TRIM: A+LR start B+LR end Y del L2+Y exit" : "L2+Y trim  L1+B back/export unsaved", props);
}

void SampleChopperModal::DrawView() {
    GUITextProperties props; props.invert_ = false;
    clearTextScreen(); drawTopBar(props); drawFrame(props); drawSampleInfo(props); if (!hasWaveform_) drawEmptyWaveformText(props); drawControls(props); publishOverlayState();
}

void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;

    bool left = (mask & EPBM_LEFT) != 0;
    bool right = (mask & EPBM_RIGHT) != 0;
    bool up = (mask & EPBM_UP) != 0;
    bool down = (mask & EPBM_DOWN) != 0;
    bool l1 = (mask & EPBM_L) != 0;
    bool r1 = (mask & EPBM_R) != 0;
    bool l2 = (mask & EPBM_L2) != 0;
    bool r2 = (mask & EPBM_R2) != 0;
    bool y = (mask & EPBM_Y) != 0;
    bool a = (mask & EPBM_A) != 0;
    bool b = (mask & EPBM_B) != 0;

    /* U2.10 control model:
       MAIN: A adds a cut marker, Y deletes the selected cut, B auditions selected chop until its end boundary.
       R1+LEFT/RIGHT browses loaded project samples; R1+A exports chops to phrase rows as cloned instruments.
       TRIM: A+LR adjusts start, B+LR adjusts end; A/B alone do not add or play.
       This follows sampler logic: marker placement is separate from trim editing and phrase export. */

    if ((l1 || r1) && b && !(left || right || up || down)) {
        stopSamplePreview();
        EndModal(0);
        isDirty_ = true;
        return;
    }

    if (l2 && b && !(left || right || up || down)) {
        stopSamplePreview();
        setStatus("Stop playback");
        return;
    }

    if (l2 && y) {
        toggleTrimMode();
        return;
    }

    if (r1 && a) {
        exportChopsToPhrase();
        return;
    }

    if (r1 && (left || right)) {
        selectSample(right ? 1 : -1);
        return;
    }

    if (r2 && a) {
        playFullSample();
        return;
    }

    if (r2 && (left || right)) {
        selectChop(right ? 1 : -1);
        return;
    }

    if (trimMode_) {
        if ((left || right) && (a || b)) {
            int delta = getFrameStepForEdit();
            if (!right) delta = -delta;
            if (l1) delta *= 10;
            if (a) nudgeSelectedStart(delta);
            else if (b) nudgeSelectedEnd(delta);
            return;
        }

        if (y && !l1 && !r1 && !l2 && !r2) {
            deleteSelectedChop();
            return;
        }

        if ((a || b) && !(left || right)) {
            setStatus(a ? "Trim: A+LEFT/RIGHT" : "Trim: B+LEFT/RIGHT");
            return;
        }
    } else {
        if (y && !l1 && !r1 && !l2 && !r2) {
            deleteSelectedChop();
            return;
        }

        if (b && !l1 && !r1 && !l2 && !r2) {
            playSelectedChop();
            return;
        }

        if (a && !l1 && !r1 && !l2 && !r2) {
            addChopAtCursor();
            return;
        }
    }

    if (left || right) {
        int deltaPx = right ? 2 : -2;
        if (l1) deltaPx = right ? 24 : -24;
        nudgeCursorPixels(deltaPx);
        return;
    }

    if (up || down) {
        int deltaPercent = up ? 5 : -5;
        if (l1) deltaPercent = up ? 10 : -10;
        nudgeZoomPercent(deltaPercent);
        return;
    }
}


__LGPT_U210_CPP__

if ! grep -q "SampleChopperModal.o" projects/Makefile; then
  python3 - <<'__LGPT_U210_PY__'
from pathlib import Path
p=Path('projects/Makefile')
s=p.read_text()
needle='ImportSampleDialog.o '
if needle in s:
    s=s.replace(needle, 'ImportSampleDialog.o SampleChopperModal.o ', 1)
else:
    raise SystemExit('Could not insert SampleChopperModal.o into projects/Makefile')
p.write_text(s)
__LGPT_U210_PY__
fi

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_10_CHOPPER_SAVED_CUTS_PHRASE_EXPORT_$STAMP.log"
echo "Starting U2.10 build..."
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}

echo
echo "BUILD_RC=$RC"
echo "LOG=$SRC/$LOG"
if [ "$RC" -eq 0 ]; then
  ls -lh "$SRC/dist/lgpt_libretro.so"
  sha256sum "$SRC/dist/lgpt_libretro.so"
else
  echo "Build failed. Relevant errors:"
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,220p'
  tail -n 120 "$LOG"
fi
exit "$RC"
