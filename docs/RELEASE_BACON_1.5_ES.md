Pre-release Bacon 1.5 - Sinths and EQ8 (build U2.52.3, 2026-08-18)

Build:
- lgpt_r36sx_u2523.so - SHA256 097bd4d36fb24461f83428ec007378a68e3baa6bcd291e49fc05dec209736432
- r36s_u2523_usb_audio_io (daemon USB UAC2) - f7140072... (byte-identico al anterior)
- r36s_sp404_host_audio_io - 968dfa61...
- r36s_midi_host_io - 3f0ea7a2...
- LGPT_R36SX_Bacon-1.5_SD_ROOT.zip (payload copy-to-SD-root)

Novedades Bacon 1.5:

1. Menú Sinth (Bass/Piano) en el navegador: Listen/Import/Manage/Sinth/Exit
   (item 9 + feedback de consola).
2. Preescucha con B en las páginas Bass/Piano (Player::PreviewNote/StopPreview/
   UpdatePreview con arm/desarm TreeFrogAudioSetPlaybackArmed, auto-stop ~0.9 s,
   cancelada por Start/Stop y por segundo B).
3. EQ8 gráfico fullscreen por instrumento (InstrumentEqView, reemplaza el modal;
   curva dibujada con los MISMOS coeficientes GetBandCoeffs que procesa el DSP;
   acepta IT_SYNTH/IT_PIANO; PLAYBACK fuera del editor, parámetros en el modelo).
4. Conversión de slot a sintetizador vía InstrumentView::ConvertCurrentToSynth
   (mismo protocolo seguro que el selector src: detach del observer, Stop,
   SetInstrumentType, rebuild; bloquea con sample asignado).
5. Menú TRACK del Mixer salta a EQ8.
6. Audición aislada: el preview suena por un canal/bus propios (AuditionService)
   aunque la pista esté muteada o a volumen 0; StopPreview nunca toca los 8
   PlayerChannel (aislamiento verificado por test estático).
7. SpectrumAnalyzer dirigido común: tap post-EQ/pre-gain en PlayerChannel y en
   la audición, solo cuando está armado y el instrumento es el objetivo.
8. Guard de estabilidad RBJ bell en EqBiquad.h: el peaking de la receta RBJ
   diverge con boost a baja frecuencia (verificado: +6 dB @1 kHz Q1, +2 dB
   @250 Hz Q1, +12 dB @80 Hz Q1 explotaban); el boost queda limitado al 90%
   del valor marginal A = sw/(sw-4Q(1-cw)) y el filtro siempre acota la salida.
   Aplica a InstrumentEq (EQ8) y a ParametricEQ (master EQ).

Fixes:

- Fix FAST_MATH (audio estable): sinf/powf/expf fuera del bucle por muestra de
  los sintetizadores (tablas interpoladas compartidas SynthMath.h: seno 1024 +
  2^x 256; glide hoisteado bit-identical; pan con tabla). Sin cambios de sonido
  perceptibles (error < 1e-6), cero libm por muestra.
- Fix DIALOGO PROYECTOS (crash.txt #1-#5): los 5 SIGSEGV del dispositivo eran
  NULL derefs de SelectProjectDialog (lista vacía / cursor stale) — OnRenameProject
  (dumps 1-3, core viejo) y ProcessButtonMask A→Load (dump 5, core nuevo).
  CrashTrap hacía _exit(128+sig) y picoarch reiniciaba el core → el audio moría
  ("el instrumento dejó de sonar"). Guards TREEFROG_DIALOG_NULL_GUARD_V1:
  current==0 en A→Load, ruta vacía en OnRenameProject + scan de un solo pase,
  clamp a 0 en warpToNextProject con lista vacía.
- CrashTrap: formatter hex corregido ("0x" antes de los dígitos) para que los
  dumps sean parseables.
- H38.8 hilo de audio: scratch estático en AudioMixer::Render (sin malloc por
  buffer), pow(x,3)→x*x*x en softclip, fmodf del square de BassSynth→rama.
- InstrumentEq: estados biquad por banda y por canal (la cascada ya no comparte
  estado: antes el orden de las bandas cambiaba el sonido ~23k LSB); smoothing
  con snap exacto (antes se quedaba a 63 LSBs del objetivo sin limpiar el flag).

Regresión: AUDIT_CLEAN_MAIN_U2523_OK (tests/run_all.sh), F10_BASELINE_OK
(golden core 097bd4d3), host bass 45 + piano 46 + eq8_struct 31 +
analyzer_target 54 checks OK, TEST_FX_PHASE19_AUDITION_ISOLATED_OK,
DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK, verify ERRORS=0.