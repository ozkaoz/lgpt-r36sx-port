PORT LGPT V2 - Estable probada

Port de LittleGPTracker/LGPT para R36SX V2.6 sobre TreeFrogUI.

Estado:
- Release estable probada en consola R36SX.
- No requiere carpeta roms/gme.
- El port debe trabajar desde la carpeta lgpt de la SD.

Layout validado en Windows:
F:\lgpt
F:\lgpt\projects
F:\lgpt\samples
F:\lgpt\instruments

Layout runtime validado en TreeFrog/Linux:
ROOTFOLDER=/mnt/sdcard/lgpt
SAMPLELIB=/mnt/sdcard/lgpt/samples
INSTRUMENTFOLDER=/mnt/sdcard/lgpt/instruments

Estado funcional validado:
- START reproduce y detiene correctamente.
- A + flechas funciona.
- Listen funciona.
- Import funciona correctamente.
- Los samples se cargan desde F:\lgpt\samples.
- Los samples importados funcionan en Phrase.
- Exit funciona sin congelar.
- Phrase y Table muestran bien la selección.
- Los campos 0000 se muestran completos.
- Inicia directamente en projects.

Estado visual validado:
- Fondo púrpura correcto.
- P G / SCPI / TT tienen bloque de fondo.
- Selección de Song correcta.
- Barras inferiores visibles.
- Selección del mapa restaurada a semántica clásica CD_HILITE2 + invert_.

Notas:
- Esta release se congela como base estable para continuar refactor UI controlado.
- No tocar Player/audio/input hasta terminar refactor UI.
- Un cambio por vez: parche mínimo, compilar, probar en R36SX y recién después empaquetar.
