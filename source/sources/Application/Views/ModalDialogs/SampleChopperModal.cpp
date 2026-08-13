#include "SampleChopperModal.h"
#include "Application/UI/Input/ChordResolver.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SoundSource.h"
#include "Application/Instruments/WavFile.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Player/Player.h"
#include "Application/Views/BaseClasses/UiDraw.h"
#include "System/FileSystem/FileSystem.h"
#include "Services/Time/TimeService.h"
#include <unistd.h>
#if defined(PLATFORM_TREEFROG)
#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
extern "C" void TreeFrogForceVideoRefresh(void);
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sstream>

extern "C"
__attribute__((used, visibility("default")))
const char *LgptU2510GlobalChopperHistoryBuildMarker(void) {
    return "U2510_GLOBAL_CHOPPER_HISTORY_24_OVERLAY_SAFE";
}

/* U2.11: lightweight in-session chop persistence + selective phrase assignment.
   This deliberately avoids changing LGPT project serialization while the chopper UI is still evolving.
   Boundaries are keyed by project pointer + sample index/name/size, so closing and reopening the modal
   during the same project session restores cuts and allows later selective phrase assignment. */
static const int LGPT_CHOPPER_SAVED_STATE_COUNT = 128;
static const int LGPT_CHOPPER_SAVED_BOUNDARIES = 101;

static std::string g_lgptLastDestructiveEditSamplePath;
static std::string g_lgptLastDestructiveEditUndoPath;
static std::string g_lgptLastDestructiveEditRedoPath;
static std::string g_lgptLastDestructiveEditAction;
static int g_lgptLastDestructiveEditSampleIndex = -1;
static bool g_lgptLastDestructiveEditUndone = false;
static int g_lgptLastDestructiveEditUndoSize = 0;
static int g_lgptLastDestructiveEditRedoSize = 0;
static int g_lgptLastDestructiveEditUndoSelected = 0;
static int g_lgptLastDestructiveEditRedoSelected = 0;
static int g_lgptLastDestructiveEditUndoBoundaryCount = 0;
static int g_lgptLastDestructiveEditRedoBoundaryCount = 0;
static int g_lgptLastDestructiveEditUndoBoundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];
static int g_lgptLastDestructiveEditRedoBoundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];

static short *g_lgptPhysicalUndoSamples = 0;
static short *g_lgptPhysicalRedoSamples = 0;
static int g_lgptPhysicalUndoFrames = 0;
static int g_lgptPhysicalRedoFrames = 0;

/* TREEFROG_U2_39_CHOPPER_ZOMBIE_VOICE_GUARD (Bacon 1.2.1): any destructive
   edit that will ReplaceBuffer() the shared WAV must halt ALL audio first
   (pattern voices + streaming preview), same rule as Crop/Delete. */
static void lgptStopAllAudioBeforeDestructiveEdit();
static int g_lgptPhysicalUndoChannels = 0;
static int g_lgptPhysicalRedoChannels = 0;
static int g_lgptPhysicalUndoRate = 0;
static int g_lgptPhysicalRedoRate = 0;

static void lgptFreePhysicalSnapshot(short *&buffer, int &frames, int &channels, int &rate) {
    if (buffer) free(buffer);
    buffer = 0;
    frames = 0;
    channels = 0;
    rate = 0;
}

static bool lgptCapturePhysicalSnapshot(SoundSource *source,
                                        short *&buffer,
                                        int &frames,
                                        int &channels,
                                        int &rate) {
    lgptFreePhysicalSnapshot(buffer, frames, channels, rate);
    if (!source) return false;
    frames = source->GetSize(-1);
    channels = source->GetChannelCount(-1);
    rate = source->GetSampleRate(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || frames <= 1 || channels <= 0 || rate <= 0) {
        frames = 0; channels = 0; rate = 0;
        return false;
    }
    int words = frames * channels;
    buffer = (short *)malloc(words * sizeof(short));
    if (!buffer) {
        frames = 0; channels = 0; rate = 0;
        return false;
    }
    memcpy(buffer, samples, words * sizeof(short));
    return true;
}

static bool lgptRestorePhysicalSnapshotToWav(WavFile *wav,
                                             const char *path,
                                             short *buffer,
                                             int frames,
                                             int channels,
                                             int rate) {
    if (!wav || !path || !buffer || frames <= 1 || channels <= 0 || rate <= 0) return false;
    if (!wav->ReplaceBuffer(buffer, frames, channels, rate)) return false;
    return wav->SaveBufferToPath(path);
}

static void lgptStoreDestructiveEditSnapshot(int *dstBoundaries,
                                            int &dstCount,
                                            int &dstSelected,
                                            int &dstSize,
                                            const int *srcBoundaries,
                                            int srcCount,
                                            int selected,
                                            int size) {
    dstCount = srcCount;
    if (dstCount < 0) dstCount = 0;
    if (dstCount > LGPT_CHOPPER_SAVED_BOUNDARIES) dstCount = LGPT_CHOPPER_SAVED_BOUNDARIES;
    dstSelected = selected;
    dstSize = size;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) dstBoundaries[i] = 0;
    for (int i = 0; i < dstCount; i++) dstBoundaries[i] = srcBoundaries[i];
}

static void lgptBeginDestructiveEdit(const std::string &samplePath,
                                     int sampleIndex,
                                     const char *action,
                                     const int *boundaries,
                                     int boundaryCount,
                                     int selectedChop,
                                     int sourceSize) {
    lgptFreePhysicalSnapshot(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames, g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
    lgptFreePhysicalSnapshot(g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames, g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate);
    g_lgptLastDestructiveEditSamplePath = samplePath;
    g_lgptLastDestructiveEditUndoPath = samplePath + ".u2undo";
    g_lgptLastDestructiveEditRedoPath = samplePath + ".u2redo";
    g_lgptLastDestructiveEditAction = action ? action : "Edit";
    g_lgptLastDestructiveEditSampleIndex = sampleIndex;
    g_lgptLastDestructiveEditUndone = false;
    lgptStoreDestructiveEditSnapshot(g_lgptLastDestructiveEditUndoBoundaries,
                                     g_lgptLastDestructiveEditUndoBoundaryCount,
                                     g_lgptLastDestructiveEditUndoSelected,
                                     g_lgptLastDestructiveEditUndoSize,
                                     boundaries, boundaryCount, selectedChop, sourceSize);
}

static void lgptFinishDestructiveEdit(const int *boundaries,
                                      int boundaryCount,
                                      int selectedChop,
                                      int sourceSize) {
    lgptStoreDestructiveEditSnapshot(g_lgptLastDestructiveEditRedoBoundaries,
                                     g_lgptLastDestructiveEditRedoBoundaryCount,
                                     g_lgptLastDestructiveEditRedoSelected,
                                     g_lgptLastDestructiveEditRedoSize,
                                     boundaries, boundaryCount, selectedChop, sourceSize);
    g_lgptLastDestructiveEditUndone = false;
}

static void lgptClearDestructiveEditHistory(void) {
    lgptFreePhysicalSnapshot(
        g_lgptPhysicalUndoSamples,
        g_lgptPhysicalUndoFrames,
        g_lgptPhysicalUndoChannels,
        g_lgptPhysicalUndoRate);
    lgptFreePhysicalSnapshot(
        g_lgptPhysicalRedoSamples,
        g_lgptPhysicalRedoFrames,
        g_lgptPhysicalRedoChannels,
        g_lgptPhysicalRedoRate);
    g_lgptLastDestructiveEditSamplePath.clear();
    g_lgptLastDestructiveEditUndoPath.clear();
    g_lgptLastDestructiveEditRedoPath.clear();
    g_lgptLastDestructiveEditAction.clear();
    g_lgptLastDestructiveEditSampleIndex = -1;
    g_lgptLastDestructiveEditUndone = false;
    g_lgptLastDestructiveEditUndoSize = 0;
    g_lgptLastDestructiveEditRedoSize = 0;
    g_lgptLastDestructiveEditUndoSelected = 0;
    g_lgptLastDestructiveEditRedoSelected = 0;
    g_lgptLastDestructiveEditUndoBoundaryCount = 0;
    g_lgptLastDestructiveEditRedoBoundaryCount = 0;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; ++i) {
        g_lgptLastDestructiveEditUndoBoundaries[i] = 0;
        g_lgptLastDestructiveEditRedoBoundaries[i] = 0;
    }
}

static bool lgptEndsWithWav(const std::string &name) {
    if (name.size() < 4) return false;
    char a = name[name.size() - 4];
    char b = name[name.size() - 3];
    char c = name[name.size() - 2];
    char d = name[name.size() - 1];
    if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
    if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if (d >= 'A' && d <= 'Z') d = (char)(d + 32);
    return a == '.' && b == 'w' && c == 'a' && d == 'v';
}

struct LGPTChopperSavedState {
    bool used;
    const void *projectKey;
    int sampleIndex;
    int sourceSize;
    int selectedChop;
    int boundaryCount;
    std::string sampleName;
    int boundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];
    int chopInstrument[100];

    LGPTChopperSavedState()
        : used(false), projectKey(0), sampleIndex(-1), sourceSize(0),
          selectedChop(0), boundaryCount(0), sampleName("") {
        for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) boundaries[i] = 0;
        for (int i = 0; i < 100; i++) chopInstrument[i] = -1;
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

static int lgptFindChopperSavedStateLoose(const void *projectKey,
                                          int sampleIndex,
                                          const std::string &sampleName,
                                          int sourceSize) {
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot >= 0) return slot;

    /* U2.14: reopening from Phrase/Instrument can arrive with equivalent sample identity
       but not the exact state lookup tuple. Fallback in stages so session cuts do not vanish. */
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex && s.sourceSize == sourceSize && s.sampleName == sampleName) return i;
    }
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex && s.sourceSize == sourceSize) return i;
    }
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex) return i;
    }
    return -1;
}

static bool lgptSavedStateIsFullRange(const LGPTChopperSavedState &saved) {
    if (saved.boundaryCount != 2) return false;
    if (saved.sourceSize <= 1) return false;
    return saved.boundaries[0] <= 0 && saved.boundaries[1] >= saved.sourceSize - 1;
}

static int lgptFindChopperFreeSavedStateSlot() {
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        if (!g_lgptChopperSavedStates[i].used) return i;
    }
    return 0; /* deterministic eviction; enough for the current R36SX test path */
}

static std::string lgptPersistentChopSidecarPathForSampleName(const std::string &sampleName) {
    if (sampleName.empty()) return "";
    std::string logical = "samples:";
    logical += sampleName;
    Path p(logical.c_str());
    std::string resolved = p.GetPath();
    if (resolved.empty()) return "";
    return resolved + ".u2chop";
}

static bool lgptReadWholeFile(const std::string &path, std::string &out) {
    out.clear();
    if (path.empty()) return false;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(path.c_str(), "r");
    if (!file) return false;
    char buffer[256];
    int n = 0;
    while ((n = file->Read(buffer, 1, sizeof(buffer))) > 0) {
        out.append(buffer, n);
        if (n < (int)sizeof(buffer)) break;
    }
    file->Close();
    delete file;
    return !out.empty();
}

static bool lgptGetSampleIdentityBySampleIndex(int sampleIndex,
                                               std::string &sampleName,
                                               int &sourceSize) {
    sampleName.clear();
    sourceSize = 0;
    if (sampleIndex < 0) return false;
    SamplePool *pool = SamplePool::GetInstance();
    if (!pool) return false;
    char **names = pool->GetNameList();
    int count = pool->GetNameListSize();
    if (!names || sampleIndex >= count || !names[sampleIndex]) return false;
    sampleName = names[sampleIndex];
    SoundSource *source = pool->GetSource(sampleIndex);
    sourceSize = source ? source->GetSize(-1) : 0;
    return !sampleName.empty() && sourceSize > 1;
}

static bool lgptValidatePersistentBoundaries(int *boundaries, int boundaryCount, int sourceSize) {
    if (!boundaries || sourceSize <= 1) return false;
    if (boundaryCount < 2 || boundaryCount > LGPT_CHOPPER_SAVED_BOUNDARIES) return false;
    boundaries[0] = 0;
    boundaries[boundaryCount - 1] = sourceSize - 1;
    int previous = -1;
    for (int i = 0; i < boundaryCount; i++) {
        if (boundaries[i] < 0 || boundaries[i] >= sourceSize || boundaries[i] <= previous) return false;
        previous = boundaries[i];
    }
    return true;
}

