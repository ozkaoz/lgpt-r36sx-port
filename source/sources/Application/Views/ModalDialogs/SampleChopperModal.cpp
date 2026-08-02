#include "SampleChopperModal.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SoundSource.h"
#include "Application/Instruments/WavFile.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Player/Player.h"
#include "System/FileSystem/FileSystem.h"
#include "Services/Time/TimeService.h"
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
      pitchSemitones_(0),
      pitchEditParam_(0),
      pitchAttackMs_(0),
      pitchSustainPercent_(100),
      pitchReleaseMs_(0),
      pitchScope_(0),
      selectedChop_(0),
      boundaryCount_(0),
      undoHistoryCount_(0),
      redoHistoryCount_(0),
      sampleName_(sampleName ? sampleName : "") {
    statusMessage_[0] = 0;
    operationMessage_[0] = 0;
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
    return pitchSemitones_ != 0 || pitchAttackMs_ != 0 || pitchReleaseMs_ != 0 || pitchSustainPercent_ != 100;
}

void SampleChopperModal::resetPitchEnvelopeSettings() {
    pitchSemitones_ = 0;
    pitchEditParam_ = 0;
    pitchAttackMs_ = 0;
    pitchSustainPercent_ = 100;
    pitchReleaseMs_ = 0;
    pitchScope_ = 0;
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
    saved.boundaryCount = clampInt(boundaryCount_, 2, MAX_CHOP_BOUNDARIES);
    saved.selectedChop = clampInt(selectedChop_, 0, saved.boundaryCount - 2);
    for (int i = 0; i < saved.boundaryCount; i++) saved.boundaries[i] = boundaries_[i];
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
    pitchMode_ = false;
    resetPitchEnvelopeSettings();
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

void SampleChopperModal::captureLogicalState(
    LogicalHistoryState &state,
    const char *action) const {
    state.sampleIndex = sampleIndex_;
    state.sourceSize = sourceSize_;
    state.selectedChop = selectedChop_;
    state.boundaryCount = boundaryCount_;
    state.cursorFrame = cursorFrame_;
    state.viewStartFrame = viewStartFrame_;
    state.zoomPercent = zoomPercent_;
    state.trimMode = trimMode_;
    state.pitchMode = pitchMode_;
    state.pitchSemitones = pitchSemitones_;
    state.pitchEditParam = pitchEditParam_;
    state.pitchAttackMs = pitchAttackMs_;
    state.pitchSustainPercent = pitchSustainPercent_;
    state.pitchReleaseMs = pitchReleaseMs_;
    state.pitchScope = pitchScope_;

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
            i < boundaryCount_ ? boundaries_[i] : 0;
}

void SampleChopperModal::restoreLogicalState(
    const LogicalHistoryState &state) {
    sourceSize_ = state.sourceSize;
    sampleSize_ = state.sourceSize;
    boundaryCount_ =
        clampInt(
            state.boundaryCount,
            2,
            MAX_CHOP_BOUNDARIES);

    for (int i = 0; i < MAX_CHOP_BOUNDARIES; ++i)
        boundaries_[i] = 0;

    for (int i = 0; i < boundaryCount_; ++i)
        boundaries_[i] =
            clampInt(
                state.boundaries[i],
                0,
                sourceSize_ > 0 ? sourceSize_ - 1 : 0);

    if (sourceSize_ > 1) {
        boundaries_[0] = 0;
        boundaries_[boundaryCount_ - 1] =
            sourceSize_ - 1;
    }

    for (int i = 1; i < boundaryCount_; ++i) {
        if (boundaries_[i] <= boundaries_[i - 1])
            boundaries_[i] =
                boundaries_[i - 1] + 1;
        if (sourceSize_ > 0 &&
            boundaries_[i] >= sourceSize_)
            boundaries_[i] =
                sourceSize_ - 1;
    }

    selectedChop_ =
        clampInt(
            state.selectedChop,
            0,
            boundaryCount_ - 2);
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
    pitchSemitones_ = state.pitchSemitones;
    pitchEditParam_ = clampInt(state.pitchEditParam, 0, 5);
    pitchAttackMs_ = clampInt(state.pitchAttackMs, 0, 5000);
    pitchSustainPercent_ = clampInt(state.pitchSustainPercent, 0, 150);
    pitchReleaseMs_ = clampInt(state.pitchReleaseMs, 0, 5000);
    pitchScope_ = state.pitchScope ? 1 : 0;
    chopsInitialized_ = true;

    saveChopStateForCurrentSample();
    clampViewStart();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    isDirty_ = true;
}

void SampleChopperModal::clearLogicalRedo() {
    redoHistoryCount_ = 0;
}

void SampleChopperModal::clearLogicalHistory() {
    undoHistoryCount_ = 0;
    redoHistoryCount_ = 0;
}

void SampleChopperModal::pushLogicalUndo(
    const char *action) {
    /*
     * A new branch after physical Undo invalidates the destructive Redo.
     * The active audio is already the Undo state, so keeping the old Redo
     * would overwrite later logical edits.
     */
    if (g_lgptLastDestructiveEditUndone)
        lgptClearDestructiveEditHistory();

    if (undoHistoryCount_ >= MAX_LOGICAL_HISTORY) {
        for (int i = 1;
             i < MAX_LOGICAL_HISTORY;
             ++i) {
            undoHistory_[i - 1] =
                undoHistory_[i];
        }
        undoHistoryCount_ =
            MAX_LOGICAL_HISTORY - 1;
    }

    captureLogicalState(
        undoHistory_[undoHistoryCount_],
        action);
    ++undoHistoryCount_;
    clearLogicalRedo();
}

bool SampleChopperModal::undoLogicalEdit() {
    if (undoHistoryCount_ <= 0)
        return false;

    const LogicalHistoryState state =
        undoHistory_[undoHistoryCount_ - 1];

    if (state.sampleIndex != sampleIndex_ ||
        strcmp(
            state.samplePath,
            samplePath_.c_str()) != 0 ||
        state.sourceSize != sourceSize_) {
        clearLogicalHistory();
        setStatus("Undo history does not match sample");
        return false;
    }

    if (redoHistoryCount_ >= MAX_LOGICAL_HISTORY) {
        for (int i = 1;
             i < MAX_LOGICAL_HISTORY;
             ++i) {
            redoHistory_[i - 1] =
                redoHistory_[i];
        }
        redoHistoryCount_ =
            MAX_LOGICAL_HISTORY - 1;
    }

    captureLogicalState(
        redoHistory_[redoHistoryCount_],
        state.action);
    ++redoHistoryCount_;
    --undoHistoryCount_;

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
    if (redoHistoryCount_ <= 0)
        return false;

    const LogicalHistoryState state =
        redoHistory_[redoHistoryCount_ - 1];

    if (state.sampleIndex != sampleIndex_ ||
        strcmp(
            state.samplePath,
            samplePath_.c_str()) != 0 ||
        state.sourceSize != sourceSize_) {
        clearLogicalHistory();
        setStatus("Redo history does not match sample");
        return false;
    }

    if (undoHistoryCount_ >= MAX_LOGICAL_HISTORY) {
        for (int i = 1;
             i < MAX_LOGICAL_HISTORY;
             ++i) {
            undoHistory_[i - 1] =
                undoHistory_[i];
        }
        undoHistoryCount_ =
            MAX_LOGICAL_HISTORY - 1;
    }

    captureLogicalState(
        undoHistory_[undoHistoryCount_],
        state.action);
    ++undoHistoryCount_;
    --redoHistoryCount_;

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
    if (boundaryCount_ >= MAX_CHOP_BOUNDARIES) { setStatus("Max 100 chops reached"); return; }
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
    int minEdge = (boundaryCount_ >= 2) ? boundaries_[0] : 0;
    int maxEdge = (boundaryCount_ >= 2) ? boundaries_[boundaryCount_ - 1] : (sourceSize_ - 1);
    if (frame <= minEdge || frame >= maxEdge) { setStatus("Cannot chop at edge"); return; }
    for (int i = 0; i < boundaryCount_; i++) {
        if (abs(boundaries_[i] - frame) <= 1) { setStatus("Chop already exists"); return; }
    }
    pushLogicalUndo("Add cut");
    boundaries_[boundaryCount_++] = frame;
    sortBoundaries();
    int idx = findBoundaryIndex(frame);
    if (idx > 0) selectedChop_ = idx - 1;
    if (selectedChop_ < 0) selectedChop_ = 0;
    if (selectedChop_ > boundaryCount_ - 2) selectedChop_ = boundaryCount_ - 2;
    saveChopStateForCurrentSample();
    publishOverlayState();
    char msg[64]; snprintf(msg, sizeof(msg), liveCut ? "Live chop %02d at %d" : "Chop %02d at %d", selectedChop_, frame); setStatus(msg);
}

void SampleChopperModal::deleteSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { setStatus("No chop to delete"); return; }

    /* Chops are stored as boundaries. Deleting a chop removes one internal boundary
       and merges the selected region with a neighbor. Edge boundaries 0/end are never removed. */
    int removeIdx = (selectedChop_ > 0) ? selectedChop_ : 1;
    if (removeIdx <= 0 || removeIdx >= boundaryCount_ - 1) { setStatus("Cannot delete edge"); return; }

    pushLogicalUndo("Merge cuts");
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

bool SampleChopperModal::hasActiveSliceRange() const {
    if (boundaryCount_ < 2 || sourceSize_ <= 1) return false;
    if (boundaryCount_ > 2) return true;
    return (boundaries_[0] > 0 || boundaries_[1] < sourceSize_ - 1);
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
    if (boundaryCount_ < 2) { setStatus("No range to trim"); return; }
    int idx = selectedChop_;
    int minFrame = (idx == 0) ? 0 : boundaries_[idx - 1] + 1;
    int maxFrame = boundaries_[idx + 1] - 1;
    int nextFrame =
        clampInt(
            boundaries_[idx] + deltaFrames,
            minFrame,
            maxFrame);
    if (nextFrame == boundaries_[idx]) return;
    pushLogicalUndo("Move cut start");
    boundaries_[idx] = nextFrame;
    cursorFrame_ = boundaries_[idx];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Adjusted chop start");
}

void SampleChopperModal::nudgeSelectedEnd(int deltaFrames) {
    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to trim"); return; }
    int idx = selectedChop_ + 1;
    int minFrame = boundaries_[idx - 1] + 1;
    int maxFrame = (idx == boundaryCount_ - 1) ? (sourceSize_ - 1) : (boundaries_[idx + 1] - 1);
    int nextFrame =
        clampInt(
            boundaries_[idx] + deltaFrames,
            minFrame,
            maxFrame);
    if (nextFrame == boundaries_[idx]) return;
    pushLogicalUndo("Move cut end");
    boundaries_[idx] = nextFrame;
    cursorFrame_ = boundaries_[idx];
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Adjusted chop end");
}


void SampleChopperModal::cropToSelectedRange() {
    if (sourceSize_ <= 1) { setStatus("No sample to crop"); return; }
    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to crop"); return; }

    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    if (end <= start) { setStatus("Bad crop range"); return; }

    /* U2.14: safe logical crop. We keep the chosen/trimmed range as a single S01 slice
       and ignore material outside it at playback time. We do not rewrite the WAV file here. */
    pushLogicalUndo("Keep logical range");
    boundaryCount_ = 2;
    boundaries_[0] = start;
    boundaries_[1] = end;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
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


bool SampleChopperModal::destructiveCropToSelectedRange() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to crop"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Crop WAV only"); return false; }

    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to crop"); return false; }

    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
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
    showOperationProgress("Keep range", 5);

    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Keep range", 25);

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) { clearOperationProgress(); setStatus("No crop memory"); return false; }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));
    showOperationProgress("Keep range", 50);

    if (!wav->ReplaceBuffer(cropped, frameCount, channels, rate)) {
        free(cropped); clearOperationProgress(); setStatus("Cannot crop buffer"); return false;
    }
    free(cropped);
    showOperationProgress("Keep range", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview(); publishOverlayState();
        clearOperationProgress(); setStatus("Cannot write crop"); return false;
    }
    showOperationProgress("Keep range", 85);

    sourceSize_ = frameCount;
    sampleSize_ = frameCount;
    viewStartFrame_ = 0;
    cursorFrame_ = 0;
    boundaryCount_ = 2;
    boundaries_[0] = 0;
    boundaries_[1] = frameCount - 1;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
    trimMode_ = true;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
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

    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to delete"); return false; }

    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
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
    showOperationProgress("Delete range", 5);

    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Delete", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Delete range", 25);

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
    showOperationProgress("Delete range", 50);

    if (!wav->ReplaceBuffer(edited, nextSize, channels, rate)) {
        free(edited); clearOperationProgress(); setStatus("Cannot edit buffer"); return false;
    }
    free(edited);
    showOperationProgress("Delete range", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview(); publishOverlayState();
        clearOperationProgress(); setStatus("Cannot write edit"); return false;
    }
    showOperationProgress("Delete range", 85);

    int oldBoundaries[MAX_CHOP_BOUNDARIES];
    int oldCount = boundaryCount_;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) oldBoundaries[i] = boundaries_[i];
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
    boundaryCount_ = out;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = (i < boundaryCount_) ? nextBoundaries[i] : 0;
    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = boundaries_[selectedChop_];
    trimMode_ = true;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress("Delete complete", 100);
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
    clearLogicalHistory();
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 10);
    if (!lgptRestorePhysicalSnapshotToWav(wav, samplePath_.c_str(), restoreSamples,
                                          restoreFrames, restoreChannels, restoreRate)) {
        clearOperationProgress();
        setStatus(redo ? "Redo restore fail" : "Undo restore fail");
        return false;
    }
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 70);

    sourceSize_ = restoreFrames;
    sampleSize_ = restoreFrames;
    boundaryCount_ = clampInt(restoreCount, 2, MAX_CHOP_BOUNDARIES);
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    for (int i = 0; i < boundaryCount_; i++)
        boundaries_[i] = clampInt(restoreBoundaries[i], 0, restoreFrames - 1);
    boundaries_[0] = 0;
    boundaries_[boundaryCount_ - 1] = restoreFrames - 1;
    for (int i = 1; i < boundaryCount_; i++) {
        if (boundaries_[i] <= boundaries_[i - 1])
            boundaries_[i] = boundaries_[i - 1] + 1;
        if (boundaries_[i] >= restoreFrames)
            boundaries_[i] = restoreFrames - 1;
    }

    selectedChop_ = clampInt(restoreSelected, 0, boundaryCount_ - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = boundaries_[selectedChop_];
    trimMode_ = true;
    chopsInitialized_ = true;
    g_lgptLastDestructiveEditUndone = !redo;

    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress(redo ? "Redo complete" : "Undo complete", 100);
    return true;
}


