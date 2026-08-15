/****************************
 Filter V2 -- TPT State Variable Filter (see FilterV2.h).
****************************/

#include "FilterV2.h"
#include <math.h>

#define FV2_CHANNELS 8

// One-pole smoothing factor per sample (~33-sample time constant).  Small
// enough to kill k-rate zipper steps, short enough to stay click-free and
// responsive.  Snap when the Q15 step would truncate to zero (same trick as
// ParametricEQ::FX_EQ_SNAP) so the coefficient always reaches its target.
#define FV2_SMOOTH fl2fp(0.03f)

static filter_v2_t g_filterV2[FV2_CHANNELS];
static bool g_filterV2Inited = false;

void init_filters_v2(void) {
	if (g_filterV2Inited) return ;
	for (int c = 0; c < FV2_CHANNELS; c++) {
		filter_v2_t *f = &g_filterV2[c];
		for (int i = 0; i < 2; i++) {
			f->ic1eq[i] = 0 ;
			f->ic2eq[i] = 0 ;
		}
		f->type = FV2_LOWPASS ;
		f->mix = 0 ;
		f->dirt = i2fp(1) ;
		f->rate = 48000 ;
		f->a1 = i2fp(1) ; f->a2 = 0 ; f->a3 = 0 ; f->k = i2fp(1) ;
		f->a1T = i2fp(1) ; f->a2T = 0 ; f->a3T = 0 ; f->kT = i2fp(1) ;
		f->coeffState = 0 ;
	}
	g_filterV2Inited = true ;
}

static void filterv2_targets(filter_v2_t *f, float fc, float q) {
	int rate = f->rate ;
	if (rate < 8000) rate = 8000 ;
	if (fc < 20.0f) fc = 20.0f ;
	if (fc > 0.45f * (float)rate) fc = 0.45f * (float)rate ;
	float w0 = 2.0f * 3.14159265f * fc / (float)rate ;
	if (w0 > 3.14159265f * 0.9f) w0 = 3.14159265f * 0.9f ;
	if (w0 <= 0.0f) w0 = 1e-6f ;
	float g = tanf(w0 * 0.5f) ;  // TPT: g = tan(w0/2) = tan(pi*fc/fs)
	float k = 1.0f / q ;
	if (k > 1.0f) k = 1.0f ;
	float a1 = 1.0f / (1.0f + g * (g + k)) ;
	float a2 = g * a1 ;
	float a3 = g * a2 ;
	f->a1T = fl2fp(a1) ;
	f->a2T = fl2fp(a2) ;
	f->a3T = fl2fp(a3) ;
	f->kT = fl2fp(k) ;
}

void set_filter_v2(int channel, FilterV2Type type, fixed cutoff, fixed reso,
                   int mix, bool bassyMapping, bool scream, int rate) {
	if (channel < 0 || channel >= FV2_CHANNELS) channel = 0 ;
	if (!g_filterV2Inited) init_filters_v2() ;
	filter_v2_t *f = &g_filterV2[channel] ;
	if (type < 0 || type >= FV2_TYPECOUNT) type = FV2_LOWPASS ;

	// Reset the integrators only on a topology change so cross-note playback
	// stays click-free (mirrors the legacy set_filter behaviour).
	if (f->type != type) {
		for (int i = 0; i < 2; i++) {
			f->ic1eq[i] = 0 ;
			f->ic2eq[i] = 0 ;
		}
		f->type = type ;
		f->coeffState = 0 ;
	}
	if (rate != f->rate) {
		f->rate = rate ;
		f->coeffState = 0 ;
	}

	f->mix = fp_mul(i2fp(mix), fl2fp(1.0f / 255.0f)) ;
	if (f->mix < 0) f->mix = 0 ;
	if (f->mix > i2fp(1)) f->mix = i2fp(1) ;

	float p1 = fp2fl(cutoff) ;
	if (p1 < 0.0f) p1 = 0.0f ;
	if (p1 > 1.0f) p1 = 1.0f ;
	float p2 = fp2fl(reso) ;
	if (p2 < 0.0f) p2 = 0.0f ;
	if (p2 > 1.0f) p2 = 1.0f ;

	// Legacy-exact frequency mapping (Hz).
	float fc ;
	if (bassyMapping) {
		fc = powf(10.0f, 0.6f + 3.1f * p1) ;
	} else {
		fc = p1 * p1 * 22050.0f ;
	}
	// Legacy-exact resonance curve, remapped to Q in [1.0, 4.0] (k <= 1).
	float res = 1.0f - (1.0f - p2) * (1.0f - p2) * (1.0f - p2) ;
	float q = 1.0f + 3.0f * res ;
	filterv2_targets(f, fc, q) ;

	// Scream: legacy dirt gain as a pre-drive with a hard clip in Process.
	if (scream) {
		float dirt = 100.0f * (1.0f - p1) + 5000.0f * p1 ;
		f->dirt = fl2fp(1.0f + dirt * 0.0005f) ;
	} else {
		f->dirt = i2fp(1) ;
	}
}

filter_v2_t *get_filter_v2(int channel) {
	if (channel < 0 || channel >= FV2_CHANNELS) channel = 0 ;
	if (!g_filterV2Inited) init_filters_v2() ;
	return &g_filterV2[channel] ;
}

static inline void smoothCoeff(fixed &cur, fixed tgt, fixed alpha) {
	fixed step = fp_mul(alpha, tgt - cur) ;
	if (step == 0 && cur != tgt) cur = tgt ;
	else cur += step ;
}

fixed filterv2_process(filter_v2_t *f, int elem, fixed x) {
	if (f->mix == 0) return x ;
	if (elem < 0) elem = 0 ;
	if (elem > 1) elem = 1 ;

	// Coefficient smoothing / snap (once per filter, elem 0 drives it).
	if (f->coeffState == 0) {
		f->a1 = f->a1T ; f->a2 = f->a2T ; f->a3 = f->a3T ; f->k = f->kT ;
		f->coeffState = 1 ;
	} else {
		smoothCoeff(f->a1, f->a1T, FV2_SMOOTH) ;
		smoothCoeff(f->a2, f->a2T, FV2_SMOOTH) ;
		smoothCoeff(f->a3, f->a3T, FV2_SMOOTH) ;
		smoothCoeff(f->k, f->kT, FV2_SMOOTH) ;
	}

	// Scream pre-drive (with a hard clip to keep the Q15 states bounded).
	if (f->dirt != i2fp(1)) {
		x = fp_mul(x, f->dirt) ;
		if (x > i2fp(2)) x = i2fp(2) ;
		else if (x < -i2fp(2)) x = -i2fp(2) ;
	}

	fixed v3 = x - f->ic2eq[elem] ;
	fixed v1 = fp_mul(f->a1, f->ic1eq[elem]) + fp_mul(f->a2, v3) ;
	fixed v2 = f->ic2eq[elem] + fp_mul(f->a2, f->ic1eq[elem]) + fp_mul(f->a3, v3) ;
	f->ic1eq[elem] = 2 * v1 - f->ic1eq[elem] ;
	f->ic2eq[elem] = 2 * v2 - f->ic2eq[elem] ;

	fixed wet ;
	switch (f->type) {
	case FV2_HIGHPASS: wet = x - fp_mul(f->k, v1) - v2 ; break ;
	case FV2_BANDPASS: wet = v1 ; break ;
	case FV2_NOTCH:    wet = x - fp_mul(f->k, v1) ; break ;
	default:           wet = v2 ; break ;  // FV2_LOWPASS
	}
	return x + fp_mul(wet - x, f->mix) ;
}