static bool lgptLoadPersistentChopState(const void *projectKey,
                                        int sampleIndex,
                                        const std::string &sampleName,
                                        int sourceSize,
                                        int *slotOut) {
    if (slotOut) *slotOut = -1;
    if (sampleIndex < 0 || sampleName.empty() || sourceSize <= 1) return false;
    std::string sidecar = lgptPersistentChopSidecarPathForSampleName(sampleName);
    std::string content;
    if (!lgptReadWholeFile(sidecar, content)) return false;

    std::istringstream iss(content);
    std::string tok;
    int fileSampleIndex = sampleIndex;
    int fileSourceSize = sourceSize;
    int selected = 0;
    int count = 0;
    int temp[LGPT_CHOPPER_SAVED_BOUNDARIES];
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) temp[i] = 0;

    if (!(iss >> tok) || tok != "LGPT_U2_CHOPS_V1") return false;
    while (iss >> tok) {
        if (tok == "sampleIndex") iss >> fileSampleIndex;
        else if (tok == "sourceSize") iss >> fileSourceSize;
        else if (tok == "selected") iss >> selected;
        else if (tok == "boundaryCount") iss >> count;
        else if (tok == "boundaries") {
            for (int i = 0; i < count && i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) iss >> temp[i];
        } else if (tok == "END") break;
    }

    /* Size must match the currently loaded WAV. A destructive crop/pitch writes a new sidecar
       immediately, so a mismatch means stale chop data and must not be trusted. */
    if (fileSourceSize != sourceSize) return false;
    if (!lgptValidatePersistentBoundaries(temp, count, sourceSize)) return false;

    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot < 0) slot = lgptFindChopperFreeSavedStateSlot();
    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    saved.used = true;
    saved.projectKey = projectKey;
    saved.sampleIndex = sampleIndex;
    saved.sourceSize = sourceSize;
    saved.sampleName = sampleName;
    saved.boundaryCount = count;
    saved.selectedChop = selected;
    if (saved.selectedChop < 0) saved.selectedChop = 0;
    if (saved.selectedChop > saved.boundaryCount - 2) saved.selectedChop = saved.boundaryCount - 2;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) saved.boundaries[i] = 0;
    for (int i = 0; i < count; i++) saved.boundaries[i] = temp[i];
    for (int i = 0; i < 100; i++) saved.chopInstrument[i] = -1;
    if (slotOut) *slotOut = slot;
    return true;
}

static bool lgptEnsurePersistentChopStateLoaded(const void *projectKey,
                                                int sampleIndex,
                                                const std::string &sampleName,
                                                int sourceSize,
                                                int *slotOut) {
    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot >= 0) {
        if (slotOut) *slotOut = slot;
        return true;
    }
    return lgptLoadPersistentChopState(projectKey, sampleIndex, sampleName, sourceSize, slotOut);
}

static void lgptWritePersistentChopState(const LGPTChopperSavedState &saved) {
    if (!saved.used || saved.sampleName.empty() || saved.sourceSize <= 1 || saved.boundaryCount < 2) return;
    std::string sidecar = lgptPersistentChopSidecarPathForSampleName(saved.sampleName);
    if (sidecar.empty()) return;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return;
    I_File *file = fs->Open(sidecar.c_str(), "w");
    if (!file) return;
    file->Printf("LGPT_U2_CHOPS_V1\n");
    file->Printf("sampleIndex %d\n", saved.sampleIndex);
    file->Printf("sourceSize %d\n", saved.sourceSize);
    file->Printf("selected %d\n", saved.selectedChop);
    file->Printf("boundaryCount %d\n", saved.boundaryCount);
    file->Printf("boundaries");
    for (int i = 0; i < saved.boundaryCount; i++) file->Printf(" %d", saved.boundaries[i]);
    file->Printf("\nEND\n");
    file->Close();
    delete file;
    sync();
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
    if (chopCount > 100) chopCount = 100;
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
    if (chopIndex < 0 || chopIndex >= chopCount || chopIndex >= 100) {
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
    if (chopCount > 100) chopCount = 100;
    for (int i = 0; i < chopCount; i++) {
        int mapped = saved.chopInstrument[i];
        if (mapped >= 0 && mapped < MAX_SAMPLEINSTRUMENT_COUNT) {
            lgptConfigureSavedChopInstrument(viewData, mapped, saved.sampleIndex,
                                             saved.sourceSize,
                                             saved.boundaries[i],
                                             saved.boundaries[i + 1]);
        }
    }
    for (int i = chopCount; i < 100; i++) saved.chopInstrument[i] = -1;
}

static void lgptNormalizePhraseRowsForSavedChops(ViewData *viewData,
                                                 int sourceInstrumentIndex,
                                                 const LGPTChopperSavedState &saved) {
    if (!viewData || !viewData->song_ || !viewData->song_->phrase_) return;
    if (sourceInstrumentIndex < 0 || sourceInstrumentIndex >= MAX_SAMPLEINSTRUMENT_COUNT) return;
    if (lgptSavedStateIsFullRange(saved)) return;
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) return;
    if (chopCount > 100) chopCount = 100;
    Phrase *phrase = viewData->song_->phrase_;
    for (int p = 0; p < PHRASE_COUNT; p++) {
        for (int r = 0; r < 16; r++) {
            int offset = 16 * p + r;
            if (phrase->instr_[offset] == sourceInstrumentIndex &&
                phrase->note_[offset] != 0xFF &&
                phrase->note_[offset] < 100 &&
                phrase->note_[offset] >= chopCount) {
                phrase->note_[offset] = 0xFF;
            }
        }
    }
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
    int slot = -1;
    if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex, sampleName, sourceSize, &slot) || slot < 0) {
        lgptSetAssignStatus(status, statusLen, "No saved chops");
        return false;
    }

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) {
        lgptSetAssignStatus(status, statusLen, "No chops");
        return false;
    }
    if (chopCount > 100) chopCount = 100;

    Phrase *phrase = viewData->song_->phrase_;
    int offset = 16 * phraseIndex + row;
    int currentChop = -1;
    int currentInstr = phrase->instr_[offset];
    int currentNote = phrase->note_[offset];
    if (currentInstr == sourceInstrumentIndex && currentNote >= 0 && currentNote < chopCount) {
        currentChop = currentNote;
    }

    int chopIndex = requestedChop;
    if (chopIndex < 0) {
        if (currentChop >= 0) chopIndex = currentChop + delta;
        else chopIndex = saved.selectedChop + delta;
    }
    while (chopIndex < 0) chopIndex += chopCount;
    chopIndex %= chopCount;

    /* U2.12 sampler-tracker model:
       - The phrase instrument remains the original sample instrument, e.g. I05 on every row.
       - The phrase note byte stores the chop index 0..99 and is rendered as S01..S100.
       - SampleInstrument maps S-note triggers to saved chopper boundaries at playback time.
       - Existing command columns are not cleared, so PTCH/ARPG/VOLM/FCUT/etc remain available. */
    phrase->note_[offset] = (unsigned char)chopIndex;
    phrase->instr_[offset] = (unsigned char)sourceInstrumentIndex;
    if (phrase->vol_[offset] == 0xFF) {
        phrase->vol_[offset] = 0x64;
    }

    saved.selectedChop = chopIndex;
    viewData->currentInstrument_ = sourceInstrumentIndex;
    if (advanceSessionCursor) {
        int nextRow = row + 1;
        if (nextRow > 15) nextRow = 15;
        viewData->phraseCurPos_ = nextRow;
    } else {
        viewData->phraseCurPos_ = row;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "S%02d -> P%02X R%02X I%02X", chopIndex + 1, phraseIndex, row, sourceInstrumentIndex);
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
    int slot = -1;
    if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex, sampleName, sourceSize, &slot) || slot < 0) return 0;
    const LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    if (lgptSavedStateIsFullRange(saved)) return 0;
    int count = saved.boundaryCount - 1;
    if (count > 100) count = 100;
    return count > 0 ? count : 0;
}

static int lgptFindSavedStateBySampleIndex(int sampleIndex) {
    if (sampleIndex < 0) return -1;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex && s.boundaryCount >= 2) return i;
    }
    return -1;
}

// TREEFROG_U2_34_SAMPLE_MANAGER_PURGE
// TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
bool LGPTChopperHasSavedChopsForSampleIndex(int sampleIndex) {
    int slot = lgptFindSavedStateBySampleIndex(sampleIndex);
    if (slot < 0) {
        std::string sampleName;
        int sourceSize = 0;
        if (lgptGetSampleIdentityBySampleIndex(sampleIndex, sampleName, sourceSize)) {
            lgptLoadPersistentChopState(0, sampleIndex, sampleName, sourceSize, &slot);
        }
    }
    if (slot < 0) return false;
    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    if (!saved.used || lgptSavedStateIsFullRange(saved)) return false;
    return (saved.boundaryCount - 1) > 0;
}

void LGPTChopperOnSamplePoolDelete(int deletedIndex) {
    if (deletedIndex < 0) return;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (!s.used) continue;
        if (s.sampleIndex == deletedIndex) {
            s.used = false;
            s.sampleIndex = -1;
        } else if (s.sampleIndex > deletedIndex) {
            s.sampleIndex--;
        }
    }
}

bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex,
                                           int chopIndex,
                                           int *startFrame,
                                           int *endFrameExclusive) {
    int slot = lgptFindSavedStateBySampleIndex(sampleIndex);
    if (slot < 0) {
        std::string sampleName;
        int sourceSize = 0;
        if (lgptGetSampleIdentityBySampleIndex(sampleIndex, sampleName, sourceSize)) {
            lgptLoadPersistentChopState(0, sampleIndex, sampleName, sourceSize, &slot);
        }
    }
    if (slot < 0) return false;
    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    if (lgptSavedStateIsFullRange(saved)) return false;
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) return false;
    if (chopCount > 100) chopCount = 100;
    if (chopIndex < 0 || chopIndex >= chopCount) return false;
    int start = saved.boundaries[chopIndex];
    int endExclusive = saved.boundaries[chopIndex + 1] + 1;
    if (start < 0) start = 0;
    if (endExclusive <= start) endExclusive = start + 1;
    if (saved.sourceSize > 0 && endExclusive > saved.sourceSize) endExclusive = saved.sourceSize;
    if (startFrame) *startFrame = start;
    if (endFrameExclusive) *endFrameExclusive = endExclusive;
    return true;
}

bool LGPTChopperIsChopNoteForInstrument(ViewData *viewData,
                                          int sourceInstrumentIndex,
                                          int noteValue,
                                          int *displayChopNumber) {
    if (noteValue < 0 || noteValue >= 100) return false;
    int count = LGPTChopperGetSavedChopCountForInstrument(viewData, sourceInstrumentIndex);
    if (count <= 0 || noteValue >= count) return false;
    if (displayChopNumber) *displayChopNumber = noteValue + 1;
    return true;
}


#if defined(PLATFORM_TREEFROG)
static const int TF_W = 320;
static const int TF_H = 240;
static const int TF_WAVE_X = 16;
static const int TF_WAVE_Y = 72;
static const int TF_WAVE_W = 288;
static const int TF_WAVE_H = 88;
static const int TF_MAX_COLUMNS = 288;
static const int TF_MAX_CHOP_MARKERS = 100;
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
static int g_chopperOperationActive = 0;
static int g_chopperOperationPercent = 0;

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

    const unsigned short bg       = tf_rgb565(10, 10, 24);
    const unsigned short border   = tf_rgb565(63, 95, 191);
    const unsigned short axis     = tf_rgb565(70, 74, 92);
    const unsigned short waveDim  = tf_rgb565(40, 55, 100);
    const unsigned short waveMid  = tf_rgb565(110, 140, 205);
    const unsigned short waveHot  = tf_rgb565(235, 244, 255);
    const unsigned short cursor   = tf_rgb565(74, 216, 255);
    const unsigned short marker   = tf_rgb565(168, 107, 255);
    const unsigned short chop     = tf_rgb565(80, 230, 170);
    const unsigned short selected = tf_rgb565(140, 190, 255);
    const unsigned short selectedBg = tf_rgb565(28, 40, 86);
    const unsigned short trimSelected = tf_rgb565(210, 150, 255);

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

    /* U2.22: operation feedback is text-only; framebuffer waveform overlay is disabled while operationActive_. */
    if (0 && g_chopperOperationActive) { }
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
      operationActive_(false),
      operationPercent_(0),
      chopsInitialized_(false),
      trimMode_(false),
      pitchMode_(false),
      suspended_(false),

      sampleName_(sampleName ? sampleName : ""),
      splitParts_(4) {
    statusMessage_[0] = 0;
    operationMessage_[0] = 0;
    if (hasAssignedSample()) {
        samplePath_ = "samples:";
        samplePath_ += sampleName_;
    }
    for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
    for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) chopModel_.boundaries[b] = 0;
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

// TREEFROG_U2_25_PITCH_ENV_SCOPE_UNDO_PROGRESS
// U2.51.0: explicit L1+X undo and R1+X redo, accepted even on the 100% completion overlay.
// TREEFROG_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX
// U2.26: forces libretro video refresh during progress, rewrites Pitch/Env preview with WavFile, adds Sample selector, and compacts Pitch/Env UI.
// TREEFROG_U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX
// U2.27: adds L2+B preview stop in Pitch/Env, R2+LEFT/RIGHT chop target selection, compact UI, and instrument preview refresh after destructive edits.
// U2.28: centers Pitch/Env panel, clears TreeFrog waveform residue behind it, moves help to bottom, and forces Instrument preview refresh.
// TREEFROG_U2_28_CHOPPER_PANEL_INSTRUMENT_PREVIEW_FIX
// U2.29: reflows Pitch/Env below sample info, hard-disables waveform overlay before drawing, removes Scope inline help.
// TREEFROG_U2_29_CHOPPER_PANEL2_INSTRUMENT_PREVIEW_FIX
// U2.30: centers operation completion overlay and fixes Instrument-screen sample preview after Chopper edits.
// TREEFROG_U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX
// TREEFROG_U2_31_INSTRUMENT_LISTEN_RESTORE_STABLE
// U2.32: restores stable Listen/Import semantics: Instrument B does not preview; A on Listen previews; L2+B stops.
// TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE
bool SampleChopperModal::hasPitchEnvelopeChange() const {
    return pitchEnvTool_.HasChange();
}

void SampleChopperModal::resetPitchEnvelopeSettings() {
    pitchEnvTool_.Reset();
}

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
    pitchMode_ = false;
    resetPitchEnvelopeSettings();
    chopModel_.selected = 0;
    chopModel_.boundaryCount = 0;
    for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) chopModel_.boundaries[b] = 0;
}

bool SampleChopperModal::restoreChopStateForCurrentSample() {
    if (!hasAssignedSample() || sourceSize_ <= 1) {
        resetChopState();
        return false;
    }

    const void *projectKey = (viewData_ && viewData_->project_) ? (const void *)viewData_->project_ : 0;
    int slot = -1;
    if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex_, sampleName_, sourceSize_, &slot) || slot < 0) {
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
        if (value < 0 || value >= sourceSize_ || value <= previous) {
            resetChopState();
            return false;
        }
        chopModel_.boundaries[i] = value;
        previous = value;
    }
    for (int i = count; i < MAX_CHOP_BOUNDARIES; i++) chopModel_.boundaries[i] = 0;

    chopModel_.boundaryCount = count;
    chopModel_.selected = clampInt(saved.selectedChop, 0, chopModel_.boundaryCount - 2);
    cursorFrame_ = chopModel_.boundaries[chopModel_.selected];
    viewStartFrame_ = 0;
    trimMode_ = false;
    chopsInitialized_ = true;
    return true;
}

void SampleChopperModal::saveChopStateForCurrentSample() {
    if (!hasAssignedSample() || sourceSize_ <= 1 || !chopsInitialized_ || chopModel_.boundaryCount < 2) return;

    const void *projectKey = (viewData_ && viewData_->project_) ? (const void *)viewData_->project_ : 0;
    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex_, sampleName_, sourceSize_);
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
    saved.boundaryCount = clampInt(chopModel_.boundaryCount, 2, MAX_CHOP_BOUNDARIES);
    saved.selectedChop = clampInt(chopModel_.selected, 0, saved.boundaryCount - 2);
    for (int i = 0; i < saved.boundaryCount; i++) saved.boundaries[i] = chopModel_.boundaries[i];
    for (int i = saved.boundaryCount; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) saved.boundaries[i] = 0;
    if (wasNewSlotForSample) { for (int i = 0; i < 100; i++) saved.chopInstrument[i] = -1; }
    lgptWritePersistentChopState(saved);
    lgptReconfigureMappedChopInstruments(viewData_, instrumentIndex_, saved);
    lgptNormalizePhraseRowsForSavedChops(viewData_, instrumentIndex_, saved);
}

void SampleChopperModal::loadSampleByIndex(int index, const char *reason) {
    SamplePool *pool = SamplePool::GetInstance();
    int count = pool ? pool->GetNameListSize() : 0;
    saveChopStateForCurrentSample();
    stopSamplePreview();
    clearLogicalHistory();

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
        pitchMode_ = false;
        resetPitchEnvelopeSettings();
        chopModel_.selected = 0;
        chopModel_.boundaryCount = 0;
        sampleName_.clear();
        samplePath_.clear();
        for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
        for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) chopModel_.boundaries[b] = 0;
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
    pitchMode_ = false;
    resetPitchEnvelopeSettings();
    chopModel_.selected = 0;
    chopModel_.boundaryCount = 0;
    for (int i = 0; i < MAX_COLUMNS; i++) { minColumn_[i] = 0; maxColumn_[i] = 0; }
    for (int b = 0; b < MAX_CHOP_BOUNDARIES; b++) chopModel_.boundaries[b] = 0;

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
    // F3-1: delegado a ChopModel (bubble golden identico).
    chopModel_.Sort();
}

int SampleChopperModal::findBoundaryIndex(int frame) const {
    // F3-1: delegado a ChopModel (lineal golden).
    return chopModel_.Find(frame);
}

void SampleChopperModal::initializeChopsIfNeeded() {
    if (chopsInitialized_ || sourceSize_ <= 1) return;
    // F3-1: estado en ChopModel (algoritmo golden identico).
    chopModel_.InitRange(sourceSize_);
    chopsInitialized_ = true;
}

void SampleChopperModal::captureLogicalState(
    LogicalHistoryState &state,
    const char *action) const {
    state.sampleIndex = sampleIndex_;
    state.sourceSize = sourceSize_;
    state.selectedChop = chopModel_.selected;
    state.boundaryCount = chopModel_.boundaryCount;
    state.cursorFrame = cursorFrame_;
    state.viewStartFrame = viewStartFrame_;
    state.zoomPercent = zoomPercent_;
    state.trimMode = trimMode_;
    state.pitchMode = pitchMode_;
    state.pitchSemitones = pitchEnvTool_.Params().semitones;
    state.pitchEditParam = pitchEnvTool_.EditParam();
    state.pitchAttackMs = pitchEnvTool_.Params().attackMs;
    state.pitchSustainPercent = pitchEnvTool_.Params().sustainPercent;
    state.pitchReleaseMs = pitchEnvTool_.Params().releaseMs;
    state.pitchScope = pitchEnvTool_.Params().scope;

    snprintf(
        state.samplePath,
        sizeof(state.samplePath),
        "%s",
        samplePath_.c_str());
    state.samplePath[sizeof(state.samplePath) - 1] = 0;

    snprintf(
        state.action,
        sizeof(state.action),
        "%s",
        action ? action : "Edit");
    state.action[sizeof(state.action) - 1] = 0;

    for (int i = 0; i < MAX_CHOP_BOUNDARIES; ++i)
        state.boundaries[i] =
            i < chopModel_.boundaryCount ? chopModel_.boundaries[i] : 0;
}

void SampleChopperModal::restoreLogicalState(
    const LogicalHistoryState &state) {
    sourceSize_ = state.sourceSize;
    sampleSize_ = state.sourceSize;
    chopModel_.boundaryCount =
        clampInt(
            state.boundaryCount,
            2,
            MAX_CHOP_BOUNDARIES);

    for (int i = 0; i < MAX_CHOP_BOUNDARIES; ++i)
        chopModel_.boundaries[i] = 0;

    for (int i = 0; i < chopModel_.boundaryCount; ++i)
        chopModel_.boundaries[i] =
            clampInt(
                state.boundaries[i],
                0,
                sourceSize_ > 0 ? sourceSize_ - 1 : 0);

    if (sourceSize_ > 1) {
        chopModel_.boundaries[0] = 0;
        chopModel_.boundaries[chopModel_.boundaryCount - 1] =
            sourceSize_ - 1;
    }

    for (int i = 1; i < chopModel_.boundaryCount; ++i) {
        if (chopModel_.boundaries[i] <= chopModel_.boundaries[i - 1])
            chopModel_.boundaries[i] =
                chopModel_.boundaries[i - 1] + 1;
        if (sourceSize_ > 0 &&
            chopModel_.boundaries[i] >= sourceSize_)
            chopModel_.boundaries[i] =
                sourceSize_ - 1;
    }

    chopModel_.selected =
        clampInt(
            state.selectedChop,
            0,
            chopModel_.boundaryCount - 2);
    cursorFrame_ =
        clampInt(
            state.cursorFrame,
            0,
            sourceSize_ > 0 ? sourceSize_ - 1 : 0);
    viewStartFrame_ =
        state.viewStartFrame < 0
            ? 0
            : state.viewStartFrame;
    zoomPercent_ =
        clampInt(
            state.zoomPercent,
            MIN_ZOOM_PERCENT,
            MAX_ZOOM_PERCENT);
    trimMode_ = state.trimMode;
    pitchMode_ = state.pitchMode;
    pitchEnvTool_.SetSemitones(state.pitchSemitones);
    pitchEnvTool_.SetEditParam(state.pitchEditParam);
    pitchEnvTool_.SetAttackMs(state.pitchAttackMs);
    pitchEnvTool_.SetSustainPercent(state.pitchSustainPercent);
    pitchEnvTool_.SetReleaseMs(state.pitchReleaseMs);
    pitchEnvTool_.SetScope(state.pitchScope ? 1 : 0);
    chopsInitialized_ = true;

    saveChopStateForCurrentSample();
    clampViewStart();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    isDirty_ = true;
}

void SampleChopperModal::clearLogicalRedo() {
    /* F3-2: el golden solo vaciaba el redo; ClearRedo() de la capa
       preserva exactamente esa semantica (Push ya invalida el redo). */
    editHistory_.ClearRedo();
}

void SampleChopperModal::clearLogicalHistory() {
    editHistory_.Clear();
}

void SampleChopperModal::pushLogicalUndo(
    const char *action) {
    /*
     * F3-2: la pila la gestiona SampleEditHistory (mismo shift izquierda
     * si esta llena y misma invalidacion del redo).  El gancho de
     * destructivos sigue en la vista: una nueva rama tras un Undo
     * fisico invalida el Redo destructivo.
     */
    if (g_lgptLastDestructiveEditUndone)
        lgptClearDestructiveEditHistory();

    LogicalHistoryState state;
    captureLogicalState(state, action);
    editHistory_.Push(state);
}

bool SampleChopperModal::undoLogicalEdit() {
    if (editHistory_.UndoCount() <= 0)
        return false;

    LogicalHistoryState state;
    editHistory_.PeekUndo(state);

    if (state.sampleIndex != sampleIndex_ ||
        strcmp(
            state.samplePath,
            samplePath_.c_str()) != 0 ||
        state.sourceSize != sourceSize_) {
        clearLogicalHistory();
        setStatus("Undo history does not match sample");
        return false;
    }

    LogicalHistoryState redoState;
    captureLogicalState(redoState, state.action);
    if (!editHistory_.Undo(redoState))
        return false;

    restoreLogicalState(state);

    char message[64];
    snprintf(
        message,
        sizeof(message),
        "Undo: %.46s",
        state.action);
    setStatus(message);
    return true;
}

bool SampleChopperModal::redoLogicalEdit() {
    if (editHistory_.RedoCount() <= 0)
        return false;

    LogicalHistoryState state;
    editHistory_.PeekRedo(state);

    if (state.sampleIndex != sampleIndex_ ||
        strcmp(
            state.samplePath,
            samplePath_.c_str()) != 0 ||
        state.sourceSize != sourceSize_) {
        clearLogicalHistory();
        setStatus("Redo history does not match sample");
        return false;
    }

    LogicalHistoryState undoState;
    captureLogicalState(undoState, state.action);
    if (!editHistory_.Redo(undoState))
        return false;

    restoreLogicalState(state);

    char message[64];
    snprintf(
        message,
        sizeof(message),
        "Redo: %.46s",
        state.action);
    setStatus(message);
    return true;
}

bool SampleChopperModal::undoLastChopperEdit() {
    if (undoLogicalEdit())
        return true;
    return restoreLastDestructiveEdit(false);
}

bool SampleChopperModal::redoLastChopperEdit() {
    if (redoLogicalEdit())
        return true;
    return restoreLastDestructiveEdit(true);
}

void SampleChopperModal::addChopAtCursor() {
    if (sourceSize_ <= 1) { setStatus("No sample to chop"); return; }
    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount >= MAX_CHOP_BOUNDARIES) { setStatus("Max 100 chops reached"); return; }
    int frame = getCursorFrame();
    bool liveCut = false;
    if (previewActive_ && Player::GetInstance()->IsStreaming()) {
        int liveFrame = Player::GetInstance()->GetStreamingPosition();
        if (liveFrame >= previewStartFrame_ && liveFrame <= previewEndFrame_) {
            frame = clampInt(liveFrame, 0, sourceSize_ - 1);
            cursorFrame_ = frame;
            liveCut = true;
        }
    }
    int minEdge = (chopModel_.boundaryCount >= 2) ? chopModel_.boundaries[0] : 0;
    int maxEdge = (chopModel_.boundaryCount >= 2) ? chopModel_.boundaries[chopModel_.boundaryCount - 1] : (sourceSize_ - 1);
    if (frame <= minEdge || frame >= maxEdge) { setStatus("Cannot chop at edge"); return; }
    for (int i = 0; i < chopModel_.boundaryCount; i++) {
        if (abs(chopModel_.boundaries[i] - frame) <= 1) { setStatus("Chop already exists"); return; }
    }
    pushLogicalUndo("Add cut");
    // F3-1: append + sort golden en ChopModel.
    chopModel_.Append(frame);
    sortBoundaries();
    int idx = findBoundaryIndex(frame);
    if (idx > 0) chopModel_.selected = idx - 1;
    chopModel_.ClampSelectedToChops();
    saveChopStateForCurrentSample();
    publishOverlayState();
    char msg[64]; snprintf(msg, sizeof(msg), liveCut ? "Live chop %02d at %d" : "Chop %02d at %d", chopModel_.selected, frame); setStatus(msg);
}