void SampleChopperModal::togglePitchMode() {
    stopSamplePreview();
    if (!hasAssignedSample()) { setStatus("No sample for pitch"); return; }
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
    pitchEditParam_ = clampInt(pitchEditParam_ + delta, 0, 5);
    const char *name = "Pitch";
    if (pitchEditParam_ == 1) name = "Attack";
    else if (pitchEditParam_ == 2) name = "Sustain";
    else if (pitchEditParam_ == 3) name = "Release";
    else if (pitchEditParam_ == 4) name = "Scope";
    else if (pitchEditParam_ == 5) name = "Sample";
    char msg[64]; snprintf(msg, sizeof(msg), "%s selected", name);
    setStatus(msg);
    isDirty_ = true;
}

void SampleChopperModal::nudgePitchSemitones(int delta) {
    int next = clampInt(pitchSemitones_ + delta, -12, 12);
    if (next == pitchSemitones_) return;
    pushLogicalUndo("Pitch setting");
    pitchSemitones_ = next;
    char msg[64]; snprintf(msg, sizeof(msg), "Pitch %+d st", pitchSemitones_);
    setStatus(msg);
    isDirty_ = true;
}

void SampleChopperModal::nudgePitchEnvelopeValue(int delta) {
    if (pitchEditParam_ == 0) {
        nudgePitchSemitones(delta);
        return;
    }

    if (pitchEditParam_ == 1) {
        int next = clampInt(pitchAttackMs_ + (delta * 5), 0, 5000);
        if (next == pitchAttackMs_) return;
        pushLogicalUndo("Attack setting");
        pitchAttackMs_ = next;
        char msg[64]; snprintf(msg, sizeof(msg), "Attack %d ms", pitchAttackMs_);
        setStatus(msg);
    } else if (pitchEditParam_ == 2) {
        int next = clampInt(pitchSustainPercent_ + (delta * 5), 0, 150);
        if (next == pitchSustainPercent_) return;
        pushLogicalUndo("Sustain setting");
        pitchSustainPercent_ = next;
        char msg[64]; snprintf(msg, sizeof(msg), "Sustain %d%%", pitchSustainPercent_);
        setStatus(msg);
    } else if (pitchEditParam_ == 3) {
        int next = clampInt(pitchReleaseMs_ + (delta * 5), 0, 5000);
        if (next == pitchReleaseMs_) return;
        pushLogicalUndo("Release setting");
        pitchReleaseMs_ = next;
        char msg[64]; snprintf(msg, sizeof(msg), "Release %d ms", pitchReleaseMs_);
        setStatus(msg);
    } else if (pitchEditParam_ == 4) {
        pushLogicalUndo("Pitch scope");
        pitchScope_ = pitchScope_ ? 0 : 1;
        char msg[64]; snprintf(msg, sizeof(msg), "Scope %s", pitchScope_ ? "Chop" : "Sample");
        setStatus(msg);
    } else if (pitchEditParam_ == 5) {
        selectPitchTargetSample(delta);
        return;
    }
    isDirty_ = true;
}

