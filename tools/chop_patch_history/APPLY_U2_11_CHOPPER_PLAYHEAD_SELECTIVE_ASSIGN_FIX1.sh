#!/usr/bin/env bash
# U2.11 Chopper playhead overlay + selective chop assignment from Chopper/Phrase.
# Applies on top of validated Chop Base/U2.10-style source trees.
# Writes SampleChopperModal.h/.cpp, patches streaming position APIs, patches PhraseView R2 chop assignment, then rebuilds.

set -euo pipefail

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
  echo "Apply/restore validated Chop Base/U2.10 first."
  exit 4
fi

if ! grep -q "EPBM_R2" sources/Application/Views/BaseClasses/View.h; then
  echo "ERROR: EPBM_R2 input bit not found. Apply validated R36SX input refactor first."
  exit 5
fi

BACKUP="_backup_before_u2_11_chopper_playhead_selective_assign_$STAMP.tar.gz"
tar -czf "$BACKUP"   sources/Application/Views/ModalDialogs/SampleChopperModal.h   sources/Application/Views/ModalDialogs/SampleChopperModal.cpp   sources/Application/Views/PhraseView.h   sources/Application/Views/PhraseView.cpp   sources/Application/Audio/AudioFileStreamer.h   sources/Application/Audio/AudioFileStreamer.cpp   sources/Application/Player/Player.h   sources/Application/Player/Player.cpp   sources/Application/Player/PlayerMixer.h   sources/Application/Player/PlayerMixer.cpp   projects/Makefile

echo "Backup written: $SRC/$BACKUP"

cat > sources/Application/Views/ModalDialogs/SampleChopperModal.h <<'__LGPT_U211_H__'
#ifndef _SAMPLE_CHOPPER_MODAL_H_
#define _SAMPLE_CHOPPER_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "UIFramework/Framework/GUITextProperties.h"
#include <string>

class ViewData;

bool LGPTChopperAssignSavedChopToPhraseRow(ViewData *viewData,
                                           int phraseIndex,
                                           int row,
                                           int sourceInstrumentIndex,
                                           int requestedChop,
                                           int delta,
                                           bool advanceSessionCursor,
                                           char *status,
                                           int statusLen);
int LGPTChopperGetSavedChopCountForInstrument(ViewData *viewData,
                                               int sourceInstrumentIndex);

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
    bool previewActive_;
    int previewStartFrame_;
    int previewEndFrame_;
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
    void setPreviewPlaybackRange(int startFrame, int endFrame);
    void clearPreviewPlaybackRange();
    void assignSelectedChopToPhrase();
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


__LGPT_U211_H__

cat > sources/Application/Views/ModalDialogs/SampleChopperModal.cpp <<'__LGPT_U211_CPP__'
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


/* U2.11: lightweight in-session chop persistence + selective phrase assignment.
   This deliberately avoids changing LGPT project serialization while the chopper UI is still evolving.
   Boundaries are keyed by project pointer + sample index/name/size, so closing and reopening the modal
   during the same project session restores cuts and allows later selective phrase assignment. */
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
    int chopInstrument[32];

    LGPTChopperSavedState()
        : used(false), projectKey(0), sampleIndex(-1), sourceSize(0),
          selectedChop(0), boundaryCount(0), sampleName("") {
        for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) boundaries[i] = 0;
        for (int i = 0; i < 32; i++) chopInstrument[i] = -1;
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


static void lgptSetAssignStatus(char *status, int statusLen, const char *message) {
    if (!status || statusLen <= 0) return;
    if (!message) message = "";
    snprintf(status, statusLen, "%s", message);
    status[statusLen - 1] = 0;
}

static bool lgptGetSampleIdentityForInstrument(ViewData *viewData,
                                               int instrumentIndex,
                                               int &sampleIndex,
                                               std::string &sampleName,
                                               int &sourceSize) {
    sampleIndex = NO_SAMPLE;
    sampleName.clear();
    sourceSize = 0;
    if (!viewData || !viewData->project_) return false;
    if (instrumentIndex < 0 || instrumentIndex >= MAX_SAMPLEINSTRUMENT_COUNT) return false;

    InstrumentBank *bank = viewData->project_->GetInstrumentBank();
    if (!bank) return false;
    I_Instrument *raw = bank->GetInstrument(instrumentIndex);
    if (!raw || raw->GetType() != IT_SAMPLE) return false;

    SampleInstrument *instr = (SampleInstrument *)raw;
    Variable *sampleVar = instr->FindVariable(SIP_SAMPLE);
    sampleIndex = sampleVar ? sampleVar->GetInt() : instr->GetSampleIndex();
    if (sampleIndex == NO_SAMPLE || sampleIndex < 0) return false;

    const char *fileName = instr->GetFileName();
    if (fileName && fileName[0]) sampleName = fileName;
    else {
        SamplePool *pool = SamplePool::GetInstance();
        char **names = pool ? pool->GetNameList() : 0;
        int count = pool ? pool->GetNameListSize() : 0;
        if (names && sampleIndex >= 0 && sampleIndex < count && names[sampleIndex]) {
            sampleName = names[sampleIndex];
        }
    }
    if (sampleName.empty()) return false;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex);
    sourceSize = source ? source->GetSize(-1) : instr->GetSampleSize();
    return sourceSize > 1;
}

