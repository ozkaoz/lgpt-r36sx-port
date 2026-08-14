/*
 * chopper_controller_host_test.cpp -- F3-3c: host test de
 * ChopperController.h (capa pura).  FakeHost registra los efectos
 * (status/undo/combo/orden de escrituras) y el test valida con oraculos
 * golden derivados de Bacon 1.2.1 (SampleChopperModal.cpp): mensajes
 * exactos, estado final de boundaries/selected/cursor/trim/split, y el
 * orden de las escrituras de cada flujo.
 *
 * Compilar (ASAN/UBSAN, gnu++03):
 *   g++ -std=gnu++03 -Wall -Wextra -fsanitize=address,undefined \
 *       -Isource/sources chopper_controller_host_test.cpp -o chopper_controller_host_test
 *   ./chopper_controller_host_test   ->  ALL OK (N checks)
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Application/UI/Views/ModalDialogs/ChopperController.h"

static int g_checks = 0;
static int g_fails = 0;
static const char *g_lastScenario = "";

static void CHECK(bool cond, const char *what) {
    g_checks++;
    if (!cond) {
        g_fails++;
        std::printf("FAIL [%s]: %s\n", g_lastScenario, what);
    }
}

static void CHECK_EQ_INT(int got, int want, const char *what) {
    g_checks++;
    if (got != want) {
        g_fails++;
        std::printf("FAIL [%s]: %s: got %d want %d\n", g_lastScenario, what,
                    got, want);
    }
}

static void CHECK_EQ_STR(const std::string &got, const char *want,
                         const char *what) {
    g_checks++;
    if (got != want) {
        g_fails++;
        std::printf("FAIL [%s]: %s: got '%s' want '%s'\n", g_lastScenario,
                    what, got.c_str(), want);
    }
}

static void CHECK_EQ_VEC(const std::vector<std::string> &got,
                         const std::vector<std::string> &want,
                         const char *what) {
    g_checks++;
    bool ok = (got.size() == want.size());
    if (ok) {
        for (size_t i = 0; i < got.size(); i++)
            if (got[i] != want[i]) { ok = false; break; }
    }
    if (!ok) {
        g_fails++;
        std::printf("FAIL [%s]: %s: [", g_lastScenario, what);
        for (size_t i = 0; i < got.size(); i++) std::printf("'%s', ", got[i].c_str());
        std::printf("] want [");
        for (size_t i = 0; i < want.size(); i++) std::printf("'%s', ", want[i].c_str());
        std::printf("]\n");
    }
}

static void CHECK_BOUNDARIES(const ChopModel &m, const int *want, int n,
                             const char *what) {
    g_checks++;
    bool ok = (m.boundaryCount == n);
    for (int i = 0; ok && i < n; i++) ok = (m.boundaries[i] == want[i]);
    if (!ok) {
        g_fails++;
        std::printf("FAIL [%s]: %s: count %d [", g_lastScenario, what,
                    m.boundaryCount);
        for (int i = 0; i < m.boundaryCount; i++)
            std::printf("%d%s", m.boundaries[i],
                        i + 1 < m.boundaryCount ? "," : "");
        std::printf("] want %d [", n);
        for (int i = 0; i < n; i++)
            std::printf("%d%s", want[i], i + 1 < n ? "," : "");
        std::printf("]\n");
    }
}

static std::vector<std::string> mkst(const char *a) {
    std::vector<std::string> v;
    if (a) v.push_back(a);
    return v;
}

static std::vector<std::string> mkst(const char *a, const char *b) {
    std::vector<std::string> v;
    v.push_back(a);
    v.push_back(b);
    return v;
}

/* ------------------------------------------------------------------ */

struct FakeHost : ChopperController::Host {
    std::vector<std::string> log;
    std::vector<std::string> statuses;

    bool sampleLoaded;
    int snapCode;        /* 0 ok, 1 sin source, 2 buffer malo */
    short *snapSamples;
    int snapChannels;
    bool streamingActive;
    int nextStreamPos;
    bool hasStreamPos;

    FakeHost()
        : sampleLoaded(false),
          snapCode(0),
          snapSamples(0),
          snapChannels(1),
          streamingActive(false),
          nextStreamPos(0),
          hasStreamPos(false) {}

