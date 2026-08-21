#include "EQ.h"
#include <math.h>
#include <stdint.h>
#include "Application/Utils/fixed.h"

// BUG1 FIX (Bacon 1.5 FX): EQ <-80 dB
static const uint16_t eqGainTable[] = {
    0, 0, 1, 1, 2, 3, 4, 5, 7, 9, 12, 15, 20, 26, 34, 44,
    57, 74, 96, 124, 161, 209, 271, 351, 455, 590, 765, 992, 1286, 1667, 2161, 2802,
    3632, 4708, 6102, 7909, 10252, 13289, 17225, 22330, 28946, 37523, 48641, 63054, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535
};
static const int kEqGainTableSize = sizeof(eqGainTable)/sizeof(eqGainTable[0]);
fixed EQ_GetGainFixed(int dB){
    int idx=dB+80;
    if(idx<0) idx=0;
    if(idx>=kEqGainTableSize) idx=kEqGainTableSize-1;
    if(dB<=-80) return 0;
    float m=powf(10.0f,(float)dB/20.0f);
    if(m<0) m=0; if(m>4) m=4;
    return fl2fp(m);
}
float EQ_GetMultiplierFloat(int dB){
    if(dB<=-80) return 0.0f;
    if(dB>=24) return powf(10.0f,24.0f/20.0f);
    return powf(10.0f,(float)dB/20.0f);
}
