# F5 - Politica de storage/SD estricta

Tramo del refactor `refactor/bacon-1.2.1-preserve` (golden Bacon 1.2.1,
core MIPS byte-identico `7709b665`).

## Objetivo

Toda escritura en la SD del R36SX debe pertenecer a uno de exactamente
tres tipos, y nada nuevo puede escribir fuera de ellos:

| Tipo       | Que contiene                                              | Dondé                         |
|------------|-----------------------------------------------------------|-------------------------------|
| Volatile   | cache/tmpfs que nunca llega a la SD                       | `/tmp/r36sx_*`, fifos, locks  |
| Persistent | configuracion y datos de usuario del core                 | `/mnt/sdcard/lgpt/*`          |
| Diagnostic | logs de diagnostico, fuera de la raiz de datos            | `/mnt/sdcard/LGPT_OTG_LOGS`   |

## Que se hizo

`source/sources/Services/Storage/StoragePolicy.h` (capa pura header-only
C++03, sin ninguna dependencia):

- `StoragePolicyRoot()`: la raiz unica del core (`/mnt/sdcard/lgpt`), la
  misma que TreeFrogSystem instala como alias `bin`.  Una sola fuente de
  verdad para core y daemons.
- `StoragePolicyClassify(path)`: clasifica cualquier ruta en
  Volatile/Persistent/Diagnostic, o `-1` si esta fuera de los tres tipos
  (escritura nueva no permitida).  Con fronteras de segmento: `/mnt/
  sdcard/lgpt2` no es `lgpt`.
- `StorageCategoryName()`: nombres canonicos de las tres categorias.
- `kStorageInventory[]`: inventario declarativo `{ruta, tipo, quien,
  cuando}` de los accesos actuales de file en el port, con dueno
  (modulo/daemon) y momento.

### Inventario auditado

| Ruta                                            | Tipo       | Quien                                       | Cuando                    |
|-------------------------------------------------|------------|---------------------------------------------|---------------------------|
| `<root>/config.xml`                             | Persistent | Config (Application/Model)                  | boot y guardado de config |
| `<root>/last_project`                           | Persistent | AppWindow (LAST_PROJECT_NAME)               | cambio de proyecto        |
| `<root>/projects/`                              | Persistent | Persistency/AppWindow (bin:projects)        | guardar/cargar proyecto   |
| `<root>/samples/`, `<root>/samples/records`     | Persistent | UsbRecordModal (bin:samples)                | grabaciones y samples     |
| `<root>/instruments/`                           | Persistent | TreeFrogSystem::Boot                        | arranque del core         |
| `<root>/otg/`                                   | Persistent | TreeFrogUac2Bridge (flags y perfil)         | estado persistente OTG    |
| `<root>/otg/bin/`                               | Persistent | TreeFrogUac2Bridge (scripts del driver)     | aplicar perfil/modo       |
| `/mnt/sdcard/LGPT_OTG_LOGS/`                    | Diagnostic | daemons OTG + flush de apagado              | apagado y fallos          |
| `<root>/otg/logs/`                              | Diagnostic | TreeFrogUac2Bridge (setup log)              | setup del driver          |
| `/tmp/r36sx_lgpt_logs/`                         | Volatile   | FileLogger/TreeFrogLibretro                 | logs runtime              |
| `/tmp/r36sx_lgpt_usb/`                          | Volatile   | TreeFrogUac2Bridge <-> daemons (ABI)        | estado de la union USB    |
| `/tmp/r36sx_lgpt_record/`                       | Volatile   | UsbRecordModal                              | cache de grabacion        |
| `<root>/tmp/record/` (bind-mount tmpfs)         | Volatile   | lgpt_launcher_u241.sh                       | volcado de previews USB   |
| `/tmp/r36sx_{uac2_bridge,sp404_pcm,midi_pcm,aoa_bulk_pcm,usb_capture_monitor}_fifo` | Volatile | core <-> daemons | stream FIFO                 |
| `/tmp/joy_key`                                  | Volatile   | TreeFrogLibretro (Cubevol shm ftok)         | entrada compartida        |

## Evidencia

- Host test `tests/host/storage_policy_host_test.cpp`, runner
  `tests/run_host_storage_policy.sh` (en `scripts/audit.sh`):
  `STORAGE_POLICY_HOST_ALL_OK (55 checks)` ASAN/UBSAN (clasificacion de
  cada ruta real de source/ y device/, fronteras de segmento, null/vacia,
  inventario coherente con la clasificacion).
- Baseline `tests/test_f5_baseline.py`: `F5_BASELINE_OK` — capa pura sin
  includes ni GUI/audio/daemons; barrido estatico de `source/` y
  `device/` contra la politica ("nada escribe fuera de los tres tipos");
  la raiz del alias `bin` del core es la misma que StoragePolicyRoot.
- Audit completo: `AUDIT_CLEAN_MAIN_U2523_OK`.
- Build MIPS byte-identico `7709b665` (capa pura sin consumidor runtime,
  no altera el binario); gate diag `NO_DIAGNOSTICS_OUTSIDE_DEVICE`;
  deploy SD == build; backup `LGPT_BEFORE_U2523_20260813_225053`.
- Commit: 3a31cbf.