    void SetStatus(const char *message) {
        statuses.push_back(std::string("status:") + (message ? message : ""));
        log.push_back(std::string("status:") + (message ? message : ""));
    }
    void SetOperationCombo(const char *combo) {
        log.push_back(std::string("combo:") + combo);
    }
    void PushLogicalUndo(const char *action) {
        log.push_back(std::string("undo:") + action);
    }
    void SaveChopState() { log.push_back("save"); }
    void EnsureCursorVisible() { log.push_back("ensure"); }
    void PrepareWaveformPreview() { log.push_back("prep"); }
    void PublishOverlayState() { log.push_back("publish"); }
    void MarkDirty() { log.push_back("dirty"); }
    bool SampleLoaded() { return sampleLoaded; }
    int QuerySnapBuffer(short **samples, int *channels) {
        log.push_back("querySnap");
        *samples = snapSamples;
        *channels = snapChannels;
        return snapCode;
    }
    bool LiveStreamingPosition(int *frame) {
        log.push_back("queryLive");
        if (!streamingActive) return false;
        if (hasStreamPos) *frame = nextStreamPos;
        return true;
    }
};

struct Ctx {
    FakeHost host;
    ChopModel model;
    PreviewService preview;
    int sourceSize;
    int cursorFrame;
    bool trimMode;
    bool pitchMode;
    int splitParts;
    bool chopsInitialized;
    ChopperController cc;

    Ctx(int sourceSizeVal, int splitPartsVal)
        : sourceSize(sourceSizeVal),
          cursorFrame(0),
          trimMode(false),
          pitchMode(false),
          splitParts(splitPartsVal),
          chopsInitialized(false),
          cc(host, model, preview, sourceSize, cursorFrame, trimMode,
             pitchMode, splitParts, chopsInitialized) {}
};

static void scenario(const char *name) { g_lastScenario = name; }

static short *MakeFlatBuffer(int frames, int value, short *mem) {
    for (int i = 0; i < frames; i++) mem[i] = (short)value;
    return mem;
}

/* ------------------------------------------------------------------ */

static void TestInitialize() {
    scenario("InitializeChopsIfNeeded");
    {
        Ctx c(44100, 4);
        c.cc.InitializeChopsIfNeeded();
        CHECK(c.chopsInitialized, "flag on");
        int want[2] = {0, 44100 - 1};
        CHECK_BOUNDARIES(c.model, want, 2, "range");
        CHECK_EQ_INT(c.model.selected, 0, "selected");
        CHECK_EQ_INT((int)c.host.statuses.size(), 0, "no status");
        c.cc.InitializeChopsIfNeeded(); /* no-op */
        CHECK_EQ_INT(c.model.boundaryCount, 2, "no-op keeps count");
    }
    scenario("InitializeChopsIfNeeded tiny");
    {
        Ctx c(1, 4);
        c.cc.InitializeChopsIfNeeded();
        CHECK(!c.chopsInitialized, "skipped at size<=1");
        CHECK_EQ_INT(c.model.boundaryCount, 0, "model untouched");
    }
}

/* ------------------------------------------------------------------ */

