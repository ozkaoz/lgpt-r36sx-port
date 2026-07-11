# Guía simple - WSL Ubuntu 24, GitHub y SD R36SX

## 1. Abrir WSL Ubuntu 24

En Windows abre Ubuntu 24.04 y ejecuta:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
```

## 2. Configurar Git una sola vez

```bash
git config --global user.name "ozkaoz"
git config --global user.email "TU_CORREO_DE_GITHUB"
```

## 3. Publicar en GitHub

Crea en GitHub un repositorio vacío llamado `lgpt-r36sx-port`. No agregues README, licencia ni `.gitignore` desde GitHub.

Luego ejecuta:

```bash
bash tools/wsl/01_git_first_push.sh https://github.com/ozkaoz/lgpt-r36sx-port.git
```

## 4. Verificar SD con Stock OS + TreeFrogUI

Cambia `F` por la letra real de la SD en Windows:

```bash
bash tools/wsl/02_verify_treefrog_sd.sh F
```

## 5. Compilar e instalar

```bash
bash tools/wsl/00_build_install_r36sx_v26.sh "/mnt/d/R36S/PORT LPTRACKER" F /tmp/lgpt_r36sx_v26
```

## 6. Si falla por toolchain

Define la ruta del toolchain:

```bash
export TOOLCHAIN="$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot"
```

Luego repite el comando de compilación.

## 7. Orden recomendado de trabajo

1. Formatear SD.
2. Instalar Stock OS R36SX v2.6.
3. Instalar TreeFrogUI para R36SX v2.6.
4. Confirmar que la consola arranca correctamente.
5. Verificar SD con `02_verify_treefrog_sd.sh`.
6. Compilar e instalar LGPT.
7. Probar si TreeFrogUI muestra LGPT.
8. Si no aparece, trabajar la asociación FrogUI: carpeta `LGPT` hacia `lgpt_libretro.so`.
