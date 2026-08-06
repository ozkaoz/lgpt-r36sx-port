# Release: Bacon 1.2.1 - Chopper UAF Hardening

## Actualización (pre-release): EQ de 8 bandas + Spectrum en vivo

- EQ gráfico de **8 bandas por instrumento de sample** (bell / low shelf /
  high shelf / low pass / high pass / notch, respuesta RBJ), con máscara de
  bandas por pad y bypass global. Parámetros por `SetInstrument`.
- Analizador de espectro en vivo (FFT) sobre el overlay del modal.
- Actívandolo en la vista de instrumento: campo **EQ 8-B** / `SIP_EQEN`.
- La pre-release `Bacon-1.2.1` se mantiene como pre-release con estos assets.

## Resumen

Release de estabilidad sobre **Bacon 1.2 (Mixer Dev)**. Corrige el crash del
chopper (crop/delete/pitch/normalizar/undo/redo con reproducción activa) que
se reproducía como *use-after-free* del buffer de sample compartido: el
`WavFile::ReplaceBuffer()` libera la memoria del WAV mientras una voz del
patrón o el stream de preview aún la referencian.

## Qué incluye

- **Guard zombie de voces** (`SampleInstrument::Render`): si el buffer que una
  voz cacheó al dispararse ya no coincide con el buffer actual de la fuente,
  la voz se termina en silencio en lugar de leer memoria liberada. Es la
  última línea de defensa del UAF.
- **Parada de audio completa en todas las ediciones destructivas del chopper**:
  Crop y Delete ya la aplicaban; ahora también `Undo/Redo` (L1+X/R1+X),
  `Pitch/Env Apply` (A) y `Normalizar` (R2+Y) detienen el patrón
  (`Player::Stop()`) y el streaming de preview (`StopStreaming()`) antes de
  reemplazar el buffer.
- **Build 100% limpio**: corregido el warning `-Wreorder` preexistente del
  constructor de `SampleChopperModal` (orden de inicialización de
  `sampleName_`/`undoHistoryCount_`/`redoHistoryCount_`/`splitParts_`).

## Diagnóstico que motivó el fix

1. **Crash de crop con playback** (reportado en Bacon 1.2): reproducido por el
   usuario; el port dejaba de arrancar después (pantalla negra).
2. **Instrumentación temporal** (trazas `boot_steps.log` y
   `chopper_debug.log` en `LGPT_OTG_LOGS/`): confirmó que el boot del core es
   íntegro (`retro_load_game → Boot → Init → LoadProject → SamplePool →
   Persistency parsed-ok → retro_run frame0`) y que el SIG=11 f0 posterior al
   crash era un estado residual del launcher/emulador (securizado con
   reinicio completo), no del core.
3. Las trazas confirmaron que CROP/PITCH/UNDO/REDO se ejecutaban con
   `running=1` (patrón sonando) durante las ediciones destructivas: exactamente
   la condición del UAF. La instrumentación se retiró en este release.

## Verificación

- Build: 0 warnings, 0 errores (`BUILD_U2523_OK`).
- Core SHA256: `f01b2578d611acee69594634c2ffcc284572dcf3bb3bb170dea7f6858f4d8dc8`.
- Prueba en consola (R36S): sesión de varios minutos con crop/delete/pitch/
  undo/redo y reproducción activa sin crash.
- Los proyectos de usuario (`lgpt/projects/lgpt_KaOz`) se cargan y guardan
  correctamente (fallback `lgptsav_tmp.dat` si `lgptsav.dat` está corrupto).

## Nota SD

`F:\lgpt\project` y `F:\lgpt\samplelib` se crean a propósito en cada arranque
por el launcher (`lgpt_launcher_u241.sh`, "runtime requirements"; Git y ZIP no
retienen carpetas vacías). No son basura de este release.