void SampleChopperModal::deleteSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { setStatus("No chop to delete"); return; }

    /* Chops are stored as boundaries. Deleting a chop removes one internal boundary
       and merges the selected region with a neighbor. Edge boundaries 0/end are never removed. */
    int removeIdx = (chopModel_.selected > 0) ? chopModel_.selected : 1;
    if (removeIdx <= 0 || removeIdx >= chopModel_.boundaryCount - 1) { setStatus("Cannot delete edge"); return; }

    pushLogicalUndo("Merge cuts");
    // F3-1: shift-remove golden + reinit minimo en ChopModel.
    chopModel_.RemoveChop(removeIdx, sourceSize_);
    chopModel_.ClampSelectedToChops();
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
    int maxChop = chopModel_.boundaryCount - 2;
    chopModel_.selected = clampInt(chopModel_.selected + delta, 0, maxChop);
    cursorFrame_ = chopModel_.boundaries[chopModel_.selected];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    char msg[64]; snprintf(msg, sizeof(msg), "Selected chop %02d", chopModel_.selected); setStatus(msg);
}

bool SampleChopperModal::hasUserChops() const {
    return (chopModel_.boundaryCount > 2);
}

bool SampleChopperModal::hasActiveSliceRange() const {
    if (chopModel_.boundaryCount < 2 || sourceSize_ <= 1) return false;
    if (chopModel_.boundaryCount > 2) return true;
    return (chopModel_.boundaries[0] > 0 || chopModel_.boundaries[1] < sourceSize_ - 1);
}

int SampleChopperModal::selectedChopStartFrame() const {
    // F3-1: delegado a ChopModel (clamps golden identicos).
    return chopModel_.StartFrameForSelected();
}

int SampleChopperModal::selectedChopEndFrame() const {
    // F3-1: delegado a ChopModel (clamps golden identicos).
    return chopModel_.EndFrameForSelected(sourceSize_);
}

int SampleChopperModal::getFrameStepForEdit() const {
    int viewFrames = getViewFrameCount();
    int step = viewFrames / WAVE_W;
    if (step < 1) step = 1;
    return step;
}

void SampleChopperModal::toggleTrimMode() {
    if (pitchMode_) pitchMode_ = false;
    initializeChopsIfNeeded();
    trimMode_ = !trimMode_;
    if (trimMode_) {
        cursorFrame_ = selectedChopStartFrame();
        setStatus("CROP SAMPLE");
    } else {
        setStatus("Crop mode off");
    }
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    isDirty_ = true;
}

void SampleChopperModal::nudgeSelectedStart(int deltaFrames) {
    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount < 2) { setStatus("No range to trim"); return; }
    int idx = chopModel_.selected;
    int minFrame = (idx == 0) ? 0 : chopModel_.boundaries[idx - 1] + 1;
    int maxFrame = chopModel_.boundaries[idx + 1] - 1;
    int nextFrame =
        clampInt(
            chopModel_.boundaries[idx] + deltaFrames,
            minFrame,
            maxFrame);
    if (nextFrame == chopModel_.boundaries[idx]) return;
    pushLogicalUndo("Move cut start");
    chopModel_.boundaries[idx] = nextFrame;
    cursorFrame_ = chopModel_.boundaries[idx];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Adjusted chop start");
}

void SampleChopperModal::nudgeSelectedEnd(int deltaFrames) {
    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount < 2) { setStatus("No range to trim"); return; }
    int idx = chopModel_.selected + 1;
    int minFrame = chopModel_.boundaries[idx - 1] + 1;
    int maxFrame = (idx == chopModel_.boundaryCount - 1) ? (sourceSize_ - 1) : (chopModel_.boundaries[idx + 1] - 1);
    int nextFrame =
        clampInt(
            chopModel_.boundaries[idx] + deltaFrames,
            minFrame,
            maxFrame);
    if (nextFrame == chopModel_.boundaries[idx]) return;
    pushLogicalUndo("Move cut end");
    chopModel_.boundaries[idx] = nextFrame;
    cursorFrame_ = chopModel_.boundaries[idx];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Adjusted chop end");
}


void SampleChopperModal::cropToSelectedRange() {
    if (sourceSize_ <= 1) { setStatus("No sample to crop"); return; }
    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount < 2) { setStatus("No range to crop"); return; }

    chopModel_.selected = clampInt(chopModel_.selected, 0, chopModel_.boundaryCount - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    if (end <= start) { setStatus("Bad crop range"); return; }

    /* U2.14: safe logical crop. We keep the chosen/trimmed range as a single S01 slice
       and ignore material outside it at playback time. We do not rewrite the WAV file here. */
    pushLogicalUndo("Keep logical range");
    chopModel_.boundaryCount = 2;
    chopModel_.boundaries[0] = start;
    chopModel_.boundaries[1] = end;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) chopModel_.boundaries[i] = 0;
    chopModel_.selected = 0;
    trimMode_ = false;
    cursorFrame_ = start;
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    char msg[64];
    snprintf(msg, sizeof(msg), "Keep range %d-%d", start, end);
    setStatus(msg);
}

void SampleChopperModal::splitSampleIntoEqualParts(int parts) {
    initializeChopsIfNeeded();
    if (sourceSize_ <= 1) { setStatus("No sample to split"); return; }
    setOperationCombo("L1 + B");
    if (parts < 2 || parts > 32) parts = 4;
    int step = sourceSize_ / parts;
    if (step < 1) { setStatus("Sample too small"); return; }
    pushLogicalUndo("Split sample");
    // F3-1: rebuild de boundaryes golden en ChopModel (el guard de
    // step<1 y el status se evaluaron arriba, igual que el golden).
    chopModel_.SplitIntoEqualParts(parts, sourceSize_);
    trimMode_ = false;
    cursorFrame_ = 0;
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    char m[64];
    snprintf(m, sizeof(m), "Split sample in %d parts", parts);
    setStatus(m);
}

void SampleChopperModal::setOperationCombo(const char *combo) {
    if (!combo) combo = "";
    snprintf(operationComboLabel_, sizeof(operationComboLabel_), "%s", combo);
    operationComboLabel_[sizeof(operationComboLabel_) - 1] = 0;
}

/* TREEFROG_U2_39_CHOPPER_SPLIT_ZERO (Bacon 1.1.1): L1+B at 32 parts clears
   every cut (whole sample shows as a single region, no visible cut lines)
   and the next L1+B starts the cycle again at 4. */
void SampleChopperModal::clearAllChops() {
    initializeChopsIfNeeded();
    if (sourceSize_ <= 1) { setStatus("No sample to clear"); return; }
    pushLogicalUndo("Clear chops");
    // F3-1: estado en ChopModel (rango minimo + cero del resto).
    chopModel_.ClearAll(sourceSize_);
    trimMode_ = false;
    cursorFrame_ = 0;
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("No cuts (L1+B to split again)");
}

void SampleChopperModal::cycleSplitParts() {
    static const int kSplitCycle[] = {4, 8, 16, 32, 0, 4};
    int next = 1;
    for (int i = 0; i < 4; i++) {
        if (splitParts_ == kSplitCycle[i]) { next = i + 1; break; }
    }
    splitParts_ = kSplitCycle[next];
    if (splitParts_ == 0) {
        clearAllChops();
        return;
    }
    splitSampleIntoEqualParts(splitParts_);
}

void SampleChopperModal::snapSelectedBoundaryToZeroCross(bool isStart) {
    if (!hasAssignedSample() || sourceSize_ <= 1) { setStatus("No sample loaded"); return; }
    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount < 2) { setStatus("No chops to snap"); return; }
    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) { setStatus("No WAV source"); return; }
    short *samples = (short *)source->GetSampleBuffer(-1);
    int channels = source->GetChannelCount(-1);
    if (!samples || channels <= 0) { setStatus("Bad sample buffer"); return; }

    int idx = chopModel_.selected;
    if (!isStart) idx = clampInt(idx + 1, 1, chopModel_.boundaryCount - 1);
    if (idx < 0 || idx >= chopModel_.boundaryCount) { setStatus("Invalid boundary"); return; }
    int frame = chopModel_.boundaries[idx];
    int minFrame = (idx == 0) ? 0 : chopModel_.boundaries[idx - 1] + 1;
    int maxFrame = (idx == chopModel_.boundaryCount - 1) ? (sourceSize_ - 1) : (chopModel_.boundaries[idx + 1] - 1);
    int lo = frame - 64; if (lo < minFrame) lo = minFrame;
    int hi = frame + 64; if (hi > maxFrame) hi = maxFrame;
    int best = frame;
    long bestScore = -1;
    for (int f = lo; f <= hi; f++) {
        long score = 0;
        for (int c = 0; c < channels; c++) {
            int s = samples[f * channels + c];
            score += (s < 0) ? -s : s;
        }
        if (bestScore < 0 || score < bestScore) { bestScore = score; best = f; }
    }
    if (best != frame) {
        pushLogicalUndo(isStart ? "Snap start" : "Snap end");
        chopModel_.boundaries[idx] = best;
        cursorFrame_ = best;
        sortBoundaries();
        saveChopStateForCurrentSample();
        ensureCursorVisible();
        prepareWaveformPreview();
        publishOverlayState();
        char m[64];
        snprintf(m, sizeof(m), "Zero-cross %s %d", isStart ? "start" : "end", best);
        setStatus(m);
    } else {
        setStatus("Already at zero-cross");
    }
}

bool SampleChopperModal::destructiveCropToSelectedRange() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to crop"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Crop WAV only"); return false; }

    // TREEFROG_U2_38_CHOPPER_CROP_CRASH_FIX (Bacon 1.1.1): editing the shared
    // SoundSource buffer (ReplaceBuffer) while any voice/stream still references
    // it or while the file is open in the AudioFileStreamer is a use-after-free.
    // Halt *all* playback (song voices + streaming preview) before touching the
    // pool buffer or rewriting the WAV on disk; the waveform preview stays.
    stopSamplePreview();
    Player *p=Player::GetInstance();
    if (p) {
        if (p->IsRunning()) p->Stop();
        if (p->IsStreaming()) p->StopStreaming();
    }

    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount < 2) { setStatus("No range to crop"); return false; }

    chopModel_.selected = clampInt(chopModel_.selected, 0, chopModel_.boundaryCount - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    if (!source || !wav) { setStatus("No WAV source"); return false; }

    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    start = clampInt(start, 0, size - 1);
    end = clampInt(end, start, size - 1);
    int frameCount = end - start + 1;
    if (frameCount <= 1) { setStatus("Empty crop"); return false; }
    if (start == 0 && end == size - 1) { setStatus("Crop unchanged"); return false; }

    stopSamplePreview();
    setOperationCombo("R1 + A");
    showOperationProgress("Operacion Crop", 5);

    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Operacion Crop", 25);

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) { clearOperationProgress(); setStatus("No crop memory"); return false; }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));
    showOperationProgress("Operacion Crop", 50);

    if (!wav->ReplaceBuffer(cropped, frameCount, channels, rate)) {
        free(cropped); clearOperationProgress(); setStatus("Cannot crop buffer"); return false;
    }
    free(cropped);
    showOperationProgress("Operacion Crop", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview(); publishOverlayState();
        clearOperationProgress(); setStatus("Cannot write crop"); return false;
    }
    showOperationProgress("Operacion Crop", 85);
    sync();

    sourceSize_ = frameCount;
    sampleSize_ = frameCount;
    viewStartFrame_ = 0;
    cursorFrame_ = 0;
    chopModel_.boundaryCount = 2;
    chopModel_.boundaries[0] = 0;
    chopModel_.boundaries[1] = frameCount - 1;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) chopModel_.boundaries[i] = 0;
    chopModel_.selected = 0;
    trimMode_ = true;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress("Crop complete", 100);
    return true;
}