void SampleChopperModal::selectPitchTargetSample(int delta) {
    int oldPitch = pitchSemitones_;
    int oldParam = pitchEditParam_;
    int oldAttack = pitchAttackMs_;
    int oldSustain = pitchSustainPercent_;
    int oldRelease = pitchReleaseMs_;
    int oldScope = pitchScope_;

    loadSampleByIndex(sampleIndex_ + delta, delta > 0 ? "Next pitch sample" : "Prev pitch sample");

    pitchMode_ = true;
    trimMode_ = false;
    pitchSemitones_ = oldPitch;
    pitchEditParam_ = oldParam;
    pitchAttackMs_ = oldAttack;
    pitchSustainPercent_ = oldSustain;
    pitchReleaseMs_ = oldRelease;
    pitchScope_ = oldScope;

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
    char msg[64];
    char line[64];

#if defined(PLATFORM_TREEFROG)
    /* U2.29: pitch mode must own the whole center panel area. Disable the
       waveform overlay before the next video refresh and clear the entire
       waveform band, not only the former narrow panel. This removes the right
       side bars visible in U2.28 and avoids drawing over the Frame line. */
    g_chopperOverlayActive = 0;
    tf_rect(0, 60, 320, 112, tf_rgb565(10, 10, 24));
    tf_rect(0, 188, 320, 44, tf_rgb565(10, 10, 24));
#endif

    const int x = 1;
    const int y = 8;
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(x, y + 0,  "+------------------------------------+", props);
    snprintf(line, sizeof(line), "PITCH/ENV U2.36");
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 1, msg, props);
    snprintf(line, sizeof(line), "I%02X S%02X C%02d/%02d", instrumentIndex_, sampleIndex_, selectedChop_ + 1, (boundaryCount_ > 1 ? boundaryCount_ - 1 : 1));
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 2, msg, props);
    snprintf(line, sizeof(line), "%cPitch:%+3d st", pitchEditParam_ == 0 ? '>' : ' ', pitchSemitones_);
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 3, msg, props);
    snprintf(line, sizeof(line), "%cAttack:%4d ms", pitchEditParam_ == 1 ? '>' : ' ', pitchAttackMs_);
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 4, msg, props);
    snprintf(line, sizeof(line), "%cSustain:%3d %%", pitchEditParam_ == 2 ? '>' : ' ', pitchSustainPercent_);
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 5, msg, props);
    snprintf(line, sizeof(line), "%cRelease:%4d ms", pitchEditParam_ == 3 ? '>' : ' ', pitchReleaseMs_);
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 6, msg, props);
    snprintf(line, sizeof(line), "%cScope:%-6s", pitchEditParam_ == 4 ? '>' : ' ', pitchScope_ ? "Chop" : "Sample");
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 7, msg, props);
    snprintf(line, sizeof(line), "%cSample:%02X", pitchEditParam_ == 5 ? '>' : ' ', sampleIndex_);
    snprintf(msg, sizeof(msg), "| %-34.34s |", line);
    drawStringAbs(x, y + 8, msg, props);
    drawStringAbs(x, y + 9,  "+------------------------------------+", props);
    props.invert_ = false;

    SetColor(CD_NORMAL);
    drawStringAbs(1, 24, "UD item LR value    B preview", props);
    drawStringAbs(1, 25, "A apply B preview  L1+X undo", props);
    drawStringAbs(1, 26, "R1+X redo  L1+R1 exit R2+LR", props);
}

