# Compilación reproducible ALSA/UAC2 para R36SX Stock

Objetivo:

```text
Linux 4.4.186-release
MIPS32r2 little-endian, ABI o32
PREEMPT
CONFIG_MODVERSIONS=n
```

Toolchain validado:

```text
$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/bin/mips-mti-linux-gnu-
Codescape GNU Tools 2018.09-02, GCC 6.3.0
```

Flujo conservado en `scripts/uac2/`:

1. `UAC2_STAGE1_AUDIT.sh`: inventario de toolchain, árboles del kernel, módulo UAC2 y SD.
2. `UAC2_STAGE2_COMPILE_ALSA_R4.sh`: configuración y compilación inicial en filesystem Linux; incluye corrección DTC para host GCC moderno.
3. `UAC2_STAGE2_FINALIZE_ALSA_R5.sh`: build dirigido a `sound/`, modpost y validación de símbolos.
4. `UAC2_STAGE3_DEPLOY_AND_COLLECT_R7.sh`: despliegue reversible, checksums, montaje/desmontaje, recolección y rollback.

El árbol Linux 4.4 no debe compilarse desde NTFS/DrvFS: contiene nombres que sólo se diferencian por mayúsculas y minúsculas. El build debe ejecutarse desde ext4 dentro de WSL, por ejemplo bajo `$HOME/lgpt-r36sx-kernel-work`.
