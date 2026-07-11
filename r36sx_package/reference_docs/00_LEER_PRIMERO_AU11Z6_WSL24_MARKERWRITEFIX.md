# AU11Z6 — Comandos WSL Ubuntu 24

Ejecutar desde Ubuntu 24/WSL:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER"
rm -rf U2_38AU11Z6_CAMILO_PENA_WSL24_MARKERWRITEFIX_FULL_SOURCE
rm -rf /tmp/r36s_u2_38au11z6
unzip -o U2_38AU11Z6_CAMILO_PENA_WSL24_MARKERWRITEFIX_FULL_SOURCE.zip
cd U2_38AU11Z6_CAMILO_PENA_WSL24_MARKERWRITEFIX_FULL_SOURCE
chmod +x *.sh bin/*.sh device/*.sh

bash 00_WSL_UBUNTU24_FLUJO_COMPLETO_AU11Z6.sh \
  "/mnt/d/R36S/PORT LPTRACKER" \
  F \
  /tmp/r36s_u2_38au11z6 \
  2>&1 | tee "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11Z6_TERMINAL_$(date +%Y%m%d_%H%M%S).log"
```

Si vuelve a aparecer `Invalid argument` sobre `/mnt/f/lgpt/otg`, ejecutar diagnóstico:

```bash
bash 00_WSL_UBUNTU24_DIAGNOSTICAR_SD_INVALID_ARGUMENT_AU11Z6.sh F \
  2>&1 | tee "/mnt/d/R36S/PORT LPTRACKER/U2_38AU11Z6_SD_INVALID_ARGUMENT_DIAG_$(date +%Y%m%d_%H%M%S).log"
```
