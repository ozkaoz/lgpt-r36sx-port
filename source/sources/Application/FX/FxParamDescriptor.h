#ifndef _FX_PARAM_DESCRIPTOR_H_
#define _FX_PARAM_DESCRIPTOR_H_

#include <math.h>

// FXP_DESCRIPTORS_V1 (bacon-1.5, item 1): capa comun de parametros FX
// normalizados.  Cada parametro continuo del port se describe con un
// FxParamDescriptor que define sus tres vistas:
//   - percent : 0..100 (o -100..100 para parametros simetricos) -- la
//     UNICA vista visible de edicion para parametros continuos.
//   - raw     : valor legacy persistido (00-FF / 0000-FFFF) cuando el
//     parametro se guarda como entero en proyectos (p. ej. SIP_FILTCUTOFF).
//   - dsp     : valor natural del motor (ms, dB, Hz, s, Q o Q15 0..1).
// Los mapeos son monotonicos, acotados y reversibles; el raw legacy nunca
// se reescribe desde percent salvo cuando el usuario edita explicitamente,
// de modo que la serializacion de proyectos existentes queda intacta.
// La capa es pura: solo <math.h>, sin GUI, audio, Player ni SamplePool.
// Utiliza mapeos perceptualmente adecuados: LOG2 para frecuencia/tiempo
// (igual % = igual octava/doble), LINEAR para ganancias/mezclas y
// SWITCH/DISCRETE para ON/OFF, modos y tipos.

enum FxParamKind {
    FX_PARAM_CONTINUOUS = 0,  // 0..100 %
    FX_PARAM_SWITCH = 1,      // 0/1 (ON/OFF)
    FX_PARAM_SIGNED = 2       // -100..100 %, centro 0 (pan, ganancias sim.)
};

enum FxParamCurve {
    FX_CURVE_LINEAR = 0,      // v = vmin + n*(vmax-vmin)
    FX_CURVE_LOG2 = 1         // v = vmin * (vmax/vmin)^n  (requiere vmin>0)
};

struct FxParamDescriptor {
    const char *label_ ;       // nombre corto (misma etiqueta que la UI)
    FxParamKind kind_ ;
    FxParamCurve curve_ ;
    int rawMin_ ;              // vista raw (entera) cuando existe
    int rawMax_ ;
    int rawDefault_ ;
    float dspMin_ ;            // vista dsp (natural) cuando existe
    float dspMax_ ;
    float dspDefault_ ;
    const char *unit_ ;        // "ms","s","Hz","dB","Q","" o 0
} ;

// ---------------------------------------------------------------------------
// Mapeos puros percent <-> dsp (parametros float del motor)
// ---------------------------------------------------------------------------

// percent [0,100] -> dsp, con la curva del descriptor (clamp 0..100).
inline float fxPercentToDsp(const FxParamDescriptor &d,int p) {
	if (p<0) p=0 ;
	if (p>100) p=100 ;
	float n=(float)p*0.01f ;
	switch (d.curve_) {
	case FX_CURVE_LOG2:
		return d.dspMin_*powf(d.dspMax_/d.dspMin_,n) ;
	default:
		return d.dspMin_+n*(d.dspMax_-d.dspMin_) ;
	}
}

// dsp -> percent [0,100] (inverso exacto de fxPercentToDsp; clamp al rango).
inline int fxDspToPercent(const FxParamDescriptor &d,float v) {
	if (v<d.dspMin_) v=d.dspMin_ ;
	if (v>d.dspMax_) v=d.dspMax_ ;
	switch (d.curve_) {
	case FX_CURVE_LOG2:
		return (int)(logf(v/d.dspMin_)/logf(d.dspMax_/d.dspMin_)*100.0f+0.5f) ;
	default:
		return (int)((v-d.dspMin_)/(d.dspMax_-d.dspMin_)*100.0f+0.5f) ;
	}
}

// ---------------------------------------------------------------------------
// Mapeos puros percent <-> raw (parametros legacy enteros persistidos)
// ---------------------------------------------------------------------------

// percent -> raw.  Para FX_PARAM_SIGNED el percent es -100..100 y el raw
// central (punto medio del rango) es el 0 %.
inline int fxPercentToRaw(const FxParamDescriptor &d,int p) {
	int r ;
	if (d.kind_==FX_PARAM_SIGNED) {
		if (p<-100) p=-100 ;
		if (p>100) p=100 ;
		int mid=d.rawMin_+(d.rawMax_-d.rawMin_)/2 ;
		int half=d.rawMax_-mid ;
		r=mid+(int)(((float)p*0.01f)*(float)half+((p>=0)?0.5f:-0.5f)) ;
	} else {
		if (p<0) p=0 ;
		if (p>100) p=100 ;
		r=d.rawMin_+(int)(((float)p*0.01f)*(float)(d.rawMax_-d.rawMin_)+0.5f) ;
	}
	if (r<d.rawMin_) r=d.rawMin_ ;
	if (r>d.rawMax_) r=d.rawMax_ ;
	return r ;
}

// raw -> percent (inverso de fxPercentToRaw).
inline int fxRawToPercent(const FxParamDescriptor &d,int raw) {
	if (raw<d.rawMin_) raw=d.rawMin_ ;
	if (raw>d.rawMax_) raw=d.rawMax_ ;
	if (d.kind_==FX_PARAM_SIGNED) {
		int mid=d.rawMin_+(d.rawMax_-d.rawMin_)/2 ;
		int half=d.rawMax_-mid ;
		return (int)(((float)(raw-mid)/(float)half)*100.0f+((raw>=mid)?0.5f:-0.5f)) ;
	}
	return (int)(((float)(raw-d.rawMin_)/(float)(d.rawMax_-d.rawMin_))*100.0f+0.5f) ;
}

// ---------------------------------------------------------------------------
// Descriptores de parametros continuos de instrumento (vista raw legacy)
// ---------------------------------------------------------------------------
// Los valores persistidos (SIP_*) no cambian: solo se leen/escriben a
// traves de la capa para la UI.  Las curvas legacy del DSP (p.ej.
// cutoff^2) permanecen en el render path; aqui el percent mapea lineal el
// rango raw para que el display sea exacto y reversible.

#define FX_DESC_INST_LABEL(label) label

enum FxInstParamId {
    FX_INST_PAN = 0,
    FX_INST_DETUNE,
    FX_INST_FILTER_MIX,
    FX_INST_FILTER_CUTOFF,
    FX_INST_FILTER_RESO,
    FX_INST_ATTENUATE,
    FX_INST_DRIVE,
    FX_INST_PARAM_COUNT
} ;

static const FxParamDescriptor kFxInstParams_[FX_INST_PARAM_COUNT] = {
    { "pan",       FX_PARAM_SIGNED,     FX_CURVE_LINEAR, 0,   254, 127, 0.0f, 0.0f, 0.0f, "%" },
    { "detune",    FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, 0,   255, 0,   0.0f, 0.0f, 0.0f, "%" },
    { "type",      FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, 0,   255, 0,   0.0f, 0.0f, 0.0f, "%" },
    { "cutoff",    FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, 0,   255, 255, 0.0f, 0.0f, 0.0f, "%" },
    { "reso",      FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, 0,   255, 0,   0.0f, 0.0f, 0.0f, "%" },
    { "attenuate", FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, 1,   255, 255, 0.0f, 0.0f, 0.0f, "%" },
    { "drive",     FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, 0,   255, 0,   0.0f, 0.0f, 0.0f, "%" },
} ;

#endif