bool SampleChopperModal::destructiveDeleteSelectedRange() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to edit"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Delete WAV only"); return false; }

    // TREEFROG_U2_38_CHOPPER_CROP_CRASH_FIX: same as for crop — halt all
    // playback before freeing/replacing the shared SoundSource buffer.
    stopSamplePreview();
    Player *p=Player::GetInstance();
    if (p) {
        if (p->IsRunning()) p->Stop();
        if (p->IsStreaming()) p->StopStreaming();
    }

    initializeChopsIfNeeded();
    if (chopModel_.boundaryCount < 2) { setStatus("No range to delete"); return false; }

    chopModel_.selected = clampInt(chopModel_.selected, 0, chopModel_.boundaryCount - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    if (!source || !wav) { setStatus("No WAV source"); return false; }

    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    start = clampInt(start, 0, size - 1);
    end = clampInt(end, start, size - 1);
    int removed = end - start + 1;
    int nextSize = size - removed;
    if (nextSize <= 1) { setStatus("Cannot delete all"); return false; }

    stopSamplePreview();
    setOperationCombo("L2 + Y");
    showOperationProgress("Operacion Delete", 5);

    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Delete", chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Operacion Delete", 25);

    int sampleWords = nextSize * channels;
    short *edited = (short *)malloc(sampleWords * sizeof(short));
    if (!edited) { clearOperationProgress(); setStatus("No edit memory"); return false; }

    int outFrame = 0;
    if (start > 0) {
        memcpy(edited, samples, start * channels * sizeof(short));
        outFrame = start;
    }
    int tailFrames = size - end - 1;
    if (tailFrames > 0) {
        memcpy(edited + (outFrame * channels), samples + ((end + 1) * channels), tailFrames * channels * sizeof(short));
    }
    showOperationProgress("Operacion Delete", 50);

    if (!wav->ReplaceBuffer(edited, nextSize, channels, rate)) {
        free(edited); clearOperationProgress(); setStatus("Cannot edit buffer"); return false;
    }
    free(edited);
    showOperationProgress("Operacion Delete", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview(); publishOverlayState();
        clearOperationProgress(); setStatus("Cannot write edit"); return false;
    }
    showOperationProgress("Operacion Delete", 85);
    sync();

    int oldBoundaries[MAX_CHOP_BOUNDARIES];
    int oldCount = chopModel_.boundaryCount;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) oldBoundaries[i] = chopModel_.boundaries[i];
    int nextBoundaries[MAX_CHOP_BOUNDARIES];
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) nextBoundaries[i] = 0;
    int out = 0;
    nextBoundaries[out++] = 0;
    for (int i = 1; i < oldCount - 1 && out < MAX_CHOP_BOUNDARIES - 1; i++) {
        int v = oldBoundaries[i];
        if (v <= start) {
            if (v > nextBoundaries[out - 1]) nextBoundaries[out++] = v;
        } else if (v > end) {
            int shifted = v - removed;
            if (shifted > nextBoundaries[out - 1] && shifted < nextSize - 1) nextBoundaries[out++] = shifted;
        }
    }
    if (nextBoundaries[out - 1] != nextSize - 1 && out < MAX_CHOP_BOUNDARIES) nextBoundaries[out++] = nextSize - 1;
    if (out < 2) { out = 2; nextBoundaries[0] = 0; nextBoundaries[1] = nextSize - 1; }

    sourceSize_ = nextSize;
    sampleSize_ = nextSize;
    chopModel_.boundaryCount = out;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) chopModel_.boundaries[i] = (i < chopModel_.boundaryCount) ? nextBoundaries[i] : 0;
    chopModel_.selected = clampInt(chopModel_.selected, 0, chopModel_.boundaryCount - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = chopModel_.boundaries[chopModel_.selected];
    trimMode_ = true;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress("Operacion Delete", 100);
    return true;
}

bool SampleChopperModal::restoreLastDestructiveEdit(bool redo) {
    if (!g_lgptPhysicalUndoSamples || !g_lgptPhysicalRedoSamples) {
        setStatus(redo ? "Nothing to redo" : "No edit to undo");
        return false;
    }
    if (g_lgptLastDestructiveEditSampleIndex != sampleIndex_ ||
        g_lgptLastDestructiveEditSamplePath != samplePath_) {
        setStatus("Undo/redo: wrong sample");
        return false;
    }
    if (redo && !g_lgptLastDestructiveEditUndone) {
        setStatus("Nothing to redo");
        return false;
    }
    if (!redo && g_lgptLastDestructiveEditUndone) {
        setStatus("Already undone; R1+X redo");
        return false;
    }

    short *restoreSamples = redo ? g_lgptPhysicalRedoSamples : g_lgptPhysicalUndoSamples;
    int restoreFrames = redo ? g_lgptPhysicalRedoFrames : g_lgptPhysicalUndoFrames;
    int restoreChannels = redo ? g_lgptPhysicalRedoChannels : g_lgptPhysicalUndoChannels;
    int restoreRate = redo ? g_lgptPhysicalRedoRate : g_lgptPhysicalUndoRate;
    int restoreCount = redo ? g_lgptLastDestructiveEditRedoBoundaryCount : g_lgptLastDestructiveEditUndoBoundaryCount;
    int restoreSelected = redo ? g_lgptLastDestructiveEditRedoSelected : g_lgptLastDestructiveEditUndoSelected;
    int *restoreBoundaries = redo ? g_lgptLastDestructiveEditRedoBoundaries : g_lgptLastDestructiveEditUndoBoundaries;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    if (!source || !wav) { setStatus("Undo/redo source fail"); return false; }

    stopSamplePreview();
    lgptStopAllAudioBeforeDestructiveEdit();
    clearLogicalHistory();
    setOperationCombo(redo ? "R1 + X" : "L1 + X");
    showOperationProgress(redo ? "Operacion Redo" : "Operacion Undo", 10);
    if (!lgptRestorePhysicalSnapshotToWav(wav, samplePath_.c_str(), restoreSamples,
                                          restoreFrames, restoreChannels, restoreRate)) {
        clearOperationProgress();
        setStatus(redo ? "Redo restore fail" : "Undo restore fail");
        return false;
    }
    showOperationProgress(redo ? "Operacion Redo" : "Operacion Undo", 70);

    sourceSize_ = restoreFrames;
    sampleSize_ = restoreFrames;
    chopModel_.boundaryCount = clampInt(restoreCount, 2, MAX_CHOP_BOUNDARIES);
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) chopModel_.boundaries[i] = 0;
    for (int i = 0; i < chopModel_.boundaryCount; i++)
        chopModel_.boundaries[i] = clampInt(restoreBoundaries[i], 0, restoreFrames - 1);
    chopModel_.boundaries[0] = 0;
    chopModel_.boundaries[chopModel_.boundaryCount - 1] = restoreFrames - 1;
    for (int i = 1; i < chopModel_.boundaryCount; i++) {
        if (chopModel_.boundaries[i] <= chopModel_.boundaries[i - 1])
            chopModel_.boundaries[i] = chopModel_.boundaries[i - 1] + 1;
        if (chopModel_.boundaries[i] >= restoreFrames)
            chopModel_.boundaries[i] = restoreFrames - 1;
    }

    chopModel_.selected = clampInt(restoreSelected, 0, chopModel_.boundaryCount - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = chopModel_.boundaries[chopModel_.selected];
    trimMode_ = true;
    chopsInitialized_ = true;
    g_lgptLastDestructiveEditUndone = !redo;

    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress(redo ? "Operacion Redo" : "Operacion Undo", 100);
    return true;
}


void SampleChopperModal::togglePitchMode() {
    stopSamplePreview();
    if (!hasAssignedSample()) { setStatus("No sample for pitch"); return; }
    if (!hasWaveform_) { setStatus("No waveform loaded"); return; }
    if (sourceSize_ <= 1 || sourceChannels_ <= 0 || sourceRate_ <= 0) { setStatus("Sample not ready for pitch"); return; }
    pitchMode_ = !pitchMode_;
    if (pitchMode_) {
        trimMode_ = false;
        resetPitchEnvelopeSettings();
        setStatus("Pitch/Env sample");
    } else {
        setStatus("Pitch off");
    }
    publishOverlayState();
    isDirty_ = true;
}

void SampleChopperModal::selectPitchEditParam(int delta) {
    pitchEnvTool_.NudgeEditParam(delta);
    const char *name =
        PitchEnvelopeTool::ParamName(pitchEnvTool_.EditParam());
    char msg[64]; snprintf(msg, sizeof(msg), "%s selected", name);
    setStatus(msg);
    isDirty_ = true;
}

void SampleChopperModal::nudgePitchSemitones(int delta) {
    int before = pitchEnvTool_.Params().semitones;
    int next = clampInt(before + delta, LGPT_PITCH_MIN_SEMITONES,
                        LGPT_PITCH_MAX_SEMITONES);
    if (next == before) return;
    pushLogicalUndo("Pitch setting");
    pitchEnvTool_.SetSemitones(next);
    char msg[64]; snprintf(msg, sizeof(msg), "Pitch %+d st",
                           pitchEnvTool_.Params().semitones);
    setStatus(msg);
    isDirty_ = true;
}

void SampleChopperModal::nudgePitchEnvelopeValue(int delta) {
    int param = pitchEnvTool_.EditParam();
    if (param == 0) {
        nudgePitchSemitones(delta);
        return;
    }

    if (param == 1) {
        int before = pitchEnvTool_.Params().attackMs;
        int next = clampInt(before + (delta * 5), LGPT_PITCH_MIN_ATTACK_MS,
                            LGPT_PITCH_MAX_ATTACK_MS);
        if (next == before) return;
        pushLogicalUndo("Attack setting");
        pitchEnvTool_.SetAttackMs(next);
        char msg[64]; snprintf(msg, sizeof(msg), "Attack %d ms",
                               pitchEnvTool_.Params().attackMs);
        setStatus(msg);
    } else if (param == 2) {
        int before = pitchEnvTool_.Params().sustainPercent;
        int next = clampInt(before + (delta * 5),
                            LGPT_PITCH_MIN_SUSTAIN_PERCENT,
                            LGPT_PITCH_MAX_SUSTAIN_PERCENT);
        if (next == before) return;
        pushLogicalUndo("Sustain setting");
        pitchEnvTool_.SetSustainPercent(next);
        char msg[64]; snprintf(msg, sizeof(msg), "Sustain %d%%",
                               pitchEnvTool_.Params().sustainPercent);
        setStatus(msg);
    } else if (param == 3) {
        int before = pitchEnvTool_.Params().releaseMs;
        int next = clampInt(before + (delta * 5), LGPT_PITCH_MIN_RELEASE_MS,
                            LGPT_PITCH_MAX_RELEASE_MS);
        if (next == before) return;
        pushLogicalUndo("Release setting");
        pitchEnvTool_.SetReleaseMs(next);
        char msg[64]; snprintf(msg, sizeof(msg), "Release %d ms",
                               pitchEnvTool_.Params().releaseMs);
        setStatus(msg);
    } else if (param == 4) {
        pushLogicalUndo("Pitch scope");
        pitchEnvTool_.ToggleScope();
        char msg[64]; snprintf(msg, sizeof(msg), "Scope %s",
                               pitchEnvTool_.Params().scope ? "Chop" : "Sample");
        setStatus(msg);
    } else if (param == 5) {
        selectPitchTargetSample(delta);
        return;
    }
    isDirty_ = true;
}

void SampleChopperModal::selectPitchTargetSample(int delta) {
    int oldPitch = pitchEnvTool_.Params().semitones;
    int oldParam = pitchEnvTool_.EditParam();
    int oldAttack = pitchEnvTool_.Params().attackMs;
    int oldSustain = pitchEnvTool_.Params().sustainPercent;
    int oldRelease = pitchEnvTool_.Params().releaseMs;
    int oldScope = pitchEnvTool_.Params().scope;

    loadSampleByIndex(sampleIndex_ + delta, delta > 0 ? "Next pitch sample" : "Prev pitch sample");

    pitchMode_ = true;
    trimMode_ = false;
    pitchEnvTool_.SetSemitones(oldPitch);
    pitchEnvTool_.SetEditParam(oldParam);
    pitchEnvTool_.SetAttackMs(oldAttack);
    pitchEnvTool_.SetSustainPercent(oldSustain);
    pitchEnvTool_.SetReleaseMs(oldRelease);
    pitchEnvTool_.SetScope(oldScope);

    char msg[64];
    snprintf(msg, sizeof(msg), "Pitch target sample %02X", sampleIndex_);
    setStatus(msg);
    isDirty_ = true;
}