static void TestAddChopAtCursor() {
    scenario("AddChopAtCursor no sample");
    {
        Ctx c(1, 4);
        c.cc.AddChopAtCursor();
        CHECK_EQ_VEC(c.host.statuses, mkst("status:No sample to chop"),
                     "no sample msg");
        CHECK_EQ_INT(c.model.boundaryCount, 0, "untouched");
    }
    scenario("AddChopAtCursor success");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 22050;
        c.cc.AddChopAtCursor();
        int want[3] = {0, 22050, 44100 - 1};
        CHECK_BOUNDARIES(c.model, want, 3, "sorted append");
        CHECK_EQ_INT(c.model.selected, 0, "selected = idx-1");
        std::vector<std::string> wantLog;
        wantLog.push_back("undo:Add cut");
        wantLog.push_back("save");
        wantLog.push_back("publish");
        wantLog.push_back("status:Chop 00 at 22050");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order");
    }
    scenario("AddChopAtCursor edge");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 0;
        c.cc.AddChopAtCursor();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Cannot chop at edge",
                     "edge 0");
        c.host.statuses.clear();
        c.cursorFrame = 44100 - 1;
        c.cc.AddChopAtCursor();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Cannot chop at edge",
                     "edge size-1");
    }
    scenario("AddChopAtCursor duplicate");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 100;
        c.cc.AddChopAtCursor();
        c.host.statuses.clear();
        c.cursorFrame = 100;
        c.cc.AddChopAtCursor();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Chop already exists",
                     "exact dup");
        c.host.statuses.clear();
        c.cursorFrame = 101; /* abs diff <= 1 */
        c.cc.AddChopAtCursor();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Chop already exists",
                     "diff 1 dup");
        CHECK_EQ_INT(c.model.boundaryCount, 3, "no growth");
    }
    scenario("AddChopAtCursor max 100");
    {
        Ctx c(1000000, 4);
        for (int i = 1; i <= 100; i++) { /* 100 adds + init range = 102 */
            c.cursorFrame = i * 6000;
            c.cc.AddChopAtCursor();
        }
        CHECK_EQ_INT(c.model.boundaryCount, ChopModel::kMaxBoundaries,
                     "full at 101");
        c.host.statuses.clear();
        c.cursorFrame = 500;
        c.cc.AddChopAtCursor();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Max 100 chops reached",
                     "max msg");
        CHECK_EQ_INT(c.model.boundaryCount, ChopModel::kMaxBoundaries,
                     "no growth beyond");
    }
    scenario("AddChopAtCursor live cut");
    {
        Ctx c(44100, 4);
        c.preview.SetRange(1000, 2000, c.sourceSize);
        c.host.streamingActive = true;
        c.host.hasStreamPos = true;
        c.host.nextStreamPos = 1500;
        c.cursorFrame = 0;
        c.cc.AddChopAtCursor();
        CHECK_EQ_INT(c.cursorFrame, 1500, "cursor overwritten (in range)");
        CHECK_EQ_INT(c.model.boundaries[1], 1500, "live boundary");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Live chop 00 at 1500",
                     "live msg");
        c.host.statuses.clear();
        c.host.nextStreamPos = 5000; /* fuera del rango */
        c.cursorFrame = 123;
        c.cc.AddChopAtCursor();
        CHECK_EQ_INT(c.cursorFrame, 123, "cursor kept (out of range)");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Chop 00 at 123",
                     "regular msg");
        c.host.streamingActive = false;
        c.host.statuses.clear();
        c.cursorFrame = 456;
        c.cc.AddChopAtCursor();
        /* tras 3 add: 0,123,456,1500,size-1; idx(456)=2 -> selected=1 */
        CHECK_EQ_STR(c.host.statuses.back(), "status:Chop 01 at 456",
                     "not streaming");
    }
}

/* ------------------------------------------------------------------ */

static void TestDeleteSelectedChop() {
    scenario("DeleteSelectedChop no chops");
    {
        Ctx c(44100, 4);
        c.cc.DeleteSelectedChop();
        CHECK_EQ_STR(c.host.statuses.back(), "status:No chop to delete",
                     "msg");
    }
    scenario("DeleteSelectedChop middle");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 100;
        c.cc.AddChopAtCursor();
        c.cursorFrame = 300; /* boundaries 0,100,300,size-1 */
        c.cc.AddChopAtCursor();
        c.model.selected = 1;
        c.host.log.clear();
        c.cc.DeleteSelectedChop();
        int want[3] = {0, 300, 44100 - 1};
        CHECK_BOUNDARIES(c.model, want, 3, "shift remove");
        CHECK(!c.trimMode, "trim off");
        /* seleccion 1 tras el remove; cursor = inicio del chop seleccionado */
        CHECK_EQ_INT(c.cursorFrame, 300, "cursor at start of selected");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Deleted cut", "msg");
        std::vector<std::string> wantLog;
        wantLog.push_back("undo:Merge cuts");
        wantLog.push_back("save");
        wantLog.push_back("ensure");
        wantLog.push_back("prep");
        wantLog.push_back("publish");
        wantLog.push_back("status:Deleted cut");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order");
    }
    scenario("DeleteSelectedChop selected 0");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 50;
        c.cc.AddChopAtCursor();
        c.cursorFrame = 200;
        c.cc.AddChopAtCursor();
        c.model.selected = 0;
        c.cc.DeleteSelectedChop();
        int want[3] = {0, 200, 44100 - 1};
        CHECK_BOUNDARIES(c.model, want, 3, "removeIdx 1");
    }
    scenario("DeleteSelectedChop edge guard");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 50;
        c.cc.AddChopAtCursor();
        c.cursorFrame = 200;
        c.cc.AddChopAtCursor();
        c.model.selected = c.model.boundaryCount - 1;
        c.cc.DeleteSelectedChop();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Cannot delete edge",
                     "edge guard");
        CHECK_EQ_INT(c.model.boundaryCount, 4, "untouched");
    }
}

