/****************************
 Filter V2 -- TPT State Variable Filter
 (bacon-1.5, item 2).  Replaces the legacy Chamberlin SVF used by the
 sample-instrument render loop with a Zavalishin TPT (trapezoidal
 state-variable) structure in Q15 fixed point.

   g  = tan(pi*fc/fs)            (float, control-rate)
   k  = 1/Q,  Q = 1 + 3*res      (Q15 in [0.25, 1.0], stable, no self-osc)
   a1 = 1/(1+g*(g+k)), a2 = g*a1, a3 = g*a2     (all <= 1, Q15-exact)
   v3 = x - ic2 ; v1 = a1*ic1 + a2*v3 ; v2 = ic2 + a2*ic1 + a3*v3
   ic1 = 2*v1 - ic1 ; ic2 = 2*v2 - ic2
   LP = v2 ; BP = v1 ; HP = x - k*v1 - v2 ; NOTCH = x - k*v1

 Types FV2_LOWPASS/HIGHPASS/BANDPASS/NOTCH give clearly differentiated
 responses.  Coefficients are computed in float at control rate
 (set_filter_v2) and smoothed per sample (one-pole) for anti-zipper.
 Frequency mappings preserve the legacy Hz exactly:
   original: fc = p1^2 * 22050
   bassy:    fc = 10^(0.6 + 3.1*p1)
 Resonance preserves the legacy curve res = 1-(1-p2)^3.
 Scream mode applies a legacy-style pre-drive (dirt) before the filter
 with a hard clip to keep the Q15 states bounded.
 Dry/wet: out = x + (wet - x)*mix.

 There are 8 filters, one per voice (mirrors the legacy filter[8]); each
 filter is stereo, so 2 integrator pairs per filter.  The state is static
 so rendering can be resumed across buffers without clicks.
****************************/

#ifndef _FILTER_V2_H_
#define _FILTER_V2_H_

#include "Application/Utils/fixed.h"

typedef enum {
	FV2_LOWPASS = 0,
	FV2_HIGHPASS,
	FV2_BANDPASS,
	FV2_NOTCH,
	FV2_TYPECOUNT
} FilterV2Type;

typedef struct {
	fixed ic1eq[2];       // integrator states, one pair per element (L/R)
	fixed ic2eq[2];
	fixed a1, a2, a3, k;  // smoothed coefficients (Q15)
	fixed a1T, a2T, a3T, kT;  // control-rate targets
	FilterV2Type type;
	fixed mix;            // dry/wet 0..1 (Q15)
	fixed dirt;           // scream pre-drive gain
	int rate;
	int coeffState;       // 0 = cold (snap), 1 = warm (smooth)
} filter_v2_t;

void init_filters_v2(void) ;

void set_filter_v2(int channel, FilterV2Type type, fixed cutoff, fixed reso,
                   int mix, bool bassyMapping, bool scream, int rate);

filter_v2_t *get_filter_v2(int channel) ;

fixed filterv2_process(filter_v2_t *f, int elem, fixed x);

#endif