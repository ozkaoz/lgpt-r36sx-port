# R36SX TreeFrogUI refactor plan

Base estable:
PORT LGPT V2 - Estable probada

Regla de trabajo:
- Un cambio por vez.
- Primero parche/refactor mínimo.
- Luego compilar.
- Luego probar en R36SX.
- Luego empaquetar si queda estable.
- No tocar Player/audio/input durante el refactor UI.
- No recrear ni depender de F:\roms\gme.
- Mantener runtime en F:\lgpt.

Layout runtime validado:
- Windows: F:\lgpt
- Windows: F:\lgpt\projects
- Windows: F:\lgpt\samples
- Windows: F:\lgpt\instruments
- TreeFrog/Linux: /mnt/sdcard/lgpt
- TreeFrog/Linux: /mnt/sdcard/lgpt/samples
- TreeFrog/Linux: /mnt/sdcard/lgpt/instruments

Estado visual validado:
- Fondo púrpura correcto.
- P G / SCPI / TT con bloque de fondo.
- Selección de Song correcta.
- Barras inferiores visibles.
- Phrase y Table correctos.
- Listen / Import / Exit correctos.
- Selector inicial abre directamente en projects.

Archivos UI de mayor riesgo:
- sources/Application/Views/BaseClasses/View.cpp
- sources/Application/Views/SongView.cpp
- sources/Application/Views/PhraseView.cpp
- sources/Application/Views/TableView.cpp
- sources/Application/Views/ProjectView.cpp
- sources/Application/Views/BaseClasses/UIBigHexVarField.cpp
- sources/Application/Views/ModalDialogs/SelectProjectDialog.cpp
- sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp
- sources/Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.cpp
- sources/Application/AppWindow.cpp

No tocar todavía:
- sources/Application/Player/Player.cpp
- sources/Adapters/TREEFROG/Audio/TreeFrogAudioDriver.cpp
- sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp
- input handling
- audio lifecycle

Parches UI validados que deben preservarse:
- TreeFrogGUIWindowImp.cpp mantiene semántica clásica de invert:
  glyph = backgroundColor_
  cell/background = currentColor_
- View::drawMap() usa bloques invertidos para P G / SCPI / TT.
- View::drawNotes() muestra barras inferiores con props.invert_.
- SongView mantiene selección visible.
- SelectProjectDialog inicia en root:projects.
- ImportSampleDialog conserva preview con StartStreaming.
- UIBigHexVarField conserva visualización completa de 0000.

Primeros refactors recomendados:
1. Solo documentación y auditoría de marcadores.
2. Limpieza de comentarios redundantes en archivos UI, sin cambiar líneas ejecutables.
3. Agrupar marcadores TreeFrog UI bajo nombres coherentes.
4. Extraer constantes visuales solo si no cambia comportamiento.
5. Recién después revisar helpers de dibujo UI.