/* ------------------------------------------------------------------ */

static void TestSelectChop() {
    scenario("SelectChop no user chops");
    {
        Ctx c(44100, 4);
        c.cc.SelectChop(1);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No user chops", "msg");
        CHECK_EQ_INT(c.model.selected, 0, "selected untouched");
    }
    scenario("SelectChop cycle");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 100;
        c.cc.AddChopAtCursor();
        c.cursorFrame = 300;
        c.cc.AddChopAtCursor(); /* 0,100,300,size-1; la 2a add deja selected=1 */
        c.cc.SelectChop(1);
        CHECK_EQ_INT(c.model.selected, 2, "next chop");
        CHECK_EQ_INT(c.cursorFrame, 300, "cursor at chop start");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Selected chop 02",
                     "msg");
        c.cc.SelectChop(99); /* clamp */
        CHECK_EQ_INT(c.model.selected, 2, "clamped high");
        c.cc.SelectChop(-99);
        CHECK_EQ_INT(c.model.selected, 0, "clamped low");
    }
}

/* ------------------------------------------------------------------ */

static void TestToggleTrimMode() {
    scenario("ToggleTrimMode enter/leave");
    {
        Ctx c(44100, 4);
        c.pitchMode = true;
        c.cc.ToggleTrimMode();
        CHECK(c.trimMode, "trim on");
        CHECK(!c.pitchMode, "pitch cleared");
        CHECK_EQ_INT(c.cursorFrame, 0, "cursor at selected start");
        CHECK_EQ_STR(c.host.statuses.back(), "status:CROP SAMPLE", "enter msg");
        CHECK_EQ_STR(c.host.log.back(), "dirty", "MarkDirty last");
        c.cc.ToggleTrimMode();
        CHECK(!c.trimMode, "trim off");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Crop mode off",
                     "leave msg");
    }
}

/* ------------------------------------------------------------------ */

static void TestNudgeSelected() {
    scenario("NudgeSelectedStart no range");
    {
        Ctx c(44100, 4);
        c.chopsInitialized = true; /* initialize ya no revive el rango */
        c.model.boundaryCount = 1; /* < 2 */
        c.model.boundaries[0] = 0;
        c.cc.NudgeSelectedStart(10);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No range to trim",
                     "msg");
        c.cc.NudgeSelectedEnd(10);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No range to trim",
                     "end msg");
    }
    scenario("NudgeSelectedStart move");
    {
        Ctx c(44100, 4);
        c.model.boundaryCount = 2;
        c.model.boundaries[0] = 0;
        c.model.boundaries[1] = 1000;
        c.model.selected = 0;
        c.chopsInitialized = true;
        c.host.log.clear();
        c.cc.NudgeSelectedStart(15);
        CHECK_EQ_INT(c.model.boundaries[0], 15, "moved");
        CHECK_EQ_INT(c.cursorFrame, 15, "cursor");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Adjusted chop start",
                     "msg");
        std::vector<std::string> wantLog;
        wantLog.push_back("undo:Move cut start");
        wantLog.push_back("save");
        wantLog.push_back("ensure");
        wantLog.push_back("prep");
        wantLog.push_back("publish");
        wantLog.push_back("status:Adjusted chop start");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order");
    }
    scenario("NudgeSelectedStart clamp/no-op");
    {
        Ctx c(44100, 4);
        c.model.boundaryCount = 2;
        c.model.boundaries[0] = 15;
        c.model.boundaries[1] = 1000;
        c.model.selected = 0;
        c.chopsInitialized = true;
        c.cc.NudgeSelectedStart(-100);
        CHECK_EQ_INT(c.model.boundaries[0], 0, "clamped to min");
        c.host.statuses.clear();
        c.host.log.clear();
        c.cc.NudgeSelectedStart(0);
        CHECK_EQ_INT(c.model.boundaries[0], 0, "unchanged");
        CHECK_EQ_INT((int)c.host.statuses.size(), 0, "no status on no-op");
        CHECK_EQ_INT((int)c.host.log.size(), 0, "no writes on no-op");
    }
    scenario("NudgeSelectedEnd move/clamp");
    {
        Ctx c(44100, 4);
        c.model.boundaryCount = 2;
        c.model.boundaries[0] = 0;
        c.model.boundaries[1] = 1000;
        c.model.selected = 0;
        c.chopsInitialized = true;
        c.cc.NudgeSelectedEnd(50);
        CHECK_EQ_INT(c.model.boundaries[1], 1050, "end moved");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Adjusted chop end",
                     "msg");
        c.cc.NudgeSelectedEnd(100000);
        CHECK_EQ_INT(c.model.boundaries[1], 44100 - 1, "end clamped to size-1");
        c.host.log.clear();
        c.cc.NudgeSelectedEnd(-5000);
        /* 44099-5000 = 39099 > minFrame 1 -> se mueve, no clamp */
        CHECK_EQ_INT(c.model.boundaries[1], 39099, "end lower move");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Adjusted chop end",
                     "lower clamp msg");
    }
}

