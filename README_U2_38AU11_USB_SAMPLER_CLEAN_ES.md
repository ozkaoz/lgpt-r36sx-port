# U2.38AU11 USB Sampler Clean

Objetivo: dejar el port LGPT para R36SX con lógica de consola-sampler:

- `LOCAL_ONLY`: audio por consola; USB puede seguir enumerado, pero LGPT no envía audio al host.
- `USB_OUT_AUTO_MUTE`: LGPT envía audio al host por USB y mutea consola. El gadget físico sigue siendo dúplex, pero la entrada se usa solo desde `USB-C RECORD`.
- `USB-C RECORD / SCPI-R`: entrada directa desde PC/celular por USB-C hacia la consola, con preescucha local en tiempo real y salida USB de LGPT cerrada mientras se graba/monitoriza.

Cambios AU11:

1. Se elimina `FULL_DUPLEX` como opción visible porque duplicaba `USB_OUT_AUTO_MUTE`. El gadget sigue siendo dúplex internamente.
2. `Instrument + R1 + RIGHT` abre `USB-C RECORD` como ruta oculta `SCPI-R`.
3. Se desactiva la ruta antigua `Chopper + L2 + A` hacia USB REC.
4. Al entrar a `Sample`, `Listen`, `Import`, `Manage`, `Chopper` o `USB-C RECORD`, se detiene transporte/streaming antes de tocar memoria de samples.
5. El render de audio mezcla el monitor USB aun con el proyecto detenido; esto habilita la preescucha local real.
6. El menú USB-C RECORD ya no queda bloqueado esperando release artificial; el FIFO del monitor se abre lazy desde audio render, no desde UI.
