#ifndef _MIXER_METERS_H_
#define _MIXER_METERS_H_

// F3-4b (docs/F3_ARCHITECTURE_ES.md): capa pura de los medidores VU del
// Mixer.  Contiene el smoothing golden (ataque instantaneo / release
// exponencial), la conversion a nivel de barra post-volumen y la metrica de
// las barras half-cell L/R (banda roja 0 dB+, fill en pasos de 2 px).
// No depende de GUI, audio, Player, SamplePool ni del framebuffer: solo
// <math.h> y FxPages.h (que a su vez solo usa fixed.h).
// Todo el comportamiento es byte-identico al que vivia en MixerView.cpp
// (golden Bacon 1.2.1).
#include <math.h>
#include "Application/Mixer/FxPages.h"

// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): each side of a channel is
// smoothed independently from its own post-pan peak.
class MixerMeters {
public:
	// == SONG_CHANNEL_COUNT (Application/Model/Song.h)
	static const int kChannels = 8 ;
	// TREEFROG_MIXER_COMPACT_BARS_V1 (Bacon 1.1.1 V13): each level is 3 px
	// tall (2 px fill + 1 px gap).
	static const int kLevelHeight = 3 ;
	// TREEFROG_MIXER_VU_DB_SCALE_V5: the displayed 0 dB row is at level 36/39
	// on the -36..+3 rebased scale (see FxPages.h mixVULevel).
	static float ZeroDbLevel() { return 36.0f / 39.0f ; }

	MixerMeters() {
		for (int i = 0 ; i < kChannels ; i++) {
			vuL_[i] = 0.0f ;
			vuR_[i] = 0.0f ;
		}
	}

	// TREEFROG_MIXER_VU_SMOOTH_V1 (H38.7): blend the raw audio peak into the
	// display levels.  Attack is instant (a note pops straight up), release
	// is a smooth per-frame exponential fall (~0.6^12 empties a full bar in
	// about 12 frames).  TREEFROG_MIXER_VU_STOP_RESET_V1 (Bacon 1.1.1 V14):
	// when the player is not running the peaks are sampled as 0 so the
	// release decay pulls every bar back to 0.
	void SmoothFrame(bool running, int channels, const float *peakL,
	                 const float *peakR) {
		if (channels > kChannels) channels = kChannels ;
		for (int i = 0 ; i < channels ; i++) {
			float measuredL = running ? peakL[i] : 0.0f ;
			if (measuredL > vuL_[i]) {
				vuL_[i] = measuredL ;
			} else {
				vuL_[i] *= 0.6f ;
				if (vuL_[i] < 0.001f) vuL_[i] = 0.0f ;
			}
			float measuredR = running ? peakR[i] : 0.0f ;
			if (measuredR > vuR_[i]) {
				vuR_[i] = measuredR ;
			} else {
				vuR_[i] *= 0.6f ;
				if (vuR_[i] < 0.001f) vuR_[i] = 0.0f ;
			}
		}
	}

	float LevelL(int ch) const { return vuL_[ch] ; }
	float LevelR(int ch) const { return vuR_[ch] ; }

	// TREEFROG_MIXER_COMPACT_BARS_V1: normalized post-volume level of one
	// side, recorded by drawMeterBar and rendered by PostFlushDraw.
	static float BarLevel(float peak, int volume) {
		float level = mixVULevel(peak) * float(volume) * 0.01f ;
		if (level < 0.0f) level = 0.0f ;
		if (level > 1.0f) level = 1.0f ;
		return level ;
	}

	// TREEFROG_MIXER_HALF_CELL_BARS_V1 + TREEFROG_MIXER_COMPACT_BARS_V1 +
	// TREEFROG_MIXER_BARS_BOTTOM_UP_V1: pixel metric of one half-cell bar.
	// height is the bar in character cells; levelL/levelR are the recorded
	// normalized levels (0..1).  All output in pixels / pixel-levels, ready
	// for the framebuffer renderer (which keeps the color resolution).
	struct Geometry {
		int totalPx ;          // full bar height in pixels
		int totalLevels ;      // 2-px levels along the bar
		int redBandLevels ;    // levels inside the 0 dB+ red zone
		int redBandPx ;        // red zone height in pixels
		int filledLLevels ;    // filled 2-px levels, left side
		int filledRLevels ;    // filled 2-px levels, right side
	} ;
	static Geometry GeometryFor(int height, float levelL, float levelR) {
		Geometry g ;
		g.totalPx = height * 8 ;
		g.totalLevels = g.totalPx / kLevelHeight ;
		if (g.totalLevels < 1) {
			g.redBandLevels = 0 ;
			g.redBandPx = 0 ;
			g.filledLLevels = 0 ;
			g.filledRLevels = 0 ;
			return g ;
		}
		// 0 dB+ zone: the top of the bar above the 0 dB row on the shared
		// -36..+3 scale (mixVULevel(0 dB) = 36/39), rendered solid red.
		g.redBandLevels = g.totalLevels - (int)(ZeroDbLevel() * (float)g.totalLevels + 0.5f) ;
		if (g.redBandLevels < 1) g.redBandLevels = 1 ;
		g.redBandPx = g.redBandLevels * kLevelHeight ;
		int filledL = (int)(levelL * (float)g.totalLevels + 0.5f) * kLevelHeight ;
		int filledR = (int)(levelR * (float)g.totalLevels + 0.5f) * kLevelHeight ;
		if (filledL > g.totalPx) filledL = g.totalPx ;
		if (filledR > g.totalPx) filledR = g.totalPx ;
		g.filledLLevels = filledL / kLevelHeight ;
		g.filledRLevels = filledR / kLevelHeight ;
		return g ;
	}

	// Per-row render decision inside the bar column: whether the row is in
	// the solid red band, the 1-px gap row between levels (bottom row of
	// each 3-px group, never inside the band), and whether each side is
	// filled at this row.
	struct RowState {
		bool inBand ;
		bool gapRow ;
		bool fillL ;
		bool fillR ;
	} ;
	static RowState RowStateFor(int row, int totalPx, int redBandPx,
	                            int filledLLevels, int filledRLevels) {
		RowState s ;
		s.inBand = (row < redBandPx) ;
		int levelIdx = (totalPx - 1 - row) / kLevelHeight ;
		s.fillL = (levelIdx < filledLLevels) ;
		s.fillR = (levelIdx < filledRLevels) ;
		s.gapRow = ((row % kLevelHeight) == 0) && !s.inBand ;
		return s ;
	}

private:
	float vuL_[kChannels] ;
	float vuR_[kChannels] ;
} ;

#endif