/* ------------------------------------------------------------------ */

static void TestCropToSelectedRange() {
    scenario("CropToSelectedRange no sample");
    {
        Ctx c(1, 4);
        c.cc.CropToSelectedRange();
        CHECK_EQ_STR(c.host.statuses.back(), "status:No sample to crop",
                     "msg");
    }
    scenario("CropToSelectedRange no range");
    {
        Ctx c(44100, 4);
        c.chopsInitialized = true;
        c.model.boundaryCount = 1;
        c.cc.CropToSelectedRange();
        CHECK_EQ_STR(c.host.statuses.back(), "status:No range to crop",
                     "msg");
    }
    scenario("CropToSelectedRange bad range");
    {
        Ctx c(44100, 4);
        c.chopsInitialized = true;
        c.model.boundaryCount = 2;
        c.model.boundaries[0] = 100;
        c.model.boundaries[1] = 50;
        c.model.selected = 0;
        c.cc.CropToSelectedRange();
        CHECK_EQ_STR(c.host.statuses.back(), "status:Bad crop range", "msg");
    }
    scenario("CropToSelectedRange success");
    {
        Ctx c(44100, 4);
        c.model.boundaryCount = 4;
        int b[4] = {0, 100, 300, 44100 - 1};
        memcpy(c.model.boundaries, b, sizeof(b));
        c.model.selected = 1;
        c.chopsInitialized = true;
        c.host.log.clear();
        c.cc.CropToSelectedRange();
        int want[2] = {100, 300};
        CHECK_BOUNDARIES(c.model, want, 2, "kept range");
        CHECK_EQ_INT(c.model.selected, 0, "selected reset");
        CHECK(!c.trimMode, "trim off");
        CHECK_EQ_INT(c.cursorFrame, 100, "cursor at start");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Keep range 100-300",
                     "msg");
        std::vector<std::string> wantLog;
        wantLog.push_back("undo:Keep logical range");
        wantLog.push_back("save");
        wantLog.push_back("ensure");
        wantLog.push_back("prep");
        wantLog.push_back("publish");
        wantLog.push_back("status:Keep range 100-300");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order");
    }
}

/* ------------------------------------------------------------------ */

