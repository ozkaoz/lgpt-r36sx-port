# Runbook: Recuperación de tarjeta SD degradada (incidente 2026-08-06)

> Objetivo: documentar el incidente en el que la SD de la R36SX quedó
> ilegible tras insertarla en la consola, cómo se diagnostica, y cómo
> restaurarla sin pérdida. Debe usarse como referencia **la primera vez
> que el firmware de la consola o un `clean` dejen la SD en estado
> degradado** (`No Media` / lectura `0xFF`).

## 1. Síntomas

Cuando se inserta la SD en la R36SX (o se vuelve a conectar al PC)
puede quedar así:

| Síntoma | Significa |
|---|---|
| Windows ve un disco de **3,94 GB** (antes 29,3 GB) | El **controlador** de la tarjeta degrada y solo reporta una zona útil ínfima; el contenido de mayor LBA no es accesible |
| Lectura en bruto devuelve **`0x00`/`0xFF`** o **errores CRC** a partir de ~4 GB | La zona de datos ya no se puede leer ni escribir de forma coherente |
| `Ddiskpart create partition primary` → **"Not enough available capacity"** | El controlador no permite crear particiones en la zona degradada |
| `format fs=fat32` no completa / `Get-Volume` muestra 0 bytes | El filesystem no puede serializarse |

En la práctica: **no es una "FAT sucia"**: es **degradación del controlador
físico**. No hay forma de "ampliar los sectores del controlador" por
software; la capacidad se negocia en el bus SD por el controlador (hardware).

## 2. Prevención (recomendado antes de cada uso)

- Mantener **siempre** una copia completa del contenido de la SD en
  `D:\R36S\PORT LPTRACKER\BACKUPS\SD_FULL_*` después de cada cambio
  importante. Los directorios `SD_FULL_BACKUP_BEFORE_*` son el punto de
  restauración.
- **Ciclo de vida de la SD (paquete U2.54)**: desde 2026-08-06 los logs de
  runtime ya no se escriben en la tarjeta durante la sesión; viven en RAM
  (`/tmp/r36sx_lgpt_logs`) y solo se vuelcan a `LGPT_OTG_LOGS` en el apagado
  limpio (`otg_u241_shutdown.sh` → `u2414_flush_logs_to_sd` + `sync`).
  Consecuencias prácticas:
  - En apagado normal (menú TreeFrogUI) los logs se conservan en la SD.
  - Si la consola se apaga **bruscamente**, la última sesión de logs se pierde
    (es el coste de no desgastar la NAND). No es un fallo.
  - `scripts/collect_logs.sh` sigue leyendo desde la tarjeta, así que
    funciona igual después de un apagado limpio.
- **Regla de oro**: no extraer la SD ni cortar alimentación en mitad de un
  guardado; usar siempre el apagado del menú (que ejecuta el flush + sync).
- No apagar la consola mientras esté escribiendo (metadatos de audio/uproj).
- Antes de arrancar OTG, verificar que Windows vea la tarjeta como **FAT32**
  y con su tamaño normal; si aparece como "R36SX 3,94 GB" no seguir.

## 3. Diagnóstico (sin escribir nada) — Windows admin

Scripts de solo lectura usados en el incidente (detalles en
`scripts/SD_DIAGNOSTICS.ps1`):

1. Enumerar USB/disco: `Get-Disk`, `Get-Partition`, `Get-Volume`.
2. Detectar tamaño físico y que HTML coincide: `Get-Disk -Number <N>`.
3. Linux m-lectura sector 0 MBR:
   ```powershell
   Start-Process powershell -Verb RunAs -Arg '-File probe.ps1'
   ```
   (`probe.ps1` abre `\\ physicalDrive<N>` en `Read` y muestra MBR/PTE,
   luego prueba offsets 0,2K,4K… hasta 24 GB y reporta `FF`/`CRC`).
   - Leer sector0 `55AA` + PTE → distingue MBR ok.
   - Leer offsets altos → `FF` o error CRC ⇒ controlador degradado.

Resultados 2026-08-06 (disco 3):
```
<SECTOR0=55-AA / FF-FF / CRC>  → decisión: degradado
```

## 4. Recuperación correcta (administrador)

La opción que sí funciona es **repartitionar + formatear + copiar desde
backup** (NO hay fotagrafía del contenido en la tarjeta; el backup mando).

### 4.1. Preparación

1. Re-inserte físicamente la SD en el lector (o re-conecta reader). Si
   aparece `No Media`, puede que el controlador haya entrado en `No Media`:
   es normal tras un `clean`; reintentar en otro slot / lector.
2. Comprobar que `Get-Disk` la reporta de nuevo.

### 4.2. Limpiar

```powershell
# (admin)
Start-Process diskpart -Verb RunAs  # y ejecutar:
#   select disk <N>
#   clean all
```
`clean all` borra TODOS los sectores (puede tardar minutos). Si tras esto
el controlador sigue en `No Media`, la tarjeta debe considerarse **quemada**;
hacerlo en otro slot/lector o prueba-por-favor físico.

### 4.3. Crear partición y formato

```powershell
$p = New-Partition -DiskNumber <N> -UseMaximumSize -DriveEse`.
Format-Volume -Partition $p -FileSystem FAT32 -NewFileSystemLabel R36SX
```
(admite letras automáticamente.)

### 4.4. Prueba definitiva: persistencia de escritura en bruto

Si tras `clean all` la tarjeta vuelve a "Online" pero **no** acepta crear
particion, hacer esta prueba de escritura/relectura en bruto:

```powershell
# admin - escribir 1 MB de 0x55 en el sector 0 y releer.
# Si los bytes NO persisten (read-back != 0x55) el controlador esta
# agotado y la tarjeta no es recuperable por software.
```

Resultado 2026-08-06 (disco 3, 3,94 GB):
```
WRITE done first 1MB=0x55
READ_BACK n=1048576 ok0x55=0 otherBytes=1048576
PERSIST_FAIL (los bytes no se escribieron/leen)
```
**Conclusión**: tarjeta físicamente agotada (fin de vida). No reparable por
software. Única salida: **tarjeta nueva**; todos los datos viven en
`D:\R36S\PORT LPTRACKER\BACKUPS`.

### 4.5. Copiar contenido desde backup

```powershell
# vía WSL (rutas con espacio):
SD_MOUNT=/mnt/i bash scripts/install.sh
```
o, restaura de raíz de backup:
```bash
cp -a /mnt/d/R36S/PORT LPTRACKER/BACKUPS/SD_FULL_BACKUP_BEFORE_FORMAT_20260806/.  /mnt/i/
```
y luego superponer proyecto + core EQ (ver §5).

## 5. Overlay imprescindible (fase final de esta sesión)

De `D:\R36S\PORT LPTRACKER\GITHUB\lgpt-r36sx-port\dist`:

1. **Core EQ**: `cubegm/cores/lgpt_r36sx_port_libretro.so` ← `dist/lgpt_libretro.so`
   SHA256 debe ser `11790c46940fc3d6ca924ad12235c1210e0c8e1af318038458cbe832ca4e5688`.
2. **Proyecto más reciente del usuario** (85,9 KB):
   `BACKUPS\SD_CRITICAL_20260805\lgpt\projects\lgpt_KaOz\lgptsav.dat` →
   `\lgpt\projects\lgpt_KaOz\lgptsav.dat` (no sobrescribir con el más viejo
   de `SD_FULL_…`).

## 6. Verificación posterior

```bash
sha256sum /mnt/i/cubegm/cores/lgpt_r36sx_port_libretro.so   # 11790c4… EQ
ls -la /mnt/i/lgpt/projects/lgpt_KaOz/lgptsav.dat           # 87 KB
```

## 7. Lecciones y decisiones a futuro

- El de corrupción de la tarjrel es **recurrente** en la R36SX. La única
  garantía es el respaldo incremental antes de cada cambio en la SD.
- El intento de "ampliar a 8 GB" no es factible: NO alterar la geometría de
  la partición más allá de lo que reporte el controlador; crea sectores
  fantasma y acelera la degradación.
- Si un `clean all` deja la tarjeta en `No Media` y reintentar en el mismo
  lector no la revive: **es el final del ciclo de la tarjeta** — preservar el
  backup como única fuente de verdad y reusar esta runbook con la copia.