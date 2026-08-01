#ifndef TREEFROG_WINDOWS_SPSC_TRANSPORT_H
#define TREEFROG_WINDOWS_SPSC_TRANSPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * H38.1 Windows producer isolation.
 *
 * The audio callback may call Submit() only. Submit() performs a bounded copy
 * into a single-producer/single-consumer ring and never performs file I/O,
 * sysfs access, logging, allocation, waiting, resampling or format conversion.
 */
int TreeFrogWindowsSpscTransport_Start(void);
void TreeFrogWindowsSpscTransport_Stop(void);
void TreeFrogWindowsSpscTransport_Submit(
    const int16_t *stereo44100,
    unsigned frames);
void TreeFrogWindowsSpscTransport_SetGainPercent(
    int mixer_percent,
    int project_master_percent);
int TreeFrogWindowsSpscTransport_ShouldMuteLocal(void);
void TreeFrogWindowsSpscTransport_SetMonitorEnabled(int enabled);
void TreeFrogWindowsSpscTransport_MixMonitorStereo44100(
    int16_t *stereo44100,
    unsigned frames);
const char *TreeFrogWindowsSpscTransport_BuildMarker(void);

#ifdef __cplusplus
}
#endif

#endif
