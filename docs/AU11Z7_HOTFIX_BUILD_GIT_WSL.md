# AU11Z7 hotfix: build, install and GitHub push

Corrige dos fallos:

1. El repositorio local se creó, pero el `git push` falló si WSL no resuelve `github.com`.
2. La compilación no debe usar `make -f projects/Makefile.TREEFROG`; debe usar el Makefile principal con `PLATFORM=TREEFROG` y una copia temporal sin espacios en la ruta.

Comandos:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"

# Compilar e instalar en SD F:
bash tools/wsl/00_build_install_r36sx_v26.sh "/mnt/d/R36S/PORT LPTRACKER" F /tmp/lgpt_r36sx_v26

# Verificar SD
bash tools/wsl/02_verify_treefrog_sd.sh F

# Reintentar publicación en GitHub
bash tools/wsl/03_git_push_retry.sh https://github.com/ozkaoz/lgpt-r36sx-port.git r36sx-v2.6-treefrog-au11z6
```

La instalación crea:

```text
/lgpt/
/lgpt/config.xml
/roms/lgpt/start.lgpt
/cubegm/lgpt
/cubegm/lgpt.elf
/cubegm/cores/lgpt_libretro.so
/cubegm/lgpt_libretro.so
```