void SampleChopperModal::refreshCurrentInstrumentAfterSampleEdit(int newSize) {
    if (!viewData_ || !viewData_->project_) return;
    if (newSize <= 1) newSize = sourceSize_;
    if (newSize <= 1) return;

    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (!bank) return;
    if (instrumentIndex_ < 0 || instrumentIndex_ >= MAX_SAMPLEINSTRUMENT_COUNT) return;

    I_Instrument *raw = bank->GetInstrument(instrumentIndex_);
    if (!raw || raw->GetType() != IT_SAMPLE) return;

    SampleInstrument *instr = (SampleInstrument *)raw;
    Variable *sampleVar = instr->FindVariable(SIP_SAMPLE);
    if (!sampleVar || sampleVar->GetInt() != sampleIndex_) return;

    /* U2.28: force SampleInstrument::Update() after an in-place WavFile edit.
       The sample index usually does not change, but notifying SIP_SAMPLE rebuilds
       the source pointer and restores Instrument-screen B preview on TreeFrog. */
    sampleVar->SetInt(sampleIndex_, true);

    Variable *vStart = instr->FindVariable(SIP_START);
    Variable *vLoopStart = instr->FindVariable(SIP_LOOPSTART);
    Variable *vEnd = instr->FindVariable(SIP_END);

    int start = vStart ? vStart->GetInt() : 0;
    int loopStart = vLoopStart ? vLoopStart->GetInt() : start;
    int end = vEnd ? vEnd->GetInt() : newSize;

    if (start < 0 || start >= newSize) start = 0;
    if (loopStart < 0 || loopStart >= newSize) loopStart = start;
    if (end <= start || end > newSize) end = newSize;

    if (vStart) vStart->SetInt(start);
    if (vLoopStart) vLoopStart->SetInt(loopStart);
    if (vEnd) vEnd->SetInt(end);
}

void SampleChopperModal::drawPitchScreen(GUITextProperties &props) {
    if (!pitchMode_) return;
    char buffer[40];

#if defined(PLATFORM_TREEFROG)
    /* U2.29: pitch mode must own the whole center panel area. Disable the
       waveform overlay before the next video refresh and clear the entire
       waveform band, not only the former narrow panel. This removes the right
       side bars visible in U2.28 and avoids drawing over the Frame line. */
    g_chopperOverlayActive = 0;
    /* TREEFROG_U2_40_PITCH_OVERLAY_CLEANUP (Bacon 1.1.1): clear the whole
       panel (60..240) so no stale chopper text remains visible behind the
       pitch/env submenu (previously rows 22+ kept ghost letters). */
    tf_rect(0, 60, 320, 180, tf_rgb565(10, 10, 24));
#endif

    // RC6: the Pitch/Env submenu follows the port-wide graphical language
    // (same as the other submenus): a centered title on the row just above a
    // centered label/value block, no ASCII box.  Each row highlights the
    // edited parameter by inverting on CD_HILITE2.
    MenuLayout ml = UiDraw::MakeCenteredMenuLayout(7, 11, 10, 2);
    UiDraw::DrawCenteredTitleAt(*this, ml.startY - 1, "PITCH/ENV");

    snprintf(buffer, sizeof(buffer), "I%02X S%02X C%02d/%02d",
             instrumentIndex_, sampleIndex_,
             chopModel_.selected + 1,
             (chopModel_.boundaryCount > 1 ? chopModel_.boundaryCount - 1 : 1));
    SetColor(CD_NORMAL);
    props.invert_ = false;
    DrawString(ml.labelX, ml.startY, buffer, props);

    static const char *labels[6] = {"Pitch", "Attack", "Sustain",
                                    "Release", "Scope", "Sample"};
    for (int i = 0; i < 6; i++) {
        bool selected = (pitchEnvTool_.EditParam() == i);
        char value[16];
        switch (i) {
        case 0: snprintf(value, sizeof(value), "%+3d st", pitchEnvTool_.Params().semitones); break;
        case 1: snprintf(value, sizeof(value), "%4d ms", pitchEnvTool_.Params().attackMs); break;
        case 2: snprintf(value, sizeof(value), "%3d %%", pitchEnvTool_.Params().sustainPercent); break;
        case 3: snprintf(value, sizeof(value), "%4d ms", pitchEnvTool_.Params().releaseMs); break;
        case 4: snprintf(value, sizeof(value), "%s", pitchEnvTool_.Params().scope ? "Chop" : "Sample"); break;
        default: snprintf(value, sizeof(value), "%02X", sampleIndex_); break;
        }
        SetColor(CD_NORMAL);
        props.invert_ = false;
        DrawString(ml.labelX, ml.startY + 1 + i, labels[i], props);
        SetColor(selected ? CD_HILITE2 : CD_HILITE1);
        props.invert_ = selected;
        DrawString(ml.valueX, ml.startY + 1 + i, value, props);
        props.invert_ = false;
        SetColor(CD_NORMAL);
    }

    SetColor(CD_NORMAL);
    /* Bacon 1.1.1 V13: compact pipe-separated hint lines matching the
       chopper main screen; full list in the CHOP PITCH help section. */
    drawStringAbs(1, 24, "UP/DN Item | L/R Value | B Preview", props);
    drawStringAbs(1, 25, "A Apply | L1+R1 Exit | R2+LR Target", props);
}

void SampleChopperModal::applyEnvelopeToBuffer(short *samples, int frames, int channels, int rate, int attackMs, int sustainPercent, int releaseMs) {
    PitchEnvelopeTool::ApplyEnvelope(samples, frames, channels, rate,
                                     attackMs, sustainPercent, releaseMs);
}

bool SampleChopperModal::buildPitchEnvelopeBufferFromRange(int startFrame, int endFrame, int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate) {
    if (outSamples) *outSamples = 0;
    if (outFrames) *outFrames = 0;
    if (outChannels) *outChannels = 0;
    if (outRate) *outRate = 0;
    if (!hasAssignedSample() || sourceSize_ <= 1) return false;
    if (semitones < -12) semitones = -12;
    if (semitones > 12) semitones = 12;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) return false;
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) return false;

    if (!PitchEnvelopeTool::BuildPitchedRange(
            samples, size, channels, startFrame, endFrame, semitones,
            pitchEnvTool_.Params().attackMs,
            pitchEnvTool_.Params().sustainPercent,
            pitchEnvTool_.Params().releaseMs, rate, outSamples, outFrames)) {
        return false;
    }
    if (outChannels) *outChannels = channels;
    if (outRate) *outRate = rate;
    return true;
}

bool SampleChopperModal::buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate) {
    return buildPitchEnvelopeBufferFromRange(0, sourceSize_ > 0 ? sourceSize_ - 1 : 0, semitones, outSamples, outFrames, outChannels, outRate);
}

bool SampleChopperModal::preparePitchEnvelopePreviewBuffer(short **outSamples, int *outFrames, int *outChannels, int *outRate) {
    if (!pitchMode_) return false;
    if (pitchEnvTool_.Params().scope) {
        initializeChopsIfNeeded();
        if (!hasActiveSliceRange()) return false;
        return buildPitchEnvelopeBufferFromRange(selectedChopStartFrame(), selectedChopEndFrame(), pitchEnvTool_.Params().semitones, outSamples, outFrames, outChannels, outRate);
    }
    return buildPitchedBuffer(pitchEnvTool_.Params().semitones, outSamples, outFrames, outChannels, outRate);
}

static void lgptWriteU2PreviewLE16(I_File *file, unsigned short value) {
    unsigned char b[2];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    file->Write(b, 1, 2);
}

static void lgptWriteU2PreviewLE32(I_File *file, unsigned int value) {
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    b[2] = (unsigned char)((value >> 16) & 0xFF);
    b[3] = (unsigned char)((value >> 24) & 0xFF);
    file->Write(b, 1, 4);
}

bool SampleChopperModal::writePreviewPitchWav(short *samples, int frames, int channels, int rate, std::string &logicalPath) {
    logicalPath = "samples:__u2_pitch_env_preview.wav";
    if (!samples || frames <= 0 || channels <= 0 || rate <= 0 || samplePath_.empty()) return false;

    /* U2.26: use the same WavFile writer as destructive edits. U2.24/U2.25
       used a small local RIFF writer; on the R36S build that file was created
       but FileStreamer preview was silent for modified buffers. Reusing
       WavFile::ReplaceBuffer + SaveBufferToPath keeps the preview WAV header
       identical to files that are known to stream correctly after Apply. */
    Path sourcePath(samplePath_.c_str());
    WavFile *wav = WavFile::Open(sourcePath.GetPath().c_str());
    if (!wav) return false;
    bool ok = wav->ReplaceBuffer(samples, frames, channels, rate);
    if (ok) ok = wav->SaveBufferToPath(logicalPath.c_str());
    delete wav;
    return ok;
}

void SampleChopperModal::previewPitchSetting() {
    if (!pitchMode_) return;
    if (!hasPitchEnvelopeChange()) {
        if (pitchEnvTool_.Params().scope) playSelectedChop();
        else playFullSample();
        setStatus(pitchEnvTool_.Params().scope ? "Preview chop unchanged" : "Preview unchanged");
        return;
    }

    showOperationProgress("Operacion Preview", 5);
    setOperationCombo("B");
#if defined(PLATFORM_TREEFROG)
    tf_rect(0, 0, 0, 0, 0);
#endif
    short *pitched = 0;
    int frames = 0, channels = 0, rate = 0;
    if (!preparePitchEnvelopePreviewBuffer(&pitched, &frames, &channels, &rate)) { clearOperationProgress(); setStatus("Pitch preview fail"); return; }
    showOperationProgress("Operacion Preview", 65);
    std::string logical;

    /* U2.52.0: stop any live stream BEFORE rewriting the shared preview WAV.
       Preview #1 may still be streaming this exact file while preview #2
       ReplaceBuffer()s it on disk; the audio-thread streamer holds its own
       WavFile open over that path, so a mid-read truncate can yield a corrupt
       read and crash the port. Prior order (rewrite, then Stop+Sleep) made the
       crash timing-dependent (reported once over many previews). */
    if (Player::GetInstance()->IsStreaming()) {
        Player::GetInstance()->StopStreaming();
        TimeService::GetInstance()->Sleep(80);
    }
    bool ok = writePreviewPitchWav(pitched, frames, channels, rate, logical);
    free(pitched);
    if (!ok) { clearOperationProgress(); setStatus("Pitch preview write fail"); return; }
    showOperationProgress("Operacion Preview", 90);
    Path path(logical.c_str());
    Player::GetInstance()->StartStreamingRangeAt(path, 0, frames > 0 ? frames - 1 : 0);
    playbackTriggered_ = true;
    previewActive_ = false;
#if defined(PLATFORM_TREEFROG)
    g_chopperPreviewActive = 0;
#endif
    clearOperationProgress();
    char msg[64]; snprintf(msg, sizeof(msg), "Preview %s P%+d A%d S%d R%d",
                           pitchEnvTool_.Params().scope ? "Chop" : "Sample",
                           pitchEnvTool_.Params().semitones,
                           pitchEnvTool_.Params().attackMs,
                           pitchEnvTool_.Params().sustainPercent,
                           pitchEnvTool_.Params().releaseMs);
    setStatus(msg);
    DrawView();
    publishOverlayState();
#if defined(PLATFORM_TREEFROG)
    TreeFrogForceVideoRefresh();
#endif
    isDirty_ = true;
}