static void TestSplitSampleIntoEqualParts() {
    scenario("SplitSampleIntoEqualParts no sample");
    {
        Ctx c(1, 4);
        c.cc.SplitSampleIntoEqualParts(4);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No sample to split",
                     "msg");
    }
    scenario("SplitSampleIntoEqualParts too small");
    {
        Ctx c(3, 4);
        c.cc.SplitSampleIntoEqualParts(32); /* step 0 */
        CHECK_EQ_STR(c.host.statuses.back(), "status:Sample too small",
                     "msg");
        CHECK_EQ_STR(c.host.log[0], "combo:L1 + B", "combo before guard");
        CHECK_EQ_INT(c.model.boundaryCount, 2, "untouched");
    }
    scenario("SplitSampleIntoEqualParts parts clamp");
    {
        Ctx c(1000, 4);
        c.cc.SplitSampleIntoEqualParts(50); /* -> 4 */
        CHECK_EQ_STR(c.host.statuses.back(), "status:Split sample in 4 parts",
                     "clamped msg");
        CHECK_EQ_STR(c.host.log[0], "combo:L1 + B", "combo first");
    }
    scenario("SplitSampleIntoEqualParts success");
    {
        Ctx c(1000, 4);
        c.host.log.clear();
        c.cc.SplitSampleIntoEqualParts(8);
        int want[9] = {0, 125, 250, 375, 500, 625, 750, 875, 999};
        CHECK_BOUNDARIES(c.model, want, 9, "step grid");
        CHECK_EQ_INT(c.model.selected, 0, "selected reset");
        CHECK(!c.trimMode, "trim off");
        CHECK_EQ_INT(c.cursorFrame, 0, "cursor reset");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Split sample in 8 parts",
                     "msg");
        std::vector<std::string> wantLog;
        wantLog.push_back("combo:L1 + B");
        wantLog.push_back("undo:Split sample");
        wantLog.push_back("save");
        wantLog.push_back("ensure");
        wantLog.push_back("prep");
        wantLog.push_back("publish");
        wantLog.push_back("status:Split sample in 8 parts");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order (combo first)");
    }
    scenario("SplitSampleIntoEqualParts last override");
    {
        Ctx c(1000, 4);
        c.cc.SplitSampleIntoEqualParts(32); /* step 31 -> 31+1=32 pts + last */
        CHECK_EQ_INT(c.model.boundaryCount, 32 + 1, "grid incl last");
        CHECK_EQ_INT(c.model.boundaries[c.model.boundaryCount - 1], 999,
                     "last = size-1");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Split sample in 32 parts",
                     "msg");
    }
}

/* ------------------------------------------------------------------ */

static void TestClearAllChops() {
    scenario("ClearAllChops no sample");
    {
        Ctx c(1, 4);
        c.cc.ClearAllChops();
        CHECK_EQ_STR(c.host.statuses.back(), "status:No sample to clear",
                     "msg");
    }
    scenario("ClearAllChops success");
    {
        Ctx c(44100, 4);
        c.cursorFrame = 100;
        c.cc.AddChopAtCursor();
        c.cursorFrame = 300;
        c.cc.AddChopAtCursor();
        c.trimMode = true;
        c.host.log.clear();
        c.cc.ClearAllChops();
        int want[2] = {0, 44100 - 1};
        CHECK_BOUNDARIES(c.model, want, 2, "min range");
        CHECK(!c.trimMode, "trim off");
        CHECK_EQ_INT(c.cursorFrame, 0, "cursor reset");
        CHECK_EQ_STR(c.host.statuses.back(),
                     "status:No cuts (L1+B to split again)", "msg");
        std::vector<std::string> wantLog;
        wantLog.push_back("undo:Clear chops");
        wantLog.push_back("save");
        wantLog.push_back("ensure");
        wantLog.push_back("prep");
        wantLog.push_back("publish");
        wantLog.push_back("status:No cuts (L1+B to split again)");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order");
    }
}

/* ------------------------------------------------------------------ */

static void TestCycleSplitParts() {
    scenario("CycleSplitParts 4 -> 8");
    {
        Ctx c(1000, 4);
        c.cc.CycleSplitParts();
        CHECK_EQ_INT(c.splitParts, 8, "cycle next");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Split sample in 8 parts",
                     "msg");
    }
    scenario("CycleSplitParts 32 -> clear -> next");
    {
        Ctx c(1000, 32);
        c.cc.CycleSplitParts();
        CHECK_EQ_INT(c.splitParts, 0, "clear state");
        CHECK_EQ_STR(c.host.statuses.back(), "status:No cuts (L1+B to split again)",
                     "clear msg");
        /* 0 no esta en el ciclo golden -> next=1 -> 8 */
        c.cc.CycleSplitParts();
        CHECK_EQ_INT(c.splitParts, 8, "next from 0 is 8");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Split sample in 8 parts",
                     "back to 8");
    }
    scenario("CycleSplitParts unknown value");
    {
        Ctx c(1000, 0); /* fuera del ciclo: golden -> 8 */
        c.cc.CycleSplitParts();
        CHECK_EQ_INT(c.splitParts, 8, "unknown -> 8");
    }
}

