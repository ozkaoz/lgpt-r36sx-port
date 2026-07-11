# AU11Z2 — Nota crítica WSL Ubuntu 24

Si aparece `SUMMARY=FAIL_WINDOWS_CLEAN_NEEDS_ADMIN`, no es un error de compilación: Windows exige administrador para eliminar dispositivos USB previos. Use `00_WSL_UBUNTU24_FLUJO_COMPLETO_AU11Z2.sh` o primero ejecute `00_WSL_UBUNTU24_PASO_0_LIMPIAR_WINDOWS_ADMIN_AU11Z2.sh`.

---

# Nota AU11Z1 — WSL Ubuntu 24 path fix

Si el comando `cd /mnt/d/R36S/PORT LPTRACKER/U2_38AU11Z_CAMILO_PENA_CLEAN_RECONCILED_OTG_FULL_SOURCE` falla, la carpeta no está extraída en esa ruta. Use primero `00_LEER_PRIMERO_WSL_UBUNTU24_AU11Z1_PATHFIX.md` y ejecute `bash 00_WSL_UBUNTU24_COMPILAR_E_INSTALAR_AU11Z1.sh ...`.

# desarrollo LGPT CAMILO PEÑA — U2.38AU11Z

## Objetivo del paquete

Port de LittleGPTracker/LGPT para R36SX con base estable AU11U, descriptor USB Audio AU10Y y limpieza obligatoria antes de cada prueba.

Esta versión no intenta avanzar todavía a grabación WAV real. Primero congela dos puntos que ya se habían resuelto por separado:

1. `Instrument -> sample -> doble A` no debe crashear.
2. Windows debe detectar `R36SX USB Audio` en reproducción y grabación por USB-C OTG.

## Decisión técnica

AU11Z parte de AU11Y, pero endurece el protocolo de prueba:

- limpia residuos WSL/Ubuntu 24 antes de compilar;
- mueve logs/pruebas previas del host a backup;
- limpia SD sin borrar módulos kernel `.ko`;
- preserva/restaura `/lgpt/otg/modules` si existen;
- reemplaza WAV inválidos de 64 bytes o menos por WAV silencioso válido;
- limpia runtime OTG y binarios AU9/AU10/AU11 anteriores;
- incluye limpieza de caché de dispositivos Windows R36SX/VID_1209 mediante PowerShell/PnPUtil;
- reinstala core AU11U estable y descriptor AU10Y (`idProduct=0x38EA`, `p_chmask=1`, `c_chmask=1`).

## Comando único recomendado desde WSL Ubuntu 24

```bash
cd "/mnt/d/R36S/PORT LPTRACKER"

rm -rf U2_38AU11Z_CAMILO_PENA_CLEAN_RECONCILED_OTG_FULL_SOURCE
rm -rf /tmp/r36s_u2_38au11z

unzip -o U2_38AU11Z_CAMILO_PENA_CLEAN_RECONCILED_OTG_FULL_SOURCE.zip

cd U2_38AU11Z_CAMILO_PENA_CLEAN_RECONCILED_OTG_FULL_SOURCE
chmod +x bin/*.sh device/*.sh

bash bin/00_RUN_AU11Z_FULL_CLEAN_FROM_WSL.sh \
  "/mnt/d/R36S/PORT LPTRACKER" \
  F \
  "/tmp/r36s_u2_38au11z" \
  remove \
  2>&1 | tee "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11Z_FULL_RUN_$(date +%Y%m%d_%H%M%S).log"
```

Si PowerShell informa `FAIL_WINDOWS_CLEAN_NEEDS_ADMIN`, ejecutar en Windows como administrador:

```text
windows\R36SX_AU11Z_WINDOWS_CLEAN_ADMIN.cmd
```

Después repetir el comando WSL o, como mínimo, no conectar USB-C hasta haber limpiado Windows.

## Condición mínima para probar en consola

No probar si no aparecen estas líneas:

```text
SUMMARY=PASS_AU11_SD_VERIFY
CORE_1_CMP=0
CORE_2_CMP=0
DAEMON_CMP=0
SUMMARY=PASS_AU11Z_CAMILO_FULL_CLEAN
```

Si aparece `WARN_MODULES_NOT_RESTORED_USB_AUDIO_MAY_FAIL=YES`, la navegación puede probarse, pero el USB Audio probablemente no enumerará. Antes de eliminar la SD, exportar módulos con:

```bash
bash bin/00_EXPORT_SD_MODULES_TO_PACKAGE_CACHE.sh F
```

## Prueba mínima obligatoria

1. Apagar completamente la R36SX.
2. Encender sin conectar USB-C.
3. Abrir LGPT.
4. Cargar proyecto.
5. Ir a `Instrument -> sample`.
6. Pulsar `A`, esperar, pulsar `A` otra vez.
7. Debe abrir `Listen / Import / Manage / Exit` sin crashear.
8. Solo después conectar USB-C al PC.
9. Esperar 10 segundos.
10. Windows debe mostrar `R36SX USB Audio` en reproducción y grabación.

No probar grabación WAV todavía. No ejecutar Host Helper todavía.

## Recolección de logs

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11Z_CAMILO_PENA_CLEAN_RECONCILED_OTG_FULL_SOURCE"

bash bin/04_COLLECT_AU11_LOGS_FROM_SD_FROM_WSL.sh \
  F \
  "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11Z_TEST_LOGS_$(date +%Y%m%d_%H%M%S).zip"
```

## Archivos clave

- `bin/00_RUN_AU11Z_FULL_CLEAN_FROM_WSL.sh`: limpieza Windows/WSL/SD + build + instalación + verificación.
- `bin/00_CLEANROOM_SD_WSL_PREINSTALL_AU11Z.sh`: limpieza fuerte no destructiva de SD/WSL.
- `tools/windows-clean/R36SX_CLEAN_PRETEST_AUDIO_DEVICE_CACHE.ps1`: limpieza/listado de dispositivos Windows.
- `device/otg_38au11_common.sh`: descriptor AU10Y estable.
- `source_full/`: fuente completa usada para compilar.
- `docs/CONTINUE_PROMPT_AU11Z_CAMILO_PENA_ES.md`: prompt para continuar si se cae el chat.

## AU11Z3 note

If AU11Z2 failed with `command substitution: unexpected EOF while looking for matching "`, use `00_WSL_UBUNTU24_FLUJO_COMPLETO_AU11Z3.sh`. This package fixes the Bash/PowerShell quoting issue in the Windows-admin-clean launcher. `sudo` is not a replacement for Windows administrator elevation.

## AU11Z6 marker write fix

AU11Z6 corrige el fallo de instalación en WSL/Ubuntu 24:

```text
/mnt/f/lgpt/otg/au11_active_usb_profile: Invalid argument
```

No es fallo de compilación. La compilación puede pasar con `SUMMARY=PASS_FOR_AU11_INSTALL`; AU11Z6 evita que un marcador opcional de perfil corte el proceso antes de verificar SD y comparar binarios.