bool SampleChopperModal::destructivePitchSample(int semitones) {
    if (!trimMode_ && !pitchMode_) { setStatus("Use PITCH/ENV first"); return false; }
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to pitch"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Pitch WAV only"); return false; }
    if (!hasPitchEnvelopeChange()) { setStatus("Pitch/env unchanged"); return false; }
    if (semitones < -12) semitones = -12;
    if (semitones > 12) semitones = 12;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) { setStatus("No source"); return false; }
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    initializeChopsIfNeeded();
    int editStart = 0;
    int editEnd = size - 1;
    if (pitchEnvTool_.Params().scope) {
        if (!hasActiveSliceRange()) { setStatus("No chop selected"); return false; }
        editStart = selectedChopStartFrame();
        editEnd = selectedChopEndFrame();
        editStart = clampInt(editStart, 0, size - 1);
        editEnd = clampInt(editEnd, editStart, size - 1);
    }
    int originalRangeFrames = editEnd - editStart + 1;
    if (originalRangeFrames <= 1) { setStatus("Range too small"); return false; }

    stopSamplePreview();
    lgptStopAllAudioBeforeDestructiveEdit();
    setOperationCombo("A");
    char label[56]; snprintf(label, sizeof(label), "Operacion Pitch %s P%+d", pitchEnvTool_.Params().scope ? "chop" : "sample", semitones);
    showOperationProgress(label, 0);
    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, pitchEnvTool_.Params().scope ? "PitchEnvChop" : "PitchEnv", chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress(label, 15);

    short *processed = 0;
    int processedFrames = 0, processedChannels = 0, processedRate = 0;
    if (!buildPitchEnvelopeBufferFromRange(editStart, editEnd, semitones, &processed, &processedFrames, &processedChannels, &processedRate)) {
        clearOperationProgress(); setStatus("Pitch build fail"); return false;
    }
    if (processedChannels != channels || processedRate != rate || processedFrames <= 1) {
        if (processed) free(processed);
        clearOperationProgress(); setStatus("Pitch format fail"); return false;
    }
    showOperationProgress(label, 55);

    int nextSize = processedFrames;
    short *nextBuffer = processed;
    int oldBoundaries[MAX_CHOP_BOUNDARIES];
    int oldCount = chopModel_.boundaryCount;
    int oldSelected = chopModel_.selected;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) oldBoundaries[i] = chopModel_.boundaries[i];

    if (pitchEnvTool_.Params().scope) {
        int beforeFrames = editStart;
        int afterStart = editEnd + 1;
        int afterFrames = size - afterStart;
        if (afterFrames < 0) afterFrames = 0;
        nextSize = beforeFrames + processedFrames + afterFrames;
        if (nextSize < 2 || nextSize > 40000000) { free(processed); clearOperationProgress(); setStatus("Pitch too large"); return false; }
        nextBuffer = (short *)malloc(nextSize * channels * sizeof(short));
        if (!nextBuffer) { free(processed); clearOperationProgress(); setStatus("No pitch memory"); return false; }
        if (beforeFrames > 0) memcpy(nextBuffer, samples, beforeFrames * channels * sizeof(short));
        memcpy(nextBuffer + beforeFrames * channels, processed, processedFrames * channels * sizeof(short));
        if (afterFrames > 0) memcpy(nextBuffer + (beforeFrames + processedFrames) * channels,
                                    samples + afterStart * channels,
                                    afterFrames * channels * sizeof(short));
        free(processed);
    }
    showOperationProgress(label, 72);

    WavFile *wav = (WavFile *)source;
    bool ok = wav->ReplaceBuffer(nextBuffer, nextSize, channels, rate);
    free(nextBuffer);
    if (!ok) { clearOperationProgress(); setStatus("Cannot pitch buffer"); return false; }
    showOperationProgress(label, 80);
    if (!wav->SaveBufferToPath(samplePath_.c_str())) { clearOperationProgress(); setStatus("Cannot write pitch"); return false; }
    sync();
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }
    showOperationProgress(label, 90);

    int nextBoundaries[MAX_CHOP_BOUNDARIES];
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) nextBoundaries[i] = 0;
    int out = 0;
    if (!pitchEnvTool_.Params().scope) {
        double scale = (size > 1) ? ((double)(nextSize - 1) / (double)(size - 1)) : 1.0;
        for (int i = 0; i < oldCount && i < MAX_CHOP_BOUNDARIES; i++) {
            int v = (int)(((double)oldBoundaries[i] * scale) + 0.5);
            if (v < 0) v = 0;
            if (v >= nextSize) v = nextSize - 1;
            if (out == 0 || v > nextBoundaries[out - 1]) nextBoundaries[out++] = v;
        }
    } else {
        int deltaFrames = processedFrames - originalRangeFrames;
        for (int i = 0; i < oldCount && i < MAX_CHOP_BOUNDARIES; i++) {
            int v = oldBoundaries[i];
            if (i == oldSelected + 1) v = editStart + processedFrames - 1;
            else if (i > oldSelected + 1) v = oldBoundaries[i] + deltaFrames;
            if (v < 0) v = 0;
            if (v >= nextSize) v = nextSize - 1;
            if (out == 0 || v > nextBoundaries[out - 1]) nextBoundaries[out++] = v;
        }
    }
    if (out <= 0) nextBoundaries[out++] = 0;
    nextBoundaries[0] = 0;
    if (nextBoundaries[out - 1] != nextSize - 1) {
        if (out < MAX_CHOP_BOUNDARIES) nextBoundaries[out++] = nextSize - 1;
        else nextBoundaries[out - 1] = nextSize - 1;
    }
    if (out < 2) { out = 2; nextBoundaries[0] = 0; nextBoundaries[1] = nextSize - 1; }
    chopModel_.boundaryCount = out;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) chopModel_.boundaries[i] = (i < chopModel_.boundaryCount) ? nextBoundaries[i] : 0;

    sourceSize_ = nextSize;
    sampleSize_ = nextSize;
    chopModel_.selected = clampInt(oldSelected, 0, chopModel_.boundaryCount - 2);
    cursorFrame_ = chopModel_.boundaries[chopModel_.selected];
    viewStartFrame_ = 0;
    chopsInitialized_ = true;
    lgptFinishDestructiveEdit(chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    showOperationProgress(pitchEnvTool_.Params().scope ? "Operacion Pitch chop" : "Operacion Pitch", 100);
    return true;
}

bool SampleChopperModal::normalizeSample() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No sample to normalize"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Normalize WAV only"); return false; }

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) { setStatus("No source"); return false; }
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    int total = size * channels;
    int peak = 0;
    for (int i = 0; i < total; i++) {
        int v = samples[i];
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    if (peak <= 0) { setStatus("Silent sample, nothing to normalize"); return false; }
    if (peak >= 32600) { setStatus("Already at full level"); return false; }

    double gain = 32767.0 / (double)peak;
    stopSamplePreview();
    lgptStopAllAudioBeforeDestructiveEdit();
    setOperationCombo("R2 + Y");
    char label[56]; snprintf(label, sizeof(label), "Operacion normalizar peak %d", peak);
    showOperationProgress(label, 0);
    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Normalize", chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress(label, 40);

    short *nextBuffer = (short *)malloc(total * sizeof(short));
    if (!nextBuffer) { clearOperationProgress(); setStatus("No normalize memory"); return false; }
    for (int i = 0; i < total; i++) {
        int v = (int)((double)samples[i] * gain);
        if (v < -32767) v = -32767;
        if (v > 32767) v = 32767;
        nextBuffer[i] = (short)v;
    }
    showOperationProgress(label, 60);

    WavFile *wav = (WavFile *)source;
    bool ok = wav->ReplaceBuffer(nextBuffer, size, channels, rate);
    free(nextBuffer);
    if (!ok) { clearOperationProgress(); setStatus("Cannot replace buffer"); return false; }
    showOperationProgress(label, 72);
    if (!wav->SaveBufferToPath(samplePath_.c_str())) { clearOperationProgress(); setStatus("Cannot write WAV"); return false; }
    sync();
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }
    showOperationProgress(label, 90);

    /* The frame length is unchanged, so chop boundaries stay valid as-is. */
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++)
        if (i >= chopModel_.boundaryCount) chopModel_.boundaries[i] = 0;
    sourceSize_ = size;
    sampleSize_ = size;
    chopModel_.selected = clampInt(chopModel_.selected, 0, chopModel_.boundaryCount - 2);
    cursorFrame_ = chopModel_.boundaries[chopModel_.selected];
    viewStartFrame_ = 0;
    chopsInitialized_ = true;
    lgptFinishDestructiveEdit(chopModel_.boundaries, chopModel_.boundaryCount, chopModel_.selected, sourceSize_);
    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    char done[64]; snprintf(done, sizeof(done), "Normalized peak %d -> 32767 (0 dB)", peak);
    showOperationProgress(done, 100);
    return true;
}

void SampleChopperModal::previewTrimStart() {
    if (sourceSize_ <= 1) { setStatus("No sample"); return; }
    initializeChopsIfNeeded();
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    int previewEnd = start + (sourceRate_ > 0 ? sourceRate_ * 5 : 220500);
    if (previewEnd > end) previewEnd = end;
    playFrameRange(start, previewEnd, "Preview start");
}

void SampleChopperModal::previewTrimEnd() {
    if (sourceSize_ <= 1) { setStatus("No sample"); return; }
    initializeChopsIfNeeded();
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    int previewStart = end - (sourceRate_ > 0 ? sourceRate_ * 1 : 44100);
    if (previewStart < start) previewStart = start;
    playFrameRange(previewStart, end, "Preview end");
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
    // TREEFROG_CHOPPER_HELP_V2 (Bacon 1.1.1 V16): while suspended under a
    // pushed modal (Help), the overlay must stay disabled; DrawView() keeps
    // running and would otherwise re-publish it over the modal.
    g_chopperOverlayActive = (!suspended_ && hasWaveform_ && !operationActive_ && !pitchMode_) ? 1 : 0;
    g_chopperCursorPx = frameToPixel(cursorFrame_);
    if (g_chopperCursorPx < 0) g_chopperCursorPx = 0;
    for (int i = 0; i < MAX_COLUMNS; i++) { g_chopperMinColumn[i] = minColumn_[i]; g_chopperMaxColumn[i] = maxColumn_[i]; }
    g_chopperMarkerCount = 0;
    for (int b = 1; b < chopModel_.boundaryCount - 1 && g_chopperMarkerCount < TF_MAX_CHOP_MARKERS; b++) {
        int px = frameToPixel(chopModel_.boundaries[b]);
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
    if (hasActiveSliceRange() && chopModel_.selected >= 0 && chopModel_.selected <= chopModel_.boundaryCount - 2) {
        int startFrame = chopModel_.boundaries[chopModel_.selected];
        int endFrame = chopModel_.boundaries[chopModel_.selected + 1];
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
                                              instrumentIndex_, chopModel_.selected, 0,
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
    if (sourceSize_ > 1) playFrameRange(0, sourceSize_ - 1, "Play full");
    else playFromFrame(0, "Play full");
}

void SampleChopperModal::playSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasActiveSliceRange()) { playFullSample(); return; }
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

    int chopCount = chopModel_.boundaryCount - 1;
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
        if (!configureChopInstrument((int)nextInstr, chopModel_.boundaries[i], chopModel_.boundaries[i + 1])) break;

        int row = rowStart + i;
        int offset = 16 * phraseIndex + row;
        phrase->note_[offset] = 60;
        phrase->instr_[offset] = (unsigned char)nextInstr;
        phrase->vol_[offset] = 0x64;
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

/* TREEFROG_U2_39_CHOPPER_ZOMBIE_VOICE_GUARD (Bacon 1.2.1): any destructive
   edit that will ReplaceBuffer() the shared WAV must halt ALL audio first
   (pattern voices + streaming preview), same rule as Crop/Delete. */
static void lgptStopAllAudioBeforeDestructiveEdit() {
    Player *p = Player::GetInstance();
    if (p) {
        if (p->IsRunning()) p->Stop();
        if (p->IsStreaming()) p->StopStreaming();
    }
}

void SampleChopperModal::drawStringAbs(int x, int y, const char *txt, GUITextProperties &props) { View::DrawString(x, y, txt, props); }
void SampleChopperModal::clearTextScreen() { View::ClearRect(0, 0, SCREEN_W, SCREEN_H); }
void SampleChopperModal::drawTopBar(GUITextProperties &props) { props.invert_ = true; SetColor(CD_HILITE1); drawStringAbs(0, 0, " P G  SCPI  M TT       CHOPPER       ", props); props.invert_ = false; }

void SampleChopperModal::drawFrame(GUITextProperties &props) {
    /* RC4 P6 (PLAN_RC4 11.7): solid-border frame, no ASCII box-drawing.
       Same 40-cell geometry (rows 1..22, columns 0/39) so the waveform
       overlay and the char-screen text stay aligned. */
    SetColor(CD_BORDER);
    for (int x = 0; x < 40; x++) {
        char cell[2] = {' ', 0};
        props.invert_ = true;
        drawStringAbs(x, 1, cell, props);
        drawStringAbs(x, 22, cell, props);
    }
    for (int y = 2; y < 22; y++) {
        char cell[2] = {' ', 0};
        props.invert_ = true;
        drawStringAbs(0, y, cell, props);
        drawStringAbs(39, y, cell, props);
    }
    props.invert_ = false;
    SetColor(CD_HILITE2);
    drawStringAbs(2, 2, "Graphical Chopper", props);
}

void SampleChopperModal::drawSampleInfo(GUITextProperties &props) {
    char buffer[96];
    SetColor(CD_NORMAL);
    snprintf(buffer, sizeof(buffer), "Inst:%02X Smpl:%02X Zoom:%03d%%", instrumentIndex_, sampleIndex_ < 0 ? 0 : sampleIndex_, zoomPercent_);
    buffer[37] = 0; drawStringAbs(2, 4, buffer, props);
    if (!hasAssignedSample()) { drawStringAbs(2, 5, "No sample assigned", props); drawStringAbs(2, 6, "No chop actions", props); }
    else { std::string name = sampleName_; if (name.size() > 31) name = name.substr(0, 31); snprintf(buffer, sizeof(buffer), "Name:%s", name.c_str()); buffer[37] = 0; drawStringAbs(2, 5, buffer, props); snprintf(buffer, sizeof(buffer), "Frame:%d/%d Chop:%02d/%02d%s", cursorFrame_, sourceSize_ > 0 ? sourceSize_ - 1 : 0, hasActiveSliceRange() ? chopModel_.selected : 0, hasActiveSliceRange() ? (chopModel_.boundaryCount - 1) : 0, trimMode_ ? " ADJ" : ""); buffer[37] = 0; drawStringAbs(2, 6, buffer, props); }
    if (statusMessage_[0]) { SetColor(CD_HILITE1); drawStringAbs(2, 23, statusMessage_, props); }
}

void SampleChopperModal::drawEmptyWaveformText(GUITextProperties &props) { SetColor(CD_HILITE1); drawStringAbs(2, 13, "            no sample loaded            ", props); }

void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    if (pitchMode_) {
        /* U2.27: the compact Pitch/Env panel carries its own controls.
           Avoid duplicating help lines at the bottom of the 320x240 screen. */
        return;
    }
    /* Bacon 1.1.1 V13: single hint line on the chopper; the full combo list
       lives in the CHOPPER help section (SELECT+R1).  Trim mode swaps the
       line for the crop actions. */
    drawStringAbs(0, 24,
                  trimMode_ ? "R1+A Keep  L2+Y Del  A+B Nudge  R1+B Back"
                            : "Select: Crop | L1+R1: Pitch | R1+B: Back",
                  props);
}

