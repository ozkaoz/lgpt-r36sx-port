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
int TreeFrogUac2Bridge_StartUsbCapture(const char *wav_path, int seconds);
int TreeFrogUac2Bridge_StopUsbCapture(void);
const char *TreeFrogUac2Bridge_GetUsbCaptureStatusText(void);
int TreeFrogUac2Bridge_GetLastCaptureName(char *dst, int dst_len);
int TreeFrogUac2Bridge_GetLastCapturePath(char *dst, int dst_len);
int TreeFrogUac2Bridge_GetUsbCaptureLevelPercent(void);
int TreeFrogUac2Bridge_GetUsbCaptureLevelLeftPercent(void);
int TreeFrogUac2Bridge_GetUsbCaptureLevelRightPercent(void);
int TreeFrogUac2Bridge_GetUsbCaptureElapsedSeconds(void);
int TreeFrogUac2Bridge_DiscardUsbCapture(void);
int TreeFrogUac2Bridge_SetUsbMonitor(int enable);
int TreeFrogUac2Bridge_GetUsbMonitor(void);
#ifdef __cplusplus
}
#endif
#endif