void SampleChopperModal::applyEnvelopeToBuffer(short *samples, int frames, int channels, int rate, int attackMs, int sustainPercent, int releaseMs) {
    if (!samples || frames <= 0 || channels <= 0 || rate <= 0) return;
    attackMs = clampInt(attackMs, 0, 5000);
    releaseMs = clampInt(releaseMs, 0, 5000);
    sustainPercent = clampInt(sustainPercent, 0, 150);

    int attackFrames = (int)(((long long)attackMs * (long long)rate) / 1000LL);
    int releaseFrames = (int)(((long long)releaseMs * (long long)rate) / 1000LL);
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
            double rel = (remain <= 0) ? 0.0 : ((double)remain / (double)releaseFrames);
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

    startFrame = clampInt(startFrame, 0, size - 1);
    endFrame = clampInt(endFrame, startFrame, size - 1);
    int rangeFrames = endFrame - startFrame + 1;
    if (rangeFrames <= 1) return false;

    double ratio = pow(2.0, ((double)semitones) / 12.0);
    if (ratio <= 0.0) return false;
    int nextSize = (int)(((double)rangeFrames / ratio) + 0.5);
    if (nextSize < 2) nextSize = 2;
    if (nextSize > 40000000) return false;
    short *pitched = (short *)malloc(nextSize * channels * sizeof(short));
    if (!pitched) return false;

    for (int i = 0; i < nextSize; i++) {
        double srcPos = (double)i * ratio;
        int idx = (int)srcPos;
        double frac = srcPos - (double)idx;
        if (idx < 0) idx = 0;
        if (idx >= rangeFrames - 1) { idx = rangeFrames - 1; frac = 0.0; }
        int idx2 = idx + 1;
        if (idx2 >= rangeFrames) idx2 = rangeFrames - 1;
        int srcIdx = startFrame + idx;
        int srcIdx2 = startFrame + idx2;
        for (int ch = 0; ch < channels; ch++) {
            int a = samples[srcIdx * channels + ch];
            int b = samples[srcIdx2 * channels + ch];
            int v = (int)((double)a + ((double)(b - a) * frac));
            if (v < -32768) v = -32768;
            if (v > 32767) v = 32767;
            pitched[i * channels + ch] = (short)v;
        }
    }

    applyEnvelopeToBuffer(pitched, nextSize, channels, rate, pitchAttackMs_, pitchSustainPercent_, pitchReleaseMs_);

    if (outSamples) *outSamples = pitched;
    if (outFrames) *outFrames = nextSize;
    if (outChannels) *outChannels = channels;
    if (outRate) *outRate = rate;
    return true;
}