static bool lgptConfigureSavedChopInstrument(ViewData *viewData,
                                             int instrumentIndex,
                                             int sampleIndex,
                                             int sourceSize,
                                             int startFrame,
                                             int endFrame) {
    if (!viewData || !viewData->project_) return false;
    if (instrumentIndex < 0 || instrumentIndex >= MAX_SAMPLEINSTRUMENT_COUNT) return false;

    InstrumentBank *bank = viewData->project_->GetInstrumentBank();
    if (!bank) return false;
    I_Instrument *raw = bank->GetInstrument(instrumentIndex);
    if (!raw || raw->GetType() != IT_SAMPLE) return false;

    SampleInstrument *instr = (SampleInstrument *)raw;
    instr->AssignSample(sampleIndex);

    int maxFrame = sourceSize > 0 ? sourceSize - 1 : 0;
    int start = startFrame;
    int endInclusive = endFrame;
    if (start < 0) start = 0;
    if (start > maxFrame) start = maxFrame;
    if (endInclusive < start) endInclusive = start;
    if (endInclusive > maxFrame) endInclusive = maxFrame;

    int endExclusive = endInclusive + 1;
    if (sourceSize > 0 && endExclusive > sourceSize) endExclusive = sourceSize;
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
    v = instr->FindVariable(SIP_ROOTNOTE);
    if (v) v->SetInt(60);
    return true;
}

static int lgptFindChopForInstrument(const LGPTChopperSavedState &saved, int instrumentIndex) {
    if (instrumentIndex < 0) return -1;
    int chopCount = saved.boundaryCount - 1;
    if (chopCount > 32) chopCount = 32;
    for (int i = 0; i < chopCount; i++) {
        if (saved.chopInstrument[i] == instrumentIndex) return i;
    }
    return -1;
}

static int lgptEnsureChopInstrument(ViewData *viewData,
                                    LGPTChopperSavedState &saved,
                                    int sourceInstrumentIndex,
                                    int chopIndex,
                                    char *status,
                                    int statusLen) {
    if (!viewData || !viewData->project_) {
        lgptSetAssignStatus(status, statusLen, "No project");
        return -1;
    }
    int chopCount = saved.boundaryCount - 1;
    if (chopIndex < 0 || chopIndex >= chopCount || chopIndex >= 32) {
        lgptSetAssignStatus(status, statusLen, "Bad chop");
        return -1;
    }

    InstrumentBank *bank = viewData->project_->GetInstrumentBank();
    if (!bank) {
        lgptSetAssignStatus(status, statusLen, "No instrument bank");
        return -1;
    }

    int mapped = saved.chopInstrument[chopIndex];
    if (mapped >= 0 && mapped < MAX_SAMPLEINSTRUMENT_COUNT) {
        I_Instrument *raw = bank->GetInstrument(mapped);
        if (raw && raw->GetType() == IT_SAMPLE) {
            if (lgptConfigureSavedChopInstrument(viewData, mapped, saved.sampleIndex,
                                                 saved.sourceSize,
                                                 saved.boundaries[chopIndex],
                                                 saved.boundaries[chopIndex + 1])) {
                return mapped;
            }
        }
        saved.chopInstrument[chopIndex] = -1;
    }

    unsigned short nextInstr = bank->Clone((unsigned short)sourceInstrumentIndex);
    if (nextInstr == NO_MORE_INSTRUMENT) {
        lgptSetAssignStatus(status, statusLen, "No free instruments");
        return -1;
    }
    if (!lgptConfigureSavedChopInstrument(viewData, (int)nextInstr, saved.sampleIndex,
                                          saved.sourceSize,
                                          saved.boundaries[chopIndex],
                                          saved.boundaries[chopIndex + 1])) {
        lgptSetAssignStatus(status, statusLen, "Cannot config chop");
        return -1;
    }
    saved.chopInstrument[chopIndex] = (int)nextInstr;
    return (int)nextInstr;
}

static void lgptReconfigureMappedChopInstruments(ViewData *viewData,
                                                 int sourceInstrumentIndex,
                                                 LGPTChopperSavedState &saved) {
    int chopCount = saved.boundaryCount - 1;
    if (chopCount > 32) chopCount = 32;
    for (int i = 0; i < chopCount; i++) {
        int mapped = saved.chopInstrument[i];
        if (mapped >= 0 && mapped < MAX_SAMPLEINSTRUMENT_COUNT) {
            lgptConfigureSavedChopInstrument(viewData, mapped, saved.sampleIndex,
                                             saved.sourceSize,
                                             saved.boundaries[i],
                                             saved.boundaries[i + 1]);
        }
    }
    for (int i = chopCount; i < 32; i++) saved.chopInstrument[i] = -1;
}

