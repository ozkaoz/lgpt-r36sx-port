# R36SX Windows Host Helper (AU11M)

Objetivo: aproximar el comportamiento tipo SP-404MKII en Windows. Windows debe ver la R36SX como USB Audio dúplex, pero el host necesita aplicar dos rutas:

1. salida predeterminada de Windows -> `Conector AUX interno / R36SX USB Audio` para enviar audio del PC hacia la consola y samplear;
2. entrada predeterminada de Windows -> `Micrófono / R36SX USB Audio`;
3. monitorización de ese micrófono hacia una salida física real del PC, por ejemplo BEHRINGER, parlantes o audífonos. No debe monitorizarse hacia R36SX porque eso crea bucle.

Windows no permite que el dispositivo USB Audio cambie por sí solo estas preferencias del host. Por eso este helper corre en Windows y aplica el perfil cuando detecta `R36SX USB Audio`.

Dependencia: SoundVolumeView.exe o svcl.exe de NirSoft, colocado en esta misma carpeta o instalado en PATH. No se incluye el binario.

Flujo recomendado:

1. Ejecutar `R36SX_LIST_AUDIO_DEVICES.cmd`.
2. Abrir `r36sx_audio_devices.csv` y copiar el `Command-Line Friendly ID` de:
   - R36SX Render/Speakers;
   - R36SX Capture/Microphone;
   - salida física de monitorización: BEHRINGER, Speakers, Headphones, etc.
3. Editar `R36SX_AUDIO_PROFILE_CONFIG.cmd`.
4. Ejecutar `R36SX_APPLY_AUDIO_PROFILE.cmd`.
5. Si funciona, ejecutar `R36SX_INSTALL_WATCHER_TASK.cmd` para que se aplique al iniciar sesión.

Notas:

- La salida predeterminada puede ser R36SX para enviar audio del PC a la consola.
- La escucha del micrófono R36SX debe salir por una interfaz física distinta, no por R36SX.
- La consola no puede ejecutar este helper automáticamente por USB-C sin instalar previamente algo en Windows. AutoRun no es una solución segura ni aplicable a USB Audio.