bool SampleChopperModal::buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate) {
    return buildPitchEnvelopeBufferFromRange(0, sourceSize_ > 0 ? sourceSize_ - 1 : 0, semitones, outSamples, outFrames, outChannels, outRate);
}

bool SampleChopperModal::preparePitchEnvelopePreviewBuffer(short **outSamples, int *outFrames, int *outChannels, int *outRate) {
    if (!pitchMode_) return false;
    if (pitchScope_) {
        initializeChopsIfNeeded();
        if (!hasActiveSliceRange()) return false;
        return buildPitchEnvelopeBufferFromRange(selectedChopStartFrame(), selectedChopEndFrame(), pitchSemitones_, outSamples, outFrames, outChannels, outRate);
    }
    return buildPitchedBuffer(pitchSemitones_, outSamples, outFrames, outChannels, outRate);
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
        if (pitchScope_) playSelectedChop();
        else playFullSample();
        setStatus(pitchScope_ ? "Preview chop unchanged" : "Preview unchanged");
        return;
    }

    showOperationProgress("Build preview", 5);
    short *pitched = 0;
    int frames = 0, channels = 0, rate = 0;
    if (!preparePitchEnvelopePreviewBuffer(&pitched, &frames, &channels, &rate)) { clearOperationProgress(); setStatus("Pitch preview fail"); return; }
    showOperationProgress("Build preview", 65);
    std::string logical;
    bool ok = writePreviewPitchWav(pitched, frames, channels, rate, logical);
    free(pitched);
    if (!ok) { clearOperationProgress(); setStatus("Pitch preview write fail"); return; }
    showOperationProgress("Build preview", 90);
    Path path(logical.c_str());
    Player::GetInstance()->StopStreaming();
    TimeService::GetInstance()->Sleep(80);
    Player::GetInstance()->StartStreamingRangeAt(path, 0, frames > 0 ? frames - 1 : 0);
    playbackTriggered_ = true;
    previewActive_ = false;