bool LGPTChopperAssignSavedChopToPhraseRow(ViewData *viewData,
                                           int phraseIndex,
                                           int row,
                                           int sourceInstrumentIndex,
                                           int requestedChop,
                                           int delta,
                                           bool advanceSessionCursor,
                                           char *status,
                                           int statusLen) {
    lgptSetAssignStatus(status, statusLen, "");
    if (!viewData || !viewData->song_ || !viewData->song_->phrase_ || !viewData->project_) {
        lgptSetAssignStatus(status, statusLen, "No phrase/project");
        return false;
    }
    if (phraseIndex < 0 || phraseIndex >= PHRASE_COUNT || phraseIndex == 0xFE || phraseIndex == 0xFF) {
        phraseIndex = viewData->currentPhrase_;
    }
    if (phraseIndex < 0 || phraseIndex >= PHRASE_COUNT || phraseIndex == 0xFE || phraseIndex == 0xFF) {
        lgptSetAssignStatus(status, statusLen, "No phrase");
        return false;
    }
    if (row < 0 || row > 15) row = viewData->phraseCurPos_;
    if (row < 0 || row > 15) row = 0;

    int sampleIndex = NO_SAMPLE;
    int sourceSize = 0;
    std::string sampleName;
    if (!lgptGetSampleIdentityForInstrument(viewData, sourceInstrumentIndex,
                                            sampleIndex, sampleName, sourceSize)) {
        lgptSetAssignStatus(status, statusLen, "No source sample");
        return false;
    }

    const void *projectKey = viewData->project_ ? (const void *)viewData->project_ : 0;
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot < 0) {
        lgptSetAssignStatus(status, statusLen, "No saved chops");
        return false;
    }

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) {
        lgptSetAssignStatus(status, statusLen, "No chops");
        return false;
    }
    if (chopCount > 32) chopCount = 32;

    int currentInstr = viewData->song_->phrase_->instr_[16 * phraseIndex + row];
    int currentChop = lgptFindChopForInstrument(saved, currentInstr);
    int chopIndex = requestedChop;
    if (chopIndex < 0) {
        if (currentChop >= 0) chopIndex = currentChop + delta;
        else chopIndex = saved.selectedChop + delta;
    }
    while (chopIndex < 0) chopIndex += chopCount;
    chopIndex %= chopCount;

    int assignedInstrument = lgptEnsureChopInstrument(viewData, saved, sourceInstrumentIndex,
                                                      chopIndex, status, statusLen);
    if (assignedInstrument < 0) return false;

    Phrase *phrase = viewData->song_->phrase_;
    int offset = 16 * phraseIndex + row;
    phrase->note_[offset] = 60; /* C-3 remains the neutral trigger; pitch belongs in PTCH/FX. */
    phrase->instr_[offset] = (unsigned char)assignedInstrument;
    phrase->cmd1_[offset] = I_CMD_NONE;
    phrase->param1_[offset] = 0;
    phrase->cmd2_[offset] = I_CMD_NONE;
    phrase->param2_[offset] = 0;

    saved.selectedChop = chopIndex;
    viewData->currentInstrument_ = assignedInstrument;
    if (advanceSessionCursor) {
        int nextRow = row + 1;
        if (nextRow > 15) nextRow = 15;
        viewData->phraseCurPos_ = nextRow;
    } else {
        viewData->phraseCurPos_ = row;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "Chop %02d -> P%02X R%02X I%02X", chopIndex, phraseIndex, row, assignedInstrument);
    lgptSetAssignStatus(status, statusLen, msg);
    return true;
}