/* ------------------------------------------------------------------ */

static void TestSnapSelectedBoundaryToZeroCross() {
    scenario("Snap no sample");
    {
        Ctx c(44100, 4);
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No sample loaded",
                     "msg");
    }
    scenario("Snap no chops");
    {
        Ctx c(44100, 4);
        c.host.sampleLoaded = true;
        c.chopsInitialized = true;
        c.model.boundaryCount = 1;
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No chops to snap",
                     "msg");
    }
    scenario("Snap no source / bad buffer");
    {
        Ctx c(44100, 4);
        c.host.sampleLoaded = true;
        c.host.snapCode = 1;
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        CHECK_EQ_STR(c.host.statuses.back(), "status:No WAV source", "src");
        c.host.snapCode = 2;
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        CHECK_EQ_STR(c.host.statuses.back(), "status:Bad sample buffer",
                     "buf");
    }
    scenario("Snap start moved");
    {
        Ctx c(44100, 4);
        c.host.sampleLoaded = true;
        c.chopsInitialized = true;
        c.model.boundaryCount = 3;
        int b[3] = {0, 500, 44100 - 1};
        memcpy(c.model.boundaries, b, sizeof(b));
        c.model.selected = 0;
        short mem[500];
        MakeFlatBuffer(500, 100, mem);
        mem[20] = 0; /* minimo absoluto en 20 */
        c.host.snapSamples = mem;
        c.host.snapChannels = 1;
        c.host.log.clear();
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        CHECK_EQ_INT(c.model.boundaries[0], 20, "snapped to min");
        CHECK_EQ_INT(c.cursorFrame, 20, "cursor");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Zero-cross start 20",
                     "msg");
        std::vector<std::string> wantLog;
        wantLog.push_back("querySnap");
        wantLog.push_back("undo:Snap start");
        wantLog.push_back("save");
        wantLog.push_back("ensure");
        wantLog.push_back("prep");
        wantLog.push_back("publish");
        wantLog.push_back("status:Zero-cross start 20");
        CHECK_EQ_VEC(c.host.log, wantLog, "write order");
    }
    scenario("Snap end moved");
    {
        Ctx c(44100, 4);
        c.host.sampleLoaded = true;
        c.chopsInitialized = true;
        c.model.boundaryCount = 3;
        int b[3] = {0, 100, 200};
        memcpy(c.model.boundaries, b, sizeof(b));
        c.model.selected = 0;
        short mem[201];
        MakeFlatBuffer(201, 100, mem);
        mem[150] = 0;
        c.host.snapSamples = mem;
        c.host.snapChannels = 1;
        c.cc.SnapSelectedBoundaryToZeroCross(false); /* idx 1 -> frame 100 */
        CHECK_EQ_INT(c.model.boundaries[1], 150, "end snapped");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Zero-cross end 150",
                     "msg");
    }
    scenario("Snap already at zero-cross");
    {
        Ctx c(44100, 4);
        c.host.sampleLoaded = true;
        c.chopsInitialized = true;
        c.model.boundaryCount = 2;
        int b[2] = {100, 44100 - 1};
        memcpy(c.model.boundaries, b, sizeof(b));
        c.model.selected = 0;
        short mem[44100];
        MakeFlatBuffer(44100, 100, mem);
        mem[100] = 0; /* minimo justo en el boundary */
        c.host.snapSamples = mem;
        c.host.snapChannels = 1;
        c.host.log.clear();
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        CHECK_EQ_INT(c.model.boundaries[0], 100, "unchanged");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Already at zero-cross",
                     "msg");
        CHECK_EQ_VEC(c.host.log, mkst("querySnap", "status:Already at zero-cross"),
                     "no undo on no-op");
    }
    scenario("Snap window clamped to neighbors");
    {
        Ctx c(44100, 4);
        c.host.sampleLoaded = true;
        c.chopsInitialized = true;
        c.model.boundaryCount = 3;
        int b[3] = {100, 200, 44100 - 1};
        memcpy(c.model.boundaries, b, sizeof(b));
        c.model.selected = 0;
        short mem[201];
        MakeFlatBuffer(201, 100, mem);
        mem[35] = 0; /* lo = 100-64 = 36: el minimo queda fuera de la ventana */
        c.host.snapSamples = mem;
        c.host.snapChannels = 1;
        c.cc.SnapSelectedBoundaryToZeroCross(true);
        /* best = primer frame de la ventana con score minimo (36) */
        CHECK_EQ_INT(c.model.boundaries[0], 36, "window lo 36");
    }
}

