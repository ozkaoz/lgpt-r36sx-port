LGPT PORT V101_InputReserved_Estable

Release estable probada del port de LittleGPTracker/LGPT para R36SX V2.6 sobre TreeFrogUI.

Base:
LGPT PORT V 100_Estable

Cambios principales desde V100:
- Se agregaron símbolos internos EPBT_X / EPBT_Y / EPBT_L2 / EPBT_R2.
- Se agregaron máscaras internas EPBM_X / EPBM_Y / EPBM_L2 / EPBM_R2.
- X dejó de duplicar A y queda como botón dedicado/inactivo.
- Y dejó de duplicar B y queda como botón dedicado/inactivo.
- L2 dejó de duplicar L1/L y queda como botón dedicado/inactivo.
- R2 dejó de duplicar R1/R y queda como botón dedicado/inactivo.
- Select sigue implementado, pero inactivo con TREEFROG_ENABLE_SELECT=0.

Estado validado:
- TreeFrogUI muestra LGPT.
- LGPT abre directamente en projects.
- Carga proyectos.
- A carga proyecto.
- X ya no carga proyecto.
- B funciona como antes.
- Y no hace nada visible.
- L1/R1 conservan comportamiento.
- L2/R2 no hacen nada visible.
- Select no hace nada visible.
- START reproduce/detiene.
- Song, Groove, Chain, Phrase, Table, Listen, Import y Exit funcionan.
- No aparece F:\roms\gme.

Instalación manual:
- Copiar dist/lgpt_libretro.so a:
  F:\cubegm\cores\lgpt_libretro.so
- Copiar o mantener dist/lgpt/config.xml en:
  F:\lgpt\config.xml
- Mantener:
  F:\lgpt\projects
  F:\lgpt\samples
  F:\lgpt\instruments
- Mantener launcher TreeFrogUI:
  F:\roms\lgpt
  F:\cubegm\lgpt
  F:\cubegm\lgpt_wrapper.sh
- No crear ni depender de:
  F:\roms\gme
