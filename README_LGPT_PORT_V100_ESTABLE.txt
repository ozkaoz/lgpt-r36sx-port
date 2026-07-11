LGPT PORT V 100_Estable

Release estable probada del port de LittleGPTracker/LGPT para R36SX V2.6 sobre TreeFrogUI.

Contenido:
- Código fuente completo del port.
- Core compilado listo para instalar:
  dist/lgpt_libretro.so
- Runtime base:
  dist/lgpt/config.xml
  dist/lgpt/projects
  dist/lgpt/samples
  dist/lgpt/instruments
- Prompt actualizado para continuar desarrollo:
  PROMPT_CONTINUAR_DESARROLLO_CHATGPT.txt
- Script de build con badge desactivado:
  BUILD_TREEFROG_R36SX_BADGE_OFF.sh

Instalación manual en SD:
- Copiar dist/lgpt_libretro.so a:
  F:\cubegm\cores\lgpt_libretro.so
- Copiar dist/lgpt\config.xml a:
  F:\lgpt\config.xml
- Mantener:
  F:\lgpt\projects
  F:\lgpt\samples
  F:\lgpt\instruments

No usar ni recrear:
- F:\roms\gme

Estado validado:
- Inicia directamente en projects.
- No muestra marca V/version badge arriba a la derecha.
- START reproduce/detiene correctamente.
- A + flechas funciona.
- Listen funciona.
- Import funciona.
- Exit funciona.
- Project Exit menu está en inglés.
- Song mantiene fondo púrpura, P G / SCPI / TT, selección y barras inferiores.
- Groove, Chain, Phrase y Table tienen selección visual consistente con Song.
- Los samples cargan desde F:\lgpt\samples.
