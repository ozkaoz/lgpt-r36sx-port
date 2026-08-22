#ifndef _PERF_DIAG_H_
#define _PERF_DIAG_H_
namespace PerfDiag {
extern unsigned long g_frameCount;
extern unsigned long g_flushCount;
extern unsigned long g_videoRefreshCount;
extern unsigned long g_eqDrawCount;
extern unsigned long g_mixerDrawCount;
extern unsigned long g_analyzerComputeCount;
extern unsigned long g_analyzerComputedTrue;
extern unsigned long g_inputPollCount;
extern unsigned long g_inputEventCount;
extern char g_currentTag[64];
extern char g_currentView[32];
void Init();
void CountFrame();
void CountFlush();
void CountVideoRefresh();
void CountEqDraw();
void CountMixerDraw();
void CountAnalyzer(bool didCompute);
void CountInputPoll();
void CountInputEvent();
void SetTag(const char* tag);
void SetView(const char* v);
void SetDriver(const char* driver);
void SetDriverMode(int mode);
void Dump(const char* reason);
void Reset(const char* tag, const char* view);
void PeriodicCheck(unsigned long nowMs);
}
#endif