#if defined(PLATFORM_TREEFROG)
    g_chopperPreviewActive = 0;
#endif
    clearOperationProgress();
    char msg[64]; snprintf(msg, sizeof(msg), "Preview %s P%+d A%d S%d R%d", pitchScope_ ? "Chop" : "Sample", pitchSemitones_, pitchAttackMs_, pitchSustainPercent_, pitchReleaseMs_);
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
    if (pitchScope_) {
        if (!hasActiveSliceRange()) { setStatus("No chop selected"); return false; }
        editStart = selectedChopStartFrame();
        editEnd = selectedChopEndFrame();
        editStart = clampInt(editStart, 0, size - 1);
        editEnd = clampInt(editEnd, editStart, size - 1);
    }
    int originalRangeFrames = editEnd - editStart + 1;
    if (originalRangeFrames <= 1) { setStatus("Range too small"); return false; }

    stopSamplePreview();
    char label[56]; snprintf(label, sizeof(label), "Pitch/env %s P%+d", pitchScope_ ? "chop" : "sample", semitones);
    showOperationProgress(label, 0);
    clearLogicalHistory();
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, pitchScope_ ? "PitchEnvChop" : "PitchEnv", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
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
    int oldCount = boundaryCount_;
    int oldSelected = selectedChop_;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) oldBoundaries[i] = boundaries_[i];

    if (pitchScope_) {
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
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }
    showOperationProgress(label, 90);

    int nextBoundaries[MAX_CHOP_BOUNDARIES];
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) nextBoundaries[i] = 0;
    int out = 0;
    if (!pitchScope_) {
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
    boundaryCount_ = out;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = (i < boundaryCount_) ? nextBoundaries[i] : 0;

    sourceSize_ = nextSize;
    sampleSize_ = nextSize;
    selectedChop_ = clampInt(oldSelected, 0, boundaryCount_ - 2);
    cursorFrame_ = boundaries_[selectedChop_];
    viewStartFrame_ = 0;
    chopsInitialized_ = true;
    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    refreshCurrentInstrumentAfterSampleEdit(sourceSize_);
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    showOperationProgress(pitchScope_ ? "Pitch chop complete" : "Pitch sample complete", 100);
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
    g_chopperOverlayActive = (hasWaveform_ && !operationActive_ && !pitchMode_) ? 1 : 0;
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
    if (hasActiveSliceRange() && selectedChop_ >= 0 && selectedChop_ <= boundaryCount_ - 2) {
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
void SampleChopperModal::drawStringAbs(int x, int y, const char *txt, GUITextProperties &props) { View::DrawString(x, y, txt, props); }
void SampleChopperModal::clearTextScreen() { View::ClearRect(0, 0, SCREEN_W, SCREEN_H); }
void SampleChopperModal::drawTopBar(GUITextProperties &props) { props.invert_ = true; SetColor(CD_HILITE1); drawStringAbs(0, 0, " P G  SCPI  M TT       CHOPPER       ", props); props.invert_ = false; }

void SampleChopperModal::drawFrame(GUITextProperties &props) {
    SetColor(CD_BORDER);
    drawStringAbs(0, 1,  "+--------------------------------------+", props);
    for (int y = 2; y < 22; y++) { drawStringAbs(0, y, "|", props); drawStringAbs(39, y, "|", props); }
    drawStringAbs(0, 22, "+--------------------------------------+", props);
    SetColor(CD_HILITE2);
    drawStringAbs(2, 2, "Graphical Chopper U2.36", props);
}

void SampleChopperModal::drawSampleInfo(GUITextProperties &props) {
    char buffer[96];
    SetColor(CD_NORMAL);
    snprintf(buffer, sizeof(buffer), "Inst:%02X Smpl:%02X Zoom:%03d%%", instrumentIndex_, sampleIndex_ < 0 ? 0 : sampleIndex_, zoomPercent_);
    buffer[37] = 0; drawStringAbs(2, 4, buffer, props);
    if (!hasAssignedSample()) { drawStringAbs(2, 5, "No sample assigned", props); drawStringAbs(2, 6, "No chop actions", props); }
    else { std::string name = sampleName_; if (name.size() > 31) name = name.substr(0, 31); snprintf(buffer, sizeof(buffer), "Name:%s", name.c_str()); buffer[37] = 0; drawStringAbs(2, 5, buffer, props); snprintf(buffer, sizeof(buffer), "Frame:%d/%d Chop:%02d/%02d%s", cursorFrame_, sourceSize_ > 0 ? sourceSize_ - 1 : 0, hasActiveSliceRange() ? selectedChop_ : 0, hasActiveSliceRange() ? (boundaryCount_ - 1) : 0, trimMode_ ? " ADJ" : ""); buffer[37] = 0; drawStringAbs(2, 6, buffer, props); }
    if (statusMessage_[0]) { SetColor(CD_HILITE1); drawStringAbs(2, 21, statusMessage_, props); }
}

void SampleChopperModal::drawEmptyWaveformText(GUITextProperties &props) { SetColor(CD_HILITE1); drawStringAbs(2, 13, "----------- no sample loaded -----------", props); }

void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    if (pitchMode_) {
        /* U2.27: the compact Pitch/Env panel carries its own controls.
           Avoid duplicating help lines at the bottom of the 320x240 screen. */
        return;
    }
    drawStringAbs(0, 24, "R1+LR sample  L1+LR fast cursor", props);
    drawStringAbs(0, 25, "A cut/live Y del B play SELECT crop", props);
    drawStringAbs(0, 26, trimMode_ ? "R1+A keep L2+Y del L1+X undo" : "R2+LR chop  R2+A full", props);
    drawStringAbs(0, 27, "L1+X undo  R1+X redo", props);
    SetColor(CD_HILITE1);
    drawStringAbs(0, 28, trimMode_ ? "CROP A/B range  Y start X end1s" : "SELECT crop L1+R1 pitch R1+B back", props);
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
    if (operationPercent_ >= 100) snprintf(status, sizeof(status), "%s OK A/L1+X/R1+X", operationMessage_);
    else snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);
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
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 0;
    g_chopperOperationPercent = 0;
#endif
    isDirty_ = true;
}

