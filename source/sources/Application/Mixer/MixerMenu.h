#ifndef _MIXER_MENU_H_
#define _MIXER_MENU_H_

// F3-4c (docs/F3_ARCHITECTURE_ES.md): capa pura del menu L1+A del Mixer
// (TREEFROG_MIXER_ACTION_MENU_V1, Bacon 1.1.1 V13).  Declara como datos los
// dos menus del L1+A: MASTER (6 filas: LIMITER / CLIP GAIN / FX DELAY /
// FX REVERB / FX EQ / FX COMP) y TRACK (5 secciones: FILTER / BITCRUSHER /
// EQ8 / FX SENDS / AUTOMATION), los clamps golden de los valores
// editables (softclip 0..4, clip gain 0..1) y el codificado de accion
// (fila master >= 2 -> pagina FX 1..4 = DELAY..COMP; fila track ->
// seccion 101..105 del editor de instrumento).
// BASS_SYNTH_EQ8_MENU (bacon-1.5, feedback): the inherited LGPT PLAYBACK
// section was removed from the instrument page; the TRACK menu row that
// jumped there now jumps to the EQ8 row instead.
// No depende de GUI, audio, Player, SamplePool ni del framebuffer: solo
// Foundation (MAKE_FOURCC para los hints FourCC de seccion).
// Todo el comportamiento es byte-identico al que vivia en MixerView.cpp
// (golden Bacon 1.2.1).
#include "Foundation/Types/Types.h"

// Numero de filas de cada menu.
static const int kMixerMasterMenuRowCount = 6 ;
static const int kMixerTrackMenuRowCount = 5 ;

// Etiquetas de fila del menu MASTER (filas 0..1 editables con L/R).
static const char *kMixerMasterMenuLabels[kMixerMasterMenuRowCount] = {
    "LIMITER", "CLIP GAIN",
    "FX DELAY", "FX REVERB", "FX EQ", "FX COMP" } ;

// Etiquetas de fila del menu TRACK (secciones del editor de instrumento).
static const char *kMixerTrackMenuLabels[kMixerTrackMenuRowCount] = {
    "FILTER", "BITCRUSHER", "EQ8", "FX SENDS", "AUTOMATION" } ;

// Clamps golden de los valores editables del menu MASTER.
static const int kMixerSoftclipMin = 0 ;
static const int kMixerSoftclipMax = 4 ;
static const int kMixerSoftclipGainMin = 0 ;
static const int kMixerSoftclipGainMax = 1 ;

// Hint FourCC de cada seccion TRACK (SIP_* puros de SampleInstrument.h).
static const unsigned int kMixerTrackSectionHints[kMixerTrackMenuRowCount] = {
    MAKE_FOURCC('F','M','I','X'), MAKE_FOURCC('C','R','S','H'),
    MAKE_FOURCC('E','Q','E','N'), MAKE_FOURCC('D','R','Y','_'),
    MAKE_FOURCC('T','B','L','A') } ;

// Numero de filas del menu activo.
inline int mixerMenuRowCount(bool masterMenu) {
    return masterMenu ? kMixerMasterMenuRowCount : kMixerTrackMenuRowCount ;
}

// Etiqueta de la fila solicitada del menu activo.
inline const char *mixerMenuLabel(bool masterMenu, int item) {
    return masterMenu ? kMixerMasterMenuLabels[item] :
                        kMixerTrackMenuLabels[item] ;
}

// Clamp golden de los valores editables (0..4 softclip, 0..1 clip gain).
inline int mixerMenuClampSoftclip(int idx) {
    if (idx < kMixerSoftclipMin) return kMixerSoftclipMin ;
    if (idx > kMixerSoftclipMax) return kMixerSoftclipMax ;
    return idx ;
}

inline int mixerMenuClampSoftclipGain(int idx) {
    if (idx < kMixerSoftclipGainMin) return kMixerSoftclipGainMin ;
    if (idx > kMixerSoftclipGainMax) return kMixerSoftclipGainMax ;
    return idx ;
}

// Codificado de accion del menu: filas master >= 2 -> pagina FX (1..4 =
// DELAY..COMP); filas track -> seccion del editor (101..105).  0 = ninguna
// (filas editables del menu master).
inline int mixerMenuActionForRow(bool masterMenu, int item) {
    if (masterMenu) {
        if (item >= 2) return item - 1 ;
        return 0 ;
    }
    return 101 + item ;
}

// Hint FourCC de la seccion TRACK (guardado con 0 fuera de rango).
inline unsigned int mixerMenuSectionHint(int section) {
    if (section < 0 || section >= kMixerTrackMenuRowCount) return 0 ;
    return kMixerTrackSectionHints[section] ;
}

#endif
