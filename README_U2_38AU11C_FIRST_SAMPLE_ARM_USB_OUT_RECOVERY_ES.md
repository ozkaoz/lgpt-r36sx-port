# U2.38AU11M — LGPT R36SX USB Sampler: first-sample safe + USB out recovery

Versión de continuidad para **Desarrollo LGPT Saber**.

Objetivo funcional:

- `LOCAL_ONLY`: audio solo por la consola.
- `USB_OUT_AUTO_MUTE`: LGPT sale hacia Windows por USB y mutea la consola.
- `USB-C RECORD / SCPI-R`: Windows/celular entra a la consola por USB-C para preescucha local, medidor L/R y grabación WAV.

Decisiones AU11M:

1. El crash de primera apertura en `Instrument -> sample -> A` se trata como una carrera de parada de transporte + construcción fría de `ImportSampleDialog`. Ahora, si el proyecto está reproduciendo o hay preview activo, el primer `A` solo detiene/arma el acceso. El segundo `A` abre `Listen / Import / Manage / Exit`. Si el proyecto ya está detenido, abre directamente.
2. Al salir de `USB-C RECORD`, el core manda `MONITOR=0` + `RECOVER_OUT=1` al daemon. El daemon cierra el PCM de reproducción USB, limpia el ring y lo reabre, para recuperar LGPT -> Windows cuando Windows mantiene habilitados reproducción y grabación.
3. `USB-C RECORD` sigue entrando desde `Instrument + R1 + Derecha` y sale con `R1 + Izquierda` hacia `Instrument`.
4. `Chopper + L2 + A` sigue deshabilitado.
5. El paquete conserva código fuente completo, scripts WSL, changelog, notas de continuidad y protocolo de prueba.

## Compilar e instalar

```bash
cd "/mnt/d/R36S/PORT LPTRACKER"
rm -rf U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_FULL_SOURCE
rm -rf /tmp/r36s_u2_38au11c
unzip -o U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_FULL_SOURCE.zip
cd U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_FULL_SOURCE
chmod +x bin/*.sh device/*.sh
bash bin/00_RUN_AU11_FULL_CLEAN_FROM_WSL.sh \
  "/mnt/d/R36S/PORT LPTRACKER" \
  F \
  "/tmp/r36s_u2_38au11c" \
  2>&1 | tee "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11U_FULL_RUN_$(date +%Y%m%d_%H%M%S).log"
```

Debe terminar con:

```text
SUMMARY=PASS_AU11_FULL_CLEAN
CORE_1_CMP=0
CORE_2_CMP=0
DAEMON_CMP=0
SUMMARY=PASS_AU11_SD_VERIFY
```

## Sacar logs

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_FULL_SOURCE"
bash bin/04_COLLECT_AU11_LOGS_FROM_SD_FROM_WSL.sh \
  F \
  "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11U_TEST_LOGS_$(date +%Y%m%d_%H%M%S).zip"
```

## Marcadores esperados

```text
U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG
AU11U_FIRST_SAMPLE_A_ARM_ONLY
AU11U_USB_OUT_RECOVERY_AFTER_MONITOR_OFF
AU11U_PCM_PLAY_FORCE_REOPEN_AFTER_USB_REC_EXIT
```