int LGPTChopperGetSavedChopCountForInstrument(ViewData *viewData,
                                               int sourceInstrumentIndex) {
    int sampleIndex = NO_SAMPLE;
    int sourceSize = 0;
    std::string sampleName;
    if (!lgptGetSampleIdentityForInstrument(viewData, sourceInstrumentIndex,
                                            sampleIndex, sampleName, sourceSize)) return 0;
    const void *projectKey = (viewData && viewData->project_) ? (const void *)viewData->project_ : 0;
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot < 0) return 0;
    int count = g_lgptChopperSavedStates[slot].boundaryCount - 1;
    return count > 0 ? count : 0;
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
static int g_chopperViewStartFrame = 0;
static int g_chopperViewFrameCount = 0;
static int g_chopperPreviewActive = 0;
static int g_chopperPreviewStartFrame = 0;
static int g_chopperPreviewEndFrame = 0;

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

    int drawFrame = -1;
    if (g_chopperPreviewActive && Player::GetInstance()->IsStreaming()) {
        drawFrame = Player::GetInstance()->GetStreamingPosition();
        if (drawFrame < g_chopperPreviewStartFrame || drawFrame > g_chopperPreviewEndFrame) {
            drawFrame = -1;
        }
    }

    int cx = TF_WAVE_X + g_chopperCursorPx;
    if (cx < TF_WAVE_X) cx = TF_WAVE_X;
    if (cx >= TF_WAVE_X + TF_WAVE_W) cx = TF_WAVE_X + TF_WAVE_W - 1;

    if (drawFrame >= 0 && g_chopperViewFrameCount > 1) {
        long long rel = (long long)(drawFrame - g_chopperViewStartFrame) * (long long)(TF_WAVE_W - 1);
        rel /= (long long)(g_chopperViewFrameCount - 1);
        int playPx = (int)rel;
        if (playPx >= 0 && playPx < TF_WAVE_W) {
            int px = TF_WAVE_X + playPx;
            tf_vline(px, TF_WAVE_Y - 4, TF_WAVE_Y + TF_WAVE_H + 4, cursor);
            tf_rect(px - 2, TF_WAVE_Y - 4, 5, 1, marker);
            tf_rect(px - 2, TF_WAVE_Y + TF_WAVE_H + 3, 5, 1, marker);
        } else {
            tf_vline(cx, TF_WAVE_Y - 4, TF_WAVE_Y + TF_WAVE_H + 4, cursor);
            tf_rect(cx - 2, TF_WAVE_Y - 4, 5, 1, marker);
            tf_rect(cx - 2, TF_WAVE_Y + TF_WAVE_H + 3, 5, 1, marker);
        }
    } else {
        tf_vline(cx, TF_WAVE_Y - 4, TF_WAVE_Y + TF_WAVE_H + 4, cursor);
        tf_rect(cx - 2, TF_WAVE_Y - 4, 5, 1, marker);
        tf_rect(cx - 2, TF_WAVE_Y + TF_WAVE_H + 3, 5, 1, marker);
    }
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
      previewActive_(false),
      previewStartFrame_(0),
      previewEndFrame_(0),
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
    bool wasNewSlotForSample = !(saved.used &&
                                 saved.projectKey == projectKey &&
                                 saved.sampleIndex == sampleIndex_ &&
                                 saved.sourceSize == sourceSize_ &&
                                 saved.sampleName == sampleName_);
    saved.used = true;
    saved.projectKey = projectKey;
    saved.sampleIndex = sampleIndex_;
    saved.sourceSize = sourceSize_;
    saved.sampleName = sampleName_;
    saved.boundaryCount = clampInt(boundaryCount_, 2, MAX_CHOP_BOUNDARIES);
    saved.selectedChop = clampInt(selectedChop_, 0, saved.boundaryCount - 2);
    for (int i = 0; i < saved.boundaryCount; i++) saved.boundaries[i] = boundaries_[i];
    for (int i = saved.boundaryCount; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) saved.boundaries[i] = 0;
    if (wasNewSlotForSample) { for (int i = 0; i < 32; i++) saved.chopInstrument[i] = -1; }
    lgptReconfigureMappedChopInstruments(viewData_, instrumentIndex_, saved);
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
    g_chopperViewStartFrame = viewStartFrame_;
    g_chopperViewFrameCount = getViewFrameCount();
    g_chopperPreviewActive = previewActive_ ? 1 : 0;
    g_chopperPreviewStartFrame = previewStartFrame_;
    g_chopperPreviewEndFrame = previewEndFrame_;
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
    g_chopperPreviewActive = 0;
#endif
}

void SampleChopperModal::setPreviewPlaybackRange(int startFrame, int endFrame) {
    previewActive_ = true;
    previewStartFrame_ = clampInt(startFrame, 0, sourceSize_ > 0 ? sourceSize_ - 1 : 0);
    previewEndFrame_ = clampInt(endFrame, previewStartFrame_, sourceSize_ > 0 ? sourceSize_ - 1 : previewStartFrame_);
#if defined(PLATFORM_TREEFROG)
    g_chopperPreviewActive = 1;
    g_chopperPreviewStartFrame = previewStartFrame_;
    g_chopperPreviewEndFrame = previewEndFrame_;
#endif
    publishOverlayState();
}

void SampleChopperModal::clearPreviewPlaybackRange() {
    previewActive_ = false;
    previewStartFrame_ = 0;
    previewEndFrame_ = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperPreviewActive = 0;
    g_chopperPreviewStartFrame = 0;
    g_chopperPreviewEndFrame = 0;
#endif
    publishOverlayState();
}

void SampleChopperModal::assignSelectedChopToPhrase() {
    if (!hasAssignedSample() || sourceSize_ <= 1) {
        setStatus("No sample to assign");
        return;
    }
    initializeChopsIfNeeded();
    if (!hasUserChops()) {
        setStatus("Add cuts first");
        return;
    }
    saveChopStateForCurrentSample();
    if (!ensureCurrentPhraseSlot()) return;
    int phraseIndex = viewData_->currentPhrase_;
    int row = viewData_->phraseCurPos_;
    if (row < 0 || row > 15) row = 0;
    char status[64];
    if (LGPTChopperAssignSavedChopToPhraseRow(viewData_, phraseIndex, row,
                                              instrumentIndex_, selectedChop_, 0,
                                              true, status, sizeof(status))) {
        setStatus(status);
    } else {
        setStatus(status[0] ? status : "Assign failed");
    }
    isDirty_ = true;
}

void SampleChopperModal::playFromFrame(int frame, const char *label) {
    if (!hasAssignedSample() || samplePath_.empty()) { setStatus("No sample to play"); return; }
    if (sourceSize_ > 0) frame = clampInt(frame, 0, sourceSize_ - 1);
    else frame = 0;
    Path path(samplePath_.c_str());
    Player::GetInstance()->StopStreaming();
    Player::GetInstance()->StartStreamingAt(path, frame);
    playbackTriggered_ = true;
    setPreviewPlaybackRange(frame, sourceSize_ > 0 ? sourceSize_ - 1 : frame);
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
    setPreviewPlaybackRange(startFrame, endFrame);
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

void SampleChopperModal::stopSamplePreview() { if (playbackTriggered_) { Player::GetInstance()->StopStreaming(); playbackTriggered_ = false; } clearPreviewPlaybackRange(); }
void SampleChopperModal::drawStringAbs(int x, int y, const char *txt, GUITextProperties &props) { View::DrawString(x, y, txt, props); }
void SampleChopperModal::clearTextScreen() { View::ClearRect(0, 0, SCREEN_W, SCREEN_H); }
void SampleChopperModal::drawTopBar(GUITextProperties &props) { props.invert_ = true; SetColor(CD_HILITE1); drawStringAbs(0, 0, " P G  SCPI  M TT       CHOPPER       ", props); props.invert_ = false; }

void SampleChopperModal::drawFrame(GUITextProperties &props) {
    SetColor(CD_BORDER);
    drawStringAbs(0, 1,  "+--------------------------------------+", props);
    for (int y = 2; y < 22; y++) { drawStringAbs(0, y, "|", props); drawStringAbs(39, y, "|", props); }
    drawStringAbs(0, 22, "+--------------------------------------+", props);
    SetColor(CD_HILITE2);
    drawStringAbs(2, 2, "Graphical Chopper U2.11", props);
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
    drawStringAbs(0, 25, "A cut Y del B play R1+A assign", props);
    drawStringAbs(0, 26, "R2+LR chop  R2+A full  L2+B stop", props);
    SetColor(CD_HILITE1);
    drawStringAbs(0, 28, trimMode_ ? "TRIM: A+LR start B+LR end Y del L2+Y exit" : "L2+Y trim  L1+B back  Phr:R2+LR", props);
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
       R1+LEFT/RIGHT browses loaded project samples; R1+A assigns selected chop to the current phrase row.
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
        assignSelectedChopToPhrase();
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


__LGPT_U211_CPP__

python3 - <<'__LGPT_U211_PATCH_PY__'
from pathlib import Path
import re

ROOT = Path('.')

def require(path):
    p = ROOT / path
    if not p.exists():
        raise SystemExit('Missing required file: %s' % path)
    return p

def write_if_changed(p, s):
    old = p.read_text()
    if old != s:
        p.write_text(s)

def replace_once(s, old, new, label):
    if old not in s:
        raise SystemExit('Patch failed: missing %s' % label)
    return s.replace(old, new, 1)

# --- AudioFileStreamer: range playback + live stream position access ---
p = require('sources/Application/Audio/AudioFileStreamer.h')
s = p.read_text()
if 'StartRangeAt' not in s:
    s = replace_once(s, 'bool StartAt(const Path &, int startFrame) ;',
                     'bool StartAt(const Path &, int startFrame) ;\n\tbool StartRangeAt(const Path &, int startFrame, int endFrame) ;',
                     'AudioFileStreamer.h StartAt declaration')
if 'bool IsPlaying() const' not in s:
    s = replace_once(s, 'void Stop() ;',
                     'void Stop() ;\n\tbool IsPlaying() const ;\n\tint GetPosition() const ;\n\tint GetStartFrame() const ;\n\tint GetEndFrame() const ;',
                     'AudioFileStreamer.h Stop declaration')
if 'int endFrame_' not in s:
    s = replace_once(s, 'int startFrame_ ;', 'int startFrame_ ;\n\tint endFrame_ ;',
                     'AudioFileStreamer.h startFrame member')
write_if_changed(p, s)

p = require('sources/Application/Audio/AudioFileStreamer.cpp')
s = p.read_text()

# Constructor: add endFrame_ init only inside AudioFileStreamer::AudioFileStreamer().
ctor = re.search(r'AudioFileStreamer::AudioFileStreamer\s*\(\s*\)\s*\{(?P<body>.*?)\n\}\s*;', s, flags=re.S)
if not ctor:
    raise SystemExit('Patch failed: AudioFileStreamer.cpp constructor block')
if 'endFrame_' not in ctor.group('body'):
    body = ctor.group('body')
    if 'startFrame_=0 ;' not in body:
        raise SystemExit('Patch failed: AudioFileStreamer.cpp constructor startFrame')
    new_body = body.replace('startFrame_=0 ;', 'startFrame_=0 ;\n\tendFrame_=-1 ;', 1)
    s = s[:ctor.start('body')] + new_body + s[ctor.end('body'):]

# StartAt / StartRangeAt: tolerate clean trees and partially patched trees.
if 'AudioFileStreamer::StartRangeAt' not in s:
    pattern = r'bool\s+AudioFileStreamer::StartAt\s*\(\s*const Path &path\s*,\s*int startFrame\s*\)\s*\{.*?\n\}\s*;'
    repl = '''bool AudioFileStreamer::StartAt(const Path &path,int startFrame) {\n\treturn StartRangeAt(path,startFrame,-1) ;\n} ;\n\nbool AudioFileStreamer::StartRangeAt(const Path &path,int startFrame,int endFrame) {\n\tTrace::Debug("Starting to stream %s at frame %d",path.GetPath().c_str(),startFrame);\n\tpath_=path ;\n\tconst char *shift=Config::GetInstance()->GetValue("PRELISTENATTENUATION") ;\n\tshift_=(shift)?atoi(shift):1 ;\n\tif (shift_<0) shift_=0 ;\n\tif (shift_>12) shift_=12 ;\n\tTrace::Debug("Streaming shift is %d",shift_);\n\tstartFrame_=(startFrame<0)?0:startFrame ;\n\tposition_=startFrame_ ;\n\tendFrame_=endFrame ;\n\tnewPath_=true ;\n\tmode_=AFSM_PLAYING ;\n\treturn true ;\n} ;'''
    s2 = re.sub(pattern, repl, s, flags=re.S)
    if s2 == s:
        raise SystemExit('Patch failed: AudioFileStreamer.cpp StartAt block')
    s = s2
else:
    # Older failed U2.11 attempts may have StartRangeAt but may miss position_=startFrame_.
    sr = re.search(r'bool\s+AudioFileStreamer::StartRangeAt\s*\(\s*const Path &path\s*,\s*int startFrame\s*,\s*int endFrame\s*\)\s*\{(?P<body>.*?)\n\}\s*;', s, flags=re.S)
    if sr and 'position_=startFrame_' not in sr.group('body'):
        body = sr.group('body')
        if 'startFrame_=(startFrame<0)?0:startFrame ;' in body:
            body = body.replace('startFrame_=(startFrame<0)?0:startFrame ;', 'startFrame_=(startFrame<0)?0:startFrame ;\n\tposition_=startFrame_ ;', 1)
            s = s[:sr.start('body')] + body + s[sr.end('body'):]

# Stop + live position accessors: canonicalize Stop with a regex so partial trees work.
stop_re = r'void\s+AudioFileStreamer::Stop\s*\(\s*\)\s*\{(?P<body>.*?)\n\}\s*;'
if 'bool AudioFileStreamer::IsPlaying() const' not in s:
    stop_pattern = r'void\s+AudioFileStreamer::Stop\s*\(\s*\)\s*\{.*?\n\}\s*;'
    stop_new = '''void AudioFileStreamer::Stop() {\n\tmode_=AFSM_STOPPED ;\n\tnewPath_=false ;\n\tendFrame_=-1 ;\n\tTrace::Debug("Streaming stopped");\n} ;\n\nbool AudioFileStreamer::IsPlaying() const { return mode_==AFSM_PLAYING ; }\nint AudioFileStreamer::GetPosition() const { return position_ ; }\nint AudioFileStreamer::GetStartFrame() const { return startFrame_ ; }\nint AudioFileStreamer::GetEndFrame() const { return endFrame_ ; }'''
    s2 = re.sub(stop_pattern, stop_new, s, count=1, flags=re.S)
    if s2 == s:
        raise SystemExit('Patch failed: AudioFileStreamer.cpp Stop block regex')
    s = s2
else:
    stop = re.search(stop_re, s, flags=re.S)
    if stop and 'endFrame_=-1' not in stop.group('body'):
        body = stop.group('body')
        if 'newPath_=false ;' in body:
            body = body.replace('newPath_=false ;', 'newPath_=false ;\n\tendFrame_=-1 ;', 1)
            s = s[:stop.start('body')] + body + s[stop.end('body'):]

# Render range stop logic: replace the original remaining block; skip if already patched.
if 'long effectiveEnd=size-1 ;' not in s:
    old = '''\tlong remaining=size-position_ ;\n\tif (size<=0 || remaining<=0) {\n\t\tmode_=AFSM_STOPPED ;\n\t\tmemset(buffer,0,2*samplecount*sizeof(fixed)) ;\n\t\treturn false ;\n\t}\n\n\tint count=samplecount ;\n\tif (remaining<samplecount) {\n\t\tcount=(int)remaining ;\n\t\tmode_=AFSM_STOPPED ;\n\t}\n'''
    new_remaining = '''\tlong effectiveEnd=size-1 ;\n\tif (endFrame_>=0 && endFrame_<effectiveEnd) effectiveEnd=endFrame_ ;\n\tif (size<=0 || position_>effectiveEnd) {\n\t\tmode_=AFSM_STOPPED ;\n\t\tmemset(buffer,0,2*samplecount*sizeof(fixed)) ;\n\t\treturn false ;\n\t}\n\tlong remaining=effectiveEnd-position_+1 ;\n\tif (remaining<=0) {\n\t\tmode_=AFSM_STOPPED ;\n\t\tmemset(buffer,0,2*samplecount*sizeof(fixed)) ;\n\t\treturn false ;\n\t}\n\n\tint count=samplecount ;\n\tif (remaining<samplecount) {\n\t\tcount=(int)remaining ;\n\t\tmode_=AFSM_STOPPED ;\n\t}\n'''
    if old in s:
        s = s.replace(old, new_remaining, 1)
    else:
        pattern = r'\tlong\s+remaining\s*=\s*size\s*-\s*position_\s*;.*?\n\tif\s*\(\s*remaining\s*<\s*samplecount\s*\)\s*\{.*?\n\t\}\n'
        s2 = re.sub(pattern, new_remaining, s, count=1, flags=re.S)
        if s2 == s:
            raise SystemExit('Patch failed: AudioFileStreamer.cpp Render remaining block')
        s = s2
write_if_changed(p, s)

# --- PlayerMixer stream wrappers ---
p = require('sources/Application/Player/PlayerMixer.h')
s = p.read_text()
if 'StartStreamingRangeAt' not in s:
    s = replace_once(s, 'void StartStreamingAt(const Path &, int startFrame) ;',
                     'void StartStreamingAt(const Path &, int startFrame) ;\n\tvoid StartStreamingRangeAt(const Path &, int startFrame, int endFrame) ;',
                     'PlayerMixer.h StartStreamingAt declaration')
if 'bool IsStreaming()' not in s:
    s = replace_once(s, 'void StopStreaming()  ;',
                     'void StopStreaming()  ;\n\tbool IsStreaming() ;\n\tint GetStreamingPosition() ;\n\tint GetStreamingStartFrame() ;\n\tint GetStreamingEndFrame() ;',
                     'PlayerMixer.h StopStreaming declaration')
write_if_changed(p, s)

p = require('sources/Application/Player/PlayerMixer.cpp')
s = p.read_text()
if 'PlayerMixer::StartStreamingRangeAt' not in s:
    s = replace_once(s, '''void PlayerMixer::StartStreamingAt(const Path &path,int startFrame) {\n\tfileStreamer_.StartAt(path,startFrame) ;\n} ;''',
                     '''void PlayerMixer::StartStreamingAt(const Path &path,int startFrame) {\n\tfileStreamer_.StartAt(path,startFrame) ;\n} ;\n\nvoid PlayerMixer::StartStreamingRangeAt(const Path &path,int startFrame,int endFrame) {\n\tfileStreamer_.StartRangeAt(path,startFrame,endFrame) ;\n} ;''',
                     'PlayerMixer.cpp StartStreamingAt block')
if 'PlayerMixer::IsStreaming' not in s:
    s = replace_once(s, '''void PlayerMixer::StopStreaming() {\n\tfileStreamer_.Stop() ;\n} ;''',
                     '''void PlayerMixer::StopStreaming() {\n\tfileStreamer_.Stop() ;\n} ;\n\nbool PlayerMixer::IsStreaming() { return fileStreamer_.IsPlaying() ; }\nint PlayerMixer::GetStreamingPosition() { return fileStreamer_.GetPosition() ; }\nint PlayerMixer::GetStreamingStartFrame() { return fileStreamer_.GetStartFrame() ; }\nint PlayerMixer::GetStreamingEndFrame() { return fileStreamer_.GetEndFrame() ; }''',
                     'PlayerMixer.cpp StopStreaming block')
write_if_changed(p, s)

# --- Player stream wrappers ---
p = require('sources/Application/Player/Player.h')
s = p.read_text()
if 'StartStreamingRangeAt' not in s:
    s = replace_once(s, 'void StartStreamingAt(const Path &path,int startFrame) ;',
                     'void StartStreamingAt(const Path &path,int startFrame) ;\n\tvoid StartStreamingRangeAt(const Path &path,int startFrame,int endFrame) ;',
                     'Player.h StartStreamingAt declaration')
if 'bool IsStreaming()' not in s:
    s = replace_once(s, 'void StopStreaming() ;',
                     'void StopStreaming() ;\n\tbool IsStreaming() ;\n\tint GetStreamingPosition() ;\n\tint GetStreamingStartFrame() ;\n\tint GetStreamingEndFrame() ;',
                     'Player.h StopStreaming declaration')
write_if_changed(p, s)

p = require('sources/Application/Player/Player.cpp')
s = p.read_text()
if 'Player::StartStreamingRangeAt' not in s:
    old = '''void Player::StartStreamingAt(const Path &path,int startFrame) {\n#if defined(PLATFORM_TREEFROG)\n    TreeFrogAudioSetPlaybackArmed(1);\n#endif\n\n\tmixer_->StartStreamingAt(path,startFrame) ;\n}'''
    new = '''void Player::StartStreamingAt(const Path &path,int startFrame) {\n#if defined(PLATFORM_TREEFROG)\n    TreeFrogAudioSetPlaybackArmed(1);\n#endif\n\n\tmixer_->StartStreamingAt(path,startFrame) ;\n}\n\nvoid Player::StartStreamingRangeAt(const Path &path,int startFrame,int endFrame) {\n#if defined(PLATFORM_TREEFROG)\n    TreeFrogAudioSetPlaybackArmed(1);\n#endif\n\n\tmixer_->StartStreamingRangeAt(path,startFrame,endFrame) ;\n}'''
    s = replace_once(s, old, new, 'Player.cpp StartStreamingAt block')
if 'Player::IsStreaming' not in s:
    old = '''void Player::StopStreaming() {\n#if defined(PLATFORM_TREEFROG)\n    /* TREEFROG_V1_2_LISTEN_STREAM_ARM: disarm after stream preview only when\n       the sequencer itself is not running. This avoids muting normal playback. */\n    if (!isRunning_) {\n        TreeFrogAudioSetPlaybackArmed(0);\n    }\n#endif\n\n\tmixer_->StopStreaming() ;\n}'''
    new = '''void Player::StopStreaming() {\n#if defined(PLATFORM_TREEFROG)\n    /* TREEFROG_V1_2_LISTEN_STREAM_ARM: disarm after stream preview only when\n       the sequencer itself is not running. This avoids muting normal playback. */\n    if (!isRunning_) {\n        TreeFrogAudioSetPlaybackArmed(0);\n    }\n#endif\n\n\tmixer_->StopStreaming() ;\n}\n\nbool Player::IsStreaming() { return mixer_->IsStreaming() ; }\nint Player::GetStreamingPosition() { return mixer_->GetStreamingPosition() ; }\nint Player::GetStreamingStartFrame() { return mixer_->GetStreamingStartFrame() ; }\nint Player::GetStreamingEndFrame() { return mixer_->GetStreamingEndFrame() ; }'''
    s = replace_once(s, old, new, 'Player.cpp StopStreaming block')
write_if_changed(p, s)

# --- PhraseView: R2 note/instrument column assignment surface ---
p = require('sources/Application/Views/PhraseView.h')
s = p.read_text()
if 'assignChopFromPhrase' not in s:
    s = replace_once(s, 'int findClosestInstrumentFor(int);',
                     'int findClosestInstrumentFor(int);\n    int getChopSourceInstrumentForCurrentRow();\n    bool assignChopFromPhrase(int delta, bool advanceRow);',
                     'PhraseView.h private helper declarations')
write_if_changed(p, s)

p = require('sources/Application/Views/PhraseView.cpp')
s = p.read_text()
if 'ModalDialogs/SampleChopperModal.h' not in s:
    s = replace_once(s, '#include "Application/Views/CommandSelectorCommon.h"',
                     '#include "Application/Views/CommandSelectorCommon.h"\n#include "ModalDialogs/SampleChopperModal.h"',
                     'PhraseView.cpp includes')
if 'PhraseView::assignChopFromPhrase' not in s:
    helper = '''\nint PhraseView::getChopSourceInstrumentForCurrentRow() {\n    int offset = 16 * viewData_->currentPhrase_ + row_;\n    unsigned char rowInstr = phrase_->instr_[offset];\n    if (rowInstr != 0xFF) return rowInstr;\n    if (viewData_->currentInstrument_ != 0xFF) return viewData_->currentInstrument_;\n    if (lastInstr_ != 0xFF) return lastInstr_;\n    return -1;\n}\n\nbool PhraseView::assignChopFromPhrase(int delta, bool advanceRow) {\n    if (col_ != 0 && col_ != 1) {\n        View::SetNotification("Chop assign: use note/instr column");\n        return false;\n    }\n    int sourceInstrument = getChopSourceInstrumentForCurrentRow();\n    char status[64];\n    bool ok = LGPTChopperAssignSavedChopToPhraseRow(viewData_, viewData_->currentPhrase_, row_,\n                                                    sourceInstrument, -1, delta, false,\n                                                    status, sizeof(status));\n    if (status[0]) View::SetNotification(status);\n    if (ok) {\n        lastNote_ = 60;\n        lastInstr_ = viewData_->currentInstrument_;\n        if (advanceRow) updateCursor(0, 1);\n        else updateCursor(0, 0);\n        isDirty_ = true;\n    }\n    return ok;\n}\n'''
    s = replace_once(s, 'int PhraseView::findClosestInstrumentFor(int row) {',
                     helper + '\nint PhraseView::findClosestInstrumentFor(int row) {',
                     'PhraseView.cpp findClosestInstrumentFor anchor')
if 'R2+LEFT/RIGHT assigns saved chops' not in s:
    needle = '''    Player *player = Player::GetInstance();\n\n    if (mask & EPBM_B) {'''
    repl = '''    Player *player = Player::GetInstance();\n\n    // U2.11: R2+LEFT/RIGHT assigns saved chops from the note/instrument column.\n    // The note stays C-3 as a neutral trigger; pitch belongs in PTCH/ARPG/FX.\n    if ((mask & EPBM_R2) && ((col_ == 0) || (col_ == 1))) {\n        if (mask & EPBM_LEFT) { assignChopFromPhrase(-1, false); return; }\n        if (mask & EPBM_RIGHT) { assignChopFromPhrase(1, false); return; }\n        if (mask & EPBM_UP) { assignChopFromPhrase(-4, false); return; }\n        if (mask & EPBM_DOWN) { assignChopFromPhrase(4, false); return; }\n        if (mask & EPBM_A) { assignChopFromPhrase(0, true); return; }\n    }\n\n    if (mask & EPBM_B) {'''
    s = replace_once(s, needle, repl, 'PhraseView.cpp processNormalButtonMask R2 anchor')
write_if_changed(p, s)

# Ensure object is in Makefile for restored base trees.
p = require('projects/Makefile')
s = p.read_text()
if 'SampleChopperModal.o' not in s:
    s = replace_once(s, 'ImportSampleDialog.o ', 'ImportSampleDialog.o SampleChopperModal.o ',
                     'projects/Makefile SampleChopperModal.o insertion')
write_if_changed(p, s)

print('U2.11 patches applied.')

__LGPT_U211_PATCH_PY__

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so 2>/dev/null || true

LOG="BUILD_U2_11_CHOPPER_PLAYHEAD_SELECTIVE_ASSIGN_$STAMP.log"
echo "Starting U2.11 build..."
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
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,240p' || true
  tail -n 140 "$LOG" || true
fi
exit "$RC"