/* ------------------------------------------------------------------ */

static void TestHelperMethods() {
    scenario("HasActiveSliceRange");
    {
        Ctx c(44100, 4);
        c.chopsInitialized = true;
        c.model.boundaryCount = 2;
        c.model.boundaries[0] = 0;
        c.model.boundaries[1] = 44100 - 1;
        CHECK(!c.cc.HasActiveSliceRange(), "min range false");
        c.model.boundaries[0] = 5;
        CHECK(c.cc.HasActiveSliceRange(), "b0>0 true");
        c.model.boundaries[0] = 0;
        c.model.boundaries[1] = 44100 - 2;
        CHECK(c.cc.HasActiveSliceRange(), "b1<size-1 true");
        c.model.boundaryCount = 3;
        CHECK(c.cc.HasActiveSliceRange(), "any chop true");
        c.sourceSize = 1;
        CHECK(!c.cc.HasActiveSliceRange(), "tiny false");
    }
    scenario("HasUserChops");
    {
        Ctx c(44100, 4);
        c.chopsInitialized = true;
        c.model.boundaryCount = 2;
        CHECK(!c.cc.HasUserChops(), "min range false");
        c.model.boundaryCount = 3;
        CHECK(c.cc.HasUserChops(), "three true");
    }
    scenario("SelectedChopStartFrame/EndFrame");
    {
        Ctx c(44100, 4);
        c.chopsInitialized = true;
        c.model.boundaryCount = 2;
        c.model.boundaries[0] = 10;
        c.model.boundaries[1] = 20;
        c.model.selected = 0;
        CHECK_EQ_INT(c.cc.SelectedChopStartFrame(), 10, "start");
        CHECK_EQ_INT(c.cc.SelectedChopEndFrame(), 20, "end");
        c.model.boundaryCount = 1;
        CHECK_EQ_INT(c.cc.SelectedChopStartFrame(), 0, "start degenerate");
        CHECK_EQ_INT(c.cc.SelectedChopEndFrame(), 44100 - 1,
                     "end degenerate");
    }
    scenario("AddChopAtCursor live out of range");
    {
        Ctx c(100, 4);
        c.preview.SetRange(10, 90, c.sourceSize);
        c.host.streamingActive = true;
        c.host.hasStreamPos = true;
        c.host.nextStreamPos = 5000; /* fuera de 10..90 */
        c.cursorFrame = 0;
        c.cc.AddChopAtCursor();
        /* liveCut false: cursor intacto y borde -> mensaje edge */
        CHECK_EQ_INT(c.cursorFrame, 0, "cursor kept");
        CHECK_EQ_STR(c.host.statuses.back(), "status:Cannot chop at edge",
                     "edge msg");
    }
}

/* ------------------------------------------------------------------ */

int main() {
    TestInitialize();
    TestAddChopAtCursor();
    TestDeleteSelectedChop();
    TestSelectChop();
    TestToggleTrimMode();
    TestNudgeSelected();
    TestCropToSelectedRange();
    TestSplitSampleIntoEqualParts();
    TestClearAllChops();
    TestCycleSplitParts();
    TestSnapSelectedBoundaryToZeroCross();
    TestHelperMethods();

    if (g_fails == 0) {
        std::printf("ALL OK (%d checks)\n", g_checks);
        return 0;
    }
    std::printf("FAILED (%d/%d checks)\n", g_fails, g_checks);
    return 1;
}