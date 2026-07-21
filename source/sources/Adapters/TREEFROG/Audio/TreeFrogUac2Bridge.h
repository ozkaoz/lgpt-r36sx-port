#ifndef TREEFROG_UAC2_BRIDGE_H
#define TREEFROG_UAC2_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void TreeFrogUac2Bridge_Prime(void);
void TreeFrogUac2Bridge_ResetTransport(void);
void TreeFrogUac2Bridge_Close(void);
void TreeFrogUac2Bridge_SubmitStereo44100(const int16_t *stereo, int frames);
void TreeFrogUac2Bridge_MixUsbCaptureMonitorStereo44100(int16_t *stereo, int frames);
int  TreeFrogUac2Bridge_ShouldMuteLocal(void);
void TreeFrogUac2Bridge_SetMixerVolumePercent(int volume);
void TreeFrogUac2Bridge_SetProjectMasterVolumePercent(int volume);
int  TreeFrogUac2Bridge_GetMixerVolumePercent(void);
int  TreeFrogUac2Bridge_GetDriverMode(void);
const char *TreeFrogUac2Bridge_GetDriverModeName(void);
const char *TreeFrogUac2Bridge_SetDriverMode(int mode);
const char *TreeFrogUac2Bridge_CycleDriverMode(void);
int TreeFrogUac2Bridge_GetDriverModeCount(void);
const char *TreeFrogUac2Bridge_GetDriverModeNameByIndex(int mode);
const char *TreeFrogUac2Bridge_GetDriverModeDescriptionByIndex(int mode);
int TreeFrogUac2Bridge_IsDriverModeSelectable(int mode);
const char *TreeFrogUac2Bridge_GetUsbStateText(void);

enum TreeFrogUsbCaptureState {
    TREEFROG_USB_CAPTURE_IDLE = 0,
    TREEFROG_USB_CAPTURE_STARTING = 1,
    TREEFROG_USB_CAPTURE_RECORDING = 2,
    TREEFROG_USB_CAPTURE_STOPPING = 3,
    TREEFROG_USB_CAPTURE_READY = 4,
    TREEFROG_USB_CAPTURE_ERROR = 5
};

typedef struct TreeFrogUsbCaptureSnapshot {
    int state;
    int levelPercent;
    int levelLeftPercent;
    int levelRightPercent;
    int elapsedSeconds;
    int monitorEnabled;
    long frames;
    long bytes;
    char status[192];
    char error[160];
    char path[300];
    char name[128];
} TreeFrogUsbCaptureSnapshot;

/* One cached, internally consistent capture snapshot.  force=0 reuses data
 * refreshed in the last 50 ms; force!=0 performs one immediate refresh. */
int TreeFrogUac2Bridge_GetUsbCaptureSnapshot(
    TreeFrogUsbCaptureSnapshot *snapshot,
    int force);

int TreeFrogUac2Bridge_IsUsbReady(void);
int TreeFrogUac2Bridge_IsRecordingDaemonReady(void);
const char *TreeFrogUac2Bridge_GetRecordingDaemonVersionText(void);
const char *TreeFrogUac2Bridge_GetRecordingDaemonAbiText(void);
int TreeFrogUac2Bridge_StartUsbCapture(const char *wav_path, int seconds);
int TreeFrogUac2Bridge_StopUsbCapture(void);
const char *TreeFrogUac2Bridge_GetUsbCaptureStatusText(void);
int TreeFrogUac2Bridge_GetLastCaptureName(char *dst, int dst_len);
int TreeFrogUac2Bridge_GetLastCapturePath(char *dst, int dst_len);
int TreeFrogUac2Bridge_GetUsbCaptureLevelPercent(void);
int TreeFrogUac2Bridge_GetUsbCaptureLevelLeftPercent(void);
int TreeFrogUac2Bridge_GetUsbCaptureLevelRightPercent(void);
int TreeFrogUac2Bridge_GetUsbCaptureElapsedSeconds(void);
int TreeFrogUac2Bridge_GetUsbCaptureState(void);
long TreeFrogUac2Bridge_GetUsbCaptureFrames(void);
long TreeFrogUac2Bridge_GetUsbCaptureBytes(void);
const char *TreeFrogUac2Bridge_GetUsbCaptureErrorText(void);
int TreeFrogUac2Bridge_DiscardUsbCapture(void);
int TreeFrogUac2Bridge_CommitUsbCapture(void);
int TreeFrogUac2Bridge_SetUsbMonitor(int enable);
int TreeFrogUac2Bridge_GetUsbMonitor(void);
#ifdef __cplusplus
}
#endif
#endif
