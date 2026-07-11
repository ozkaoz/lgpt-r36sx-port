# LGPT R36SX - Camilo Peña AU11Z6 Stable Source

Repositorio fuente para el port de LittleGPTracker/LGPT en R36SX dentro de la línea de trabajo **desarrollo LGPT CAMILO PEÑA**.

## Estado de esta base

Base marcada como referencia estable según las pruebas del chat:

- ZIP fuente de referencia: `U2_38AU11Z6_CAMILO_PENA_WSL24_MARKERWRITEFIX_FULL_SOURCE.zip`
- Nombre técnico: `AU11Z6 = STABLE CORE / SAMPLER NO-CRASH BASE`
- Entorno de trabajo: WSL Ubuntu 24 + Windows + SD R36SX
- Objetivo de esta rama: conservar el core estable y continuar el desarrollo OTG en una rama separada.

## Qué incluye este repositorio

- Código fuente completo de LGPT/R36SX.
- Recursos, proyectos, librerías y scripts necesarios para compilar el port.
- Scripts de instalación/prueba AU11Z6 en `r36sx_package/`.
- Documentación de continuidad, changelog y notas para GitHub.

## Qué NO incluye

- Backups completos de la SD.
- Logs de prueba.
- Builds generados.
- Zips de release anteriores.
- Archivos RAR pesados.

## Regla de desarrollo desde esta base

No modificar inicialmente estas zonas mientras se desarrolle el puerto OTG:

- `InstrumentView`
- `ImportSampleDialog`
- flujo de carga de samples
- audio callback crítico

La integración OTG debe desarrollarse primero como sidecar externo y validarse fuera del core estable. Solo después debe integrarse al port.

## Compilación base AU11Z6

Desde WSL Ubuntu 24, con la SD montada como `F:`:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/LGPT_R36SX_CAMILO_PENA_AU11Z6_STABLE_GITHUB_SOURCE"
chmod +x r36sx_package/*.sh r36sx_package/wsl_scripts/*.sh r36sx_package/device_scripts/*.sh

bash r36sx_package/00_WSL_UBUNTU24_FLUJO_COMPLETO_AU11Z6.sh \
  "/mnt/d/R36S/PORT LPTRACKER" \
  F \
  /tmp/r36s_u2_38au11z6
```

La compilación/instalación debe terminar con verificación de core y daemon sin diferencias binarias.

## Continuación del OTG

Crear una rama nueva desde este commit/base:

```bash
git checkout -b otg-sidecar-from-au11z6
```

Trabajar primero en:

- `r36sx_package/device_scripts/`
- `r36sx_package/wsl_scripts/`
- daemon/PCM/FIFO como proceso externo
- configfs/UDC/UAC2 sin tocar sampler ni menús críticos

