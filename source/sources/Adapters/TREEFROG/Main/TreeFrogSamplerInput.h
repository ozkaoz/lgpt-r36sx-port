#ifndef TREEFROG_SAMPLER_INPUT_H
#define TREEFROG_SAMPLER_INPUT_H

#include <stdint.h>

/*
 * U2.50.5 sampler input source control.
 *
 * All LGPT views use one coherent authoritative source. Libretro is primary
 * after input_poll_cb(); Cubevol is diagnostic/fallback only when the frontend
 * input callback is unavailable.
 *
 * No debounce, frame threshold, timing lockout, or action quarantine is
 * applied here.
 */
enum TreeFrogSamplerSelectedSource {
    TFSS_NONE = 0,
    TFSS_LIBRETRO = 1,
    TFSS_CUBEVOL = 2
};

enum TreeFrogSamplerPhysicalBits {
    TFSP_LEFT   = 1u << 0,
    TFSP_RIGHT  = 1u << 1,
    TFSP_UP     = 1u << 2,
    TFSP_DOWN   = 1u << 3,
    TFSP_A      = 1u << 4,
    TFSP_B      = 1u << 5,
    TFSP_X      = 1u << 6,
    TFSP_Y      = 1u << 7,
    TFSP_L1     = 1u << 8,
    TFSP_R1     = 1u << 9,
    TFSP_L2     = 1u << 10,
    TFSP_R2     = 1u << 11,
    TFSP_START  = 1u << 12,
    TFSP_SELECT = 1u << 13,
    TFSP_FN     = 1u << 14
};

struct TreeFrogSamplerInputSnapshot {
    uint32_t cubevolRaw;
    uint32_t cubevolPhysical;
    uint32_t libretroPhysical;
    uint32_t selectedPhysical;
    uint16_t joypadBits;
    unsigned long snapshotSequence;
    unsigned long sequentialMismatchCount;
    int cubevolAvailable;
    int libretroAvailable;
    int bitmaskSupported;
    int selectedSource;
    int exclusiveMode;
};

extern "C" void TreeFrogSamplerInput_Read(
    TreeFrogSamplerInputSnapshot *snapshot);

extern "C" void TreeFrogSamplerInput_SetExclusiveMode(int enable);
extern "C" int TreeFrogSamplerInput_GetExclusiveMode(void);

extern "C" const char *TreeFrogSamplerInput_BuildMarker(void);

#endif
