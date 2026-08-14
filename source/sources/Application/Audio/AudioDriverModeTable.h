#ifndef _AUDIO_DRIVER_MODE_TABLE_H_
#define _AUDIO_DRIVER_MODE_TABLE_H_

// F4a (docs/F4_ARCHITECTURE_ES.md): tabla declarativa de los backends de
// audio del driver UAC2 como datos puros.  Es el nucleo de datos del futuro
// AudioRouter (objetivo F4): cada entrada describe un modo del driver con
// su nombre, descripcion, token de modo, token de politica OTG, nombre de
// rama del daemon y sus capacidades declarativas de direccion.
//
// Replica golden byte-identica de las tablas switch que vivian en el
// puente UAC2 del adaptador TREEFROG (mode_name, mode_desc, mode_token,
// policy_token, branch_name_for_mode, selectable_mode, mode_has_out,
// mode_has_in y el conteo de la UI).  Los modos que dependen de la
// direccion del sampler (USB_OUT) se modelan con capacidades que la capa
// pura no resuelve: el llamador (el bridge) pasa la direccion del sampler
// como parametro.
//
// No depende de GUI, audio, Player, daemons, POSIX ni del framebuffer:
// solo tipos integrados de C++03.  Todo el comportamiento es byte-identico
// al que vivia en el bridge (golden Bacon 1.2.1).

// Ids golden de los modos del driver (mismos valores que el enum del
// bridge; el baseline F4a verifica el contrato de valores).
static const int kAudioDriverModeLocalConsole = 0;
static const int kAudioDriverModeWindows = 1;
static const int kAudioDriverModeAndroid = 2;
static const int kAudioDriverModeUsbOut = 3;
static const int kAudioDriverModeMidi = 4;
static const int kAudioDriverModeSp404In = 5;

// Conteo golden listado en la UI: el modal de driver itera 0..count-1 y
// SP404_IN (5) es un modo interno que no se lista ni se selecciona.
static const int kAudioDriverModeUiCount = 5;

// Entrada declarativa de un modo: identidad, etiquetas golden y
// capacidades.  Las capacidades de direccion son bases declarativas:
//   outBase/inBase: la direccion es un hecho del modo (WINDOWS=duplex).
//   outDir0/inDir1: la direccion sigue al toggle del sampler (USB_OUT).
struct AudioDriverModeInfo {
    int id;
    const char *name;    // mode_name golden
    const char *desc;    // mode_desc golden
    const char *token;   // mode_token golden (audio_driver_mode)
    const char *policy;  // policy_token golden (OTG)
    const char *branch;  // branch_name_for_mode golden (daemon)
    int selectable;      // selectable_mode golden (UI)
    int outBase;         // modo con salida USB fija (WINDOWS)
    int inBase;          // modo con entrada USB fija (WINDOWS/ANDROID/SP404_IN)
    int outDir0;         // modo cuyo OUT = (direccion sampler == 0)
    int inDir1;          // modo cuyo IN  = (direccion sampler == 1)
};

// Tabla golden de los 6 modos (indice por id == posicion).
static const AudioDriverModeInfo kAudioDriverModes[6] = {
    {0, "Local Console", "Console sound, OTG may stay connected",
     "LOCAL_CONSOLE", "LOCAL_CONSOLE", "audio_driver_local_console",
     /*selectable*/1, /*outBase*/0, /*inBase*/0, /*outDir0*/0, /*inDir1*/0},
    {1, "Windows", "Duplex UAC2 gadget (PC host)",
     "USB_DUPLEX", "USB_DUPLEX_OTG", "audio_driver_usb_duplex",
     1, 1, 1, 0, 0},
    {2, "Android", "Duplex UAC2 gadget (phone host)",
     "USB_IN", "USB_IN_OTG", "audio_driver_usb_in",
     1, 0, 1, 0, 0},
    {3, "Sampler", "SP404: console sound to sampler (EXT SOURCE)",
     "USB_OUT", "USB_OUT_OTG", "audio_driver_usb_out",
     1, 0, 0, 1, 1},
    {4, "MIDI", "MIDI: USB piano/controller",
     "MIDI", "MIDI_OTG", "audio_driver_midi",
     1, 0, 0, 0, 0},
    {5, "Sampler", "SP404 IN: sampler->console, recording only",
     "SP404_IN", "USB_OUT_OTG", "audio_driver_sp404_in",
     0, 0, 1, 0, 0},
};

// Fallback golden del switch default del bridge: cualquier id fuera de la
// tabla se comporta como LOCAL_CONSOLE en etiquetas pero no es seleccionable
// ni tiene capacidades de direccion.
static const AudioDriverModeInfo kAudioDriverModeFallback = {
    0, "Local Console", "Console sound, OTG may stay connected",
    "LOCAL_CONSOLE", "LOCAL_CONSOLE", "audio_driver_local_console",
    /*selectable*/0, /*outBase*/0, /*inBase*/0, /*outDir0*/0, /*inDir1*/0};

// Lookup declarativo por id (fallback para ids fuera de rango).
inline const AudioDriverModeInfo &AudioDriverModeInfoFor(int mode) {
    if (mode >= 0 && mode < 6) return kAudioDriverModes[mode];
    return kAudioDriverModeFallback;
}

// Replica golden de mode_name(mode).
inline const char *AudioDriverModeName(int mode) {
    return AudioDriverModeInfoFor(mode).name;
}

// Replica golden de mode_desc(mode).
inline const char *AudioDriverModeDescription(int mode) {
    return AudioDriverModeInfoFor(mode).desc;
}

// Replica golden de mode_token(mode) (escrito en audio_driver_mode).
inline const char *AudioDriverModeToken(int mode) {
    return AudioDriverModeInfoFor(mode).token;
}

// Replica golden de policy_token(mode) (escrito en la politica OTG).
inline const char *AudioDriverModePolicyToken(int mode) {
    return AudioDriverModeInfoFor(mode).policy;
}

// Replica golden de branch_name_for_mode(mode) (rama del daemon).
inline const char *AudioDriverModeBranchName(int mode) {
    return AudioDriverModeInfoFor(mode).branch;
}

// Replica golden de selectable_mode(mode).
inline int AudioDriverModeIsSelectable(int mode) {
    return AudioDriverModeInfoFor(mode).selectable;
}

// Replica golden de mode_has_out(mode) con la direccion del sampler como
// parametro (el llamador pasa la direccion del sampler).
inline int AudioDriverModeHasOut(int mode, int samplerDirectionIn) {
    const AudioDriverModeInfo &m = AudioDriverModeInfoFor(mode);
    if (m.outDir0) return samplerDirectionIn == 0;
    return m.outBase;
}

// Replica golden de mode_has_in(mode) con la direccion del sampler como
// parametro (el llamador pasa la direccion del sampler).
inline int AudioDriverModeHasIn(int mode, int samplerDirectionIn) {
    const AudioDriverModeInfo &m = AudioDriverModeInfoFor(mode);
    if (m.inDir1) return samplerDirectionIn == 1;
    return m.inBase;
}

// Replica golden del conteo de la UI del driver (GetDriverModeCount).
inline int AudioDriverModeCount(void) {
    return kAudioDriverModeUiCount;
}

#endif
