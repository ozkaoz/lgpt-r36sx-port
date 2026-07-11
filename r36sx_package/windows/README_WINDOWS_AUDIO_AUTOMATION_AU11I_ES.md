# AU11M - Automatización opcional de audio en Windows

La R36SX como dispositivo USB Audio no puede activar por sí sola `Escuchar este dispositivo`, cambiar el dispositivo predeterminado de Windows ni ejecutar scripts al conectarse. Windows separa los endpoints de reproducción y grabación; el endpoint de grabación de la R36SX debe monitorizarse hacia una salida real del PC para escuchar el proyecto.

Flujo recomendado:

1. `Micrófono (R36SX USB Audio)` = entrada desde la consola hacia Windows.
2. Activar `Escuchar este dispositivo` sobre ese micrófono.
3. `Reproducir mediante este dispositivo` debe apuntar a parlantes/audífonos/BEHRINGER, no a R36SX.
4. `Conector AUX interno (R36SX USB Audio)` = salida de Windows hacia la consola para USB-C RECORD.

No se incluye ningún EXE de terceros. Para automatizar desde Windows se puede usar SoundVolumeView/SVCL de NirSoft o un helper propio en C#/WASAPI. Los archivos CMD incluidos son plantillas: edite los nombres exactos de sus dispositivos según aparecen en `mmsys.cpl` o en SoundVolumeView.

Modo `sampler`:
- Activa escucha del micrófono R36SX hacia sus parlantes/Behringer.
- Pone la salida predeterminada en `Conector AUX interno R36SX` para enviar audio del PC hacia la consola.

Modo `monitor_project`:
- Mantiene escucha del micrófono R36SX hacia sus parlantes/Behringer.
- Devuelve la salida predeterminada de Windows a sus parlantes/Behringer.