void SampleChopperModal::showOperationProgress(const char *message, int percent) {
    operationActive_ = true;
    operationPercent_ = clampInt(percent, 0, 100);
    snprintf(operationMessage_, sizeof(operationMessage_), "%s", message ? message : "Working");
    operationMessage_[sizeof(operationMessage_) - 1] = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 1;
    g_chopperOperationPercent = operationPercent_;
#endif
    char status[64];
    /* Bacon 1.1.1 V17: progress overlays name the operation AND the trigger
       combo, e.g. "R2 + Y Operacion normalizar 45%" / "R1 + X Operacion
       Redo OK" (helper: setOperationCombo() before the operation). */
    if (operationPercent_ >= 100)
        snprintf(status, sizeof(status), "%s %s OK A/L1+X/R1+X",
                 operationComboLabel_, operationMessage_);
    else
        snprintf(status, sizeof(status), "%s %s %d%%",
                 operationComboLabel_, operationMessage_, operationPercent_);
    setStatus(status);
    DrawView();
    publishOverlayState();
    w_.Flush();
#if defined(PLATFORM_TREEFROG)
    /* U2.26: Flush updates the internal framebuffer, but RetroArch normally
       presents it only after the input callback returns. Force one video
       refresh here so intermediate CROP/PITCH percentages are actually visible
       while the destructive operation is still running. */
    TreeFrogForceVideoRefresh();
#endif
    if (operationPercent_ < 100) TimeService::GetInstance()->Sleep(90);
    isDirty_ = true;
}

void SampleChopperModal::clearOperationProgress() {
    operationActive_ = false;
    operationPercent_ = 0;
    operationMessage_[0] = 0;
    operationComboLabel_[0] = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 0;
    g_chopperOperationPercent = 0;
#endif
    isDirty_ = true;
}

void SampleChopperModal::drawOperationOverlay(GUITextProperties &props) {
    if (!operationActive_) return;
    char msg[64];
    /* Bacon 1.1.1 V15: port-wide graphical language (centered title over a
       label/value block, same as the Pitch/Env panel) -- no ASCII box of
       '+'/'-'/'|' characters.  The percent row is the current operation. */
    /* Bacon 1.1.1 V16: the operation panel sits over the pixel waveform
       band; clear its full cell area (title row 9 through hint row 13, y
       72..127) so stale waveform pixels never show between the text rows.
       The glyph cells are opaque, but the panel has no box around it. */
    tf_rect(0, 72, 320, 56, tf_rgb565(10, 10, 24));
    UiDraw::DrawCenteredTitleAt(*this, 10, "OPERATION");
    MenuLayout ml = UiDraw::MakeCenteredMenuLayout(2, 10, 22, 2);
    SetColor(CD_NORMAL);
    props.invert_ = false;
    DrawString(ml.labelX, ml.startY, "Status", props);
    SetColor(CD_HILITE2);
    snprintf(msg, sizeof(msg), "%-22.22s", operationMessage_);
    DrawString(ml.valueX, ml.startY, msg, props);
    SetColor(CD_HILITE1);
    props.invert_ = true;
    snprintf(msg, sizeof(msg), "%3d%%", operationPercent_);
    DrawString(ml.labelX, ml.startY + 1, msg, props);
    props.invert_ = false;
    SetColor(CD_NORMAL);
    DrawString(2, ml.startY + 3,
               operationPercent_ >= 100
                   ? "A close  L1+X undo  R1+X redo"
                   : "Processing sample, please wait",
               props);
}

void SampleChopperModal::DrawView() {
    GUITextProperties props; props.invert_ = false;
    clearTextScreen();
    drawTopBar(props);
    drawFrame(props);
    drawSampleInfo(props);
    if (!hasWaveform_) drawEmptyWaveformText(props);
    drawControls(props);
    drawPitchScreen(props);
    drawOperationOverlay(props);
    publishOverlayState();
}

void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;

    /* F1 input policy (REFACTOR_ROADMAP_ES.md, fase 1): el input fisico se
     * resuelve contra el catalogo dorado (ActionMap.cpp) usando el contexto
     * actual del chopper. La tabla transcribe 1:1 el orden y las condiciones
     * de esta funcion en Bacon 1.2.1; el dispatch llama a los mismos metodos
     * (y los mismos messages de status) que la implementacion original. */
    using namespace UI::Input;
    const PadMask pad = (PadMask)mask;  /* los bits EPBM_* espejan PhysicalKey */
    const ContextId ctx = pitchMode_ ? CTX_CHOPPER_PITCH
                        : (trimMode_ ? CTX_CHOPPER_TRIM : CTX_CHOPPER);
    const ActionId action = ChordResolver_Resolve(pad, ctx);

    /* U2.51.0: completion overlays must not swallow undo/redo. (Equivalente
     * exacto del golden: L1+X / R1+X puros curan el overlay, A puro
     * lo cierra, cualquier otra tecla se consume durante el overlay.) */
    if (operationActive_ && operationPercent_ >= 100) {
        if (pad == (KEY_L1 | KEY_X)) {
            clearOperationProgress(); undoLastChopperEdit(); return;
        }
        if (pad == (KEY_R1 | KEY_X)) {
            clearOperationProgress(); redoLastChopperEdit(); return;
        }
        if (pad == KEY_A) { clearOperationProgress(); DrawView(); publishOverlayState(); }
        return;
    }

    switch (action) {
        case ACTION_UNDO: undoLastChopperEdit(); return;
        case ACTION_REDO: redoLastChopperEdit(); return;

        /* L1+R1 puro: entra/sale del modo Pitch. */
        case ACTION_TOGGLE_PITCH_MODE: togglePitchMode(); return;

        /* L1+B: en trim snap del fin al zero-cross; en main cicla el split
         * (la rama original es trim-condicional y anterior al bloque pitch). */
        case ACTION_SPLIT_PARTS:
            if (trimMode_) { snapSelectedBoundaryToZeroCross(false); return; }
            cycleSplitParts(); return;
        case ACTION_SNAP_BOUNDARY_END: snapSelectedBoundaryToZeroCross(false); return;
        case ACTION_SNAP_BOUNDARY_START: snapSelectedBoundaryToZeroCross(true); return;
        case ACTION_NORMALIZE: normalizeSample(); return;

        case ACTION_STOP_PREVIEW:
            stopSamplePreview();
            setStatus(ctx == CTX_CHOPPER_PITCH ? "Stop preview" : "Stop playback");
            return;

        case ACTION_PITCH_SCOPE_NEXT:
        case ACTION_PITCH_SCOPE_PREV: {
            if (pitchEnvTool_.Params().scope) {
                selectChop(action == ACTION_PITCH_SCOPE_NEXT ? 1 : -1);
                char msg[64];
                snprintf(msg, sizeof(msg), "Pitch chop %02d/%02d",
                         chopModel_.selected + 1,
                         (chopModel_.boundaryCount > 1 ? chopModel_.boundaryCount - 1 : 1));
                setStatus(msg);
            } else {
                setStatus("Set Scope Chop first");
            }
            return;
        }

        /* R1+B: cierra el modal (EndModal) y para el preview. */
        case ACTION_CLOSE:
            stopSamplePreview(); EndModal(0); isDirty_ = true; return;

        /* SELECT puro: alterna trim mode. */
        case ACTION_TOGGLE_TRIM_MODE: toggleTrimMode(); return;

        case ACTION_CROP: destructiveCropToSelectedRange(); return;
        case ACTION_DELETE_RANGE: destructiveDeleteSelectedRange(); return;

        /* Trim: A/B+flechas nudgean inicio/fin (L1 x10). */
        case ACTION_NUDGE_START_LEFT:
            nudgeSelectedStart(-getFrameStepForEdit()); return;
        case ACTION_NUDGE_START_RIGHT:
            nudgeSelectedStart(getFrameStepForEdit()); return;
        case ACTION_NUDGE_START_LEFT_COARSE:
            nudgeSelectedStart(-getFrameStepForEdit() * 10); return;
        case ACTION_NUDGE_START_RIGHT_COARSE:
            nudgeSelectedStart(getFrameStepForEdit() * 10); return;
        case ACTION_NUDGE_END_LEFT:
            nudgeSelectedEnd(-getFrameStepForEdit()); return;
        case ACTION_NUDGE_END_RIGHT:
            nudgeSelectedEnd(getFrameStepForEdit()); return;
        case ACTION_NUDGE_END_LEFT_COARSE:
            nudgeSelectedEnd(-getFrameStepForEdit() * 10); return;
        case ACTION_NUDGE_END_RIGHT_COARSE:
            nudgeSelectedEnd(getFrameStepForEdit() * 10); return;

        case ACTION_TRIM_PREVIEW_START: previewTrimStart(); return;
        case ACTION_TRIM_PREVIEW_END: previewTrimEnd(); return;

        /* R1+A fuera de trim: auto-save + status. (En trim solo alcanza
         * cuando la rama crop no lo captura, igual que el golden.) */
        case ACTION_AUTOSAVE_CHOPS:
            saveChopStateForCurrentSample();
            setStatus("Auto-save on: assign Sxx in Phrase");
            return;

        case ACTION_SELECT_PREV_SAMPLE: selectSample(-1); return;
        case ACTION_SELECT_NEXT_SAMPLE: selectSample(1); return;
        case ACTION_PLAY_FULL: playFullSample(); return;
        case ACTION_SELECT_PREV_CHOP: selectChop(-1); return;
        case ACTION_SELECT_NEXT_CHOP: selectChop(1); return;

        case ACTION_DELETE_CHOP: deleteSelectedChop(); return;
        case ACTION_PLAY_CHOP_PREVIEW: playSelectedChop(); return;
        case ACTION_ADD_CHOP: addChopAtCursor(); return;

        /* Cursor y zoom, con L1 grueso (misma rama, L1 multiplica). */
        case ACTION_NUDGE_CURSOR_LEFT: nudgeCursorPixels(-2); return;
        case ACTION_NUDGE_CURSOR_RIGHT: nudgeCursorPixels(2); return;
        case ACTION_NUDGE_CURSOR_LEFT_COARSE: nudgeCursorPixels(-24); return;
        case ACTION_NUDGE_CURSOR_RIGHT_COARSE: nudgeCursorPixels(24); return;
        case ACTION_ZOOM_IN: nudgeZoomPercent(5); return;
        case ACTION_ZOOM_OUT: nudgeZoomPercent(-5); return;
        case ACTION_ZOOM_IN_COARSE: nudgeZoomPercent(10); return;
        case ACTION_ZOOM_OUT_COARSE: nudgeZoomPercent(-10); return;

        /* Pitch: U/D parametro, L/R valor, B preview, A apply. */
        case ACTION_PITCH_PARAM_NEXT: selectPitchEditParam(1); return;
        case ACTION_PITCH_PARAM_PREV: selectPitchEditParam(-1); return;
        case ACTION_PITCH_VALUE_UP: nudgePitchEnvelopeValue(1); return;
        case ACTION_PITCH_VALUE_DOWN: nudgePitchEnvelopeValue(-1); return;
        case ACTION_PITCH_PREVIEW: previewPitchSetting(); return;
        case ACTION_PITCH_APPLY: destructivePitchSample(pitchEnvTool_.Params().semitones); return;

        /* Nada resuelto. El bloque pitch del golden consume por defecto con
         * status; los hints de trim tambien son consumidos con status. */
        case ACTION_NONE:
            if (ctx == CTX_CHOPPER_PITCH) {
                /* Gates trim anteriores al bloque pitch (pitch+trim):
                 * L1+A snap del inicio, R2+Y normalizar. */
                if (trimMode_ && pad == (KEY_L1 | KEY_A)) {
                    snapSelectedBoundaryToZeroCross(true); return;
                }
                if (trimMode_ && pad == (KEY_R2 | KEY_Y)) {
                    normalizeSample(); return;
                }
                if (pad == KEY_SELECT) {
                    setStatus("Pitch/env: L1+R1 exit"); return;
                }
                setStatus("Pitch/env: UD item LR value");
                return;
            }
            if (ctx == CTX_CHOPPER_TRIM) {
                if ((pad & (KEY_A | KEY_B)) && !(pad & (KEY_LEFT | KEY_RIGHT))) {
                    setStatus((pad & KEY_A) ? "Crop: A+LEFT/RIGHT"
                                            : "Crop: B+LEFT/RIGHT");
                    return;
                }
                if ((pad & KEY_L2) && (pad & KEY_Y)) {
                    setStatus("Delete: L2+Y"); return;
                }
            }
            return;
        default:
            return;
    }
}