void SampleChopperModal::drawOperationOverlay(GUITextProperties &props) {
    if (!operationActive_) return;
    char msg[64];
    const int x = 3;
    const int y = 10;
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(x, y + 0, "+--------------------------------+", props);
    snprintf(msg, sizeof(msg), "| %-30.30s |", operationMessage_);
    drawStringAbs(x, y + 1, msg, props);
    snprintf(msg, sizeof(msg), "|              %3d%%              |", operationPercent_);
    msg[34] = 0;
    drawStringAbs(x, y + 2, msg, props);
    if (operationPercent_ >= 100) {
        drawStringAbs(x, y + 3, "|              OK                |", props);
        drawStringAbs(x, y + 4, "| A close  L1+X undo R1+X redo  |", props);
    } else {
        drawStringAbs(x, y + 3, "|          Please wait           |", props);
        drawStringAbs(x, y + 4, "|       Processing sample        |", props);
    }
    drawStringAbs(x, y + 5, "+--------------------------------+", props);
    props.invert_ = false;
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

    bool left = (mask & EPBM_LEFT) != 0;
    bool right = (mask & EPBM_RIGHT) != 0;
    bool up = (mask & EPBM_UP) != 0;
    bool down = (mask & EPBM_DOWN) != 0;
    bool l1 = (mask & EPBM_L) != 0;
    bool r1 = (mask & EPBM_R) != 0;
    bool l2 = (mask & EPBM_L2) != 0;
    bool r2 = (mask & EPBM_R2) != 0;
    bool y = (mask & EPBM_Y) != 0;
    bool x = (mask & EPBM_X) != 0;
    bool select = (mask & EPBM_SELECT) != 0;
    bool a = (mask & EPBM_A) != 0;
    bool b = (mask & EPBM_B) != 0;

    const bool undoChord =
        l1 && x &&
        !(left || right || up || down || a || b || y || r1 || l2 || r2 || select);
    const bool redoChord =
        r1 && x &&
        !(left || right || up || down || a || b || y || l1 || l2 || r2 || select);

    /* U2.51.0: completion overlays must not swallow undo/redo. */
    if (operationActive_ && operationPercent_ >= 100) {
        if (undoChord || redoChord) {
            clearOperationProgress();
            redoChord ? redoLastChopperEdit() : undoLastChopperEdit();
            return;
        }
        bool plainA = a && !(left || right || up || down || l1 || r1 || l2 || r2 || b || x || y || select);
        if (plainA) { clearOperationProgress(); DrawView(); publishOverlayState(); }
        return;
    }

    if (undoChord) { undoLastChopperEdit(); return; }
    if (redoChord) { redoLastChopperEdit(); return; }

    if (l1 && r1 && !(left || right || up || down || a || b || x || y || l2 || r2 || select)) {
        togglePitchMode(); return;
    }

    if (pitchMode_) {
        if (l2 && b && !(left || right || up || down || a || x || y || l1 || r1 || r2 || select)) {
            stopSamplePreview(); setStatus("Stop preview"); return;
        }
        if (r2 && (left || right) && !(up || down || a || b || x || y || l1 || r1 || l2 || select)) {
            if (pitchScope_) {
                selectChop(right ? 1 : -1);
                char msg[64]; snprintf(msg, sizeof(msg), "Pitch chop %02d/%02d", selectedChop_ + 1, (boundaryCount_ > 1 ? boundaryCount_ - 1 : 1));
                setStatus(msg);
            } else {
                setStatus("Set Scope Chop first");
            }
            return;
        }
        if ((up || down) && !(left || right || a || b || x || y || l1 || r1 || l2 || r2 || select)) {
            selectPitchEditParam(down ? 1 : -1);
            return;
        }
        if ((left || right) && !(up || down || a || b || x || y || l1 || r1 || l2 || r2 || select)) {
            nudgePitchEnvelopeValue(right ? 1 : -1);
            return;
        }
        if (b && !(left || right || up || down || a || x || y || l1 || r1 || l2 || r2 || select)) {
            previewPitchSetting(); return;
        }
        if (a && !(left || right || up || down || b || x || y || l1 || r1 || l2 || r2 || select)) {
            destructivePitchSample(pitchSemitones_); return;
        }
        if (select && !(left || right || up || down || a || b || x || y || l1 || r1 || l2 || r2)) {
            setStatus("Pitch/env: L1+R1 exit"); return;
        }
        setStatus("Pitch/env: UD item LR value");
        return;
    }

    if (r1 && b && !(left || right || up || down || a || x || y)) {
        stopSamplePreview(); EndModal(0); isDirty_ = true; return;
    }

    if (l2 && b && !(left || right || up || down || a || x || y)) {
        stopSamplePreview(); setStatus("Stop playback"); return;
    }

    if (select && !(left || right || up || down || a || b || x || y || l1 || r1 || l2 || r2)) {
        toggleTrimMode(); return;
    }

    if (trimMode_) {
        if (r1 && a && !(left || right || up || down || b || x || y || l2 || r2)) {
            destructiveCropToSelectedRange(); return;
        }
        if (l2 && y && !(left || right || up || down || a || b || x || r1 || r2)) {
            destructiveDeleteSelectedRange(); return;
        }
        if ((left || right) && (a || b) && !(r1 || r2 || l2 || x || y)) {
            int delta = getFrameStepForEdit();
            if (!right) delta = -delta;
            if (l1) delta *= 10;
            if (a) nudgeSelectedStart(delta); else if (b) nudgeSelectedEnd(delta);
            return;
        }
        if (y && !x && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down || a || b)) {
            previewTrimStart(); return;
        }
        if (x && !y && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down || a || b)) {
            previewTrimEnd(); return;
        }
        if ((a || b) && !(left || right)) {
            setStatus(a ? "Crop: A+LEFT/RIGHT" : "Crop: B+LEFT/RIGHT"); return;
        }
        if (l2 && y) { setStatus("Delete: L2+Y"); return; }
    }

    if (r1 && a) {
        saveChopStateForCurrentSample();
        setStatus("Auto-save on: assign Sxx in Phrase");
        return;
    }
    if (!trimMode_ && r1 && (left || right)) {
        selectSample(right ? 1 : -1); return;
    }
    if (r2 && a) { playFullSample(); return; }
    if (r2 && (left || right)) { selectChop(right ? 1 : -1); return; }

    if (!trimMode_) {
        if (y && !l1 && !r1 && !l2 && !r2) { deleteSelectedChop(); return; }
        if (b && !l1 && !r1 && !l2 && !r2) { playSelectedChop(); return; }
        if (a && !l1 && !r1 && !l2 && !r2) { addChopAtCursor(); return; }
    }
    if (left || right) {
        int deltaPx = right ? 2 : -2;
        if (l1) deltaPx = right ? 24 : -24;
        nudgeCursorPixels(deltaPx); return;
    }
    if (up || down) {
        int deltaPercent = up ? 5 : -5;
        if (l1) deltaPercent = up ? 10 : -10;
        nudgeZoomPercent(deltaPercent); return;
    }
}

