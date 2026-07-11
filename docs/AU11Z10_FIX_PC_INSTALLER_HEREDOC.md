# AU11Z10 - Fix generador de instalador PC

Corrige `tools/wsl/05_create_pc_reinstall_package_from_sd.sh`.

## Problema

La versión AU11Z9 escribía el README del instalador con un heredoc no citado. El bloque Markdown con triple backtick fue interpretado por Bash como sustitución de comandos, provocando errores como:

- `powershell: command not found`
- `.INSTALL_LGPT_R36SX_TO_SD.ps1: command not found`
- `F: command not found`

## Corrección

El README del instalador ahora se escribe con heredoc citado y comandos indentados, evitando que Bash ejecute texto destinado a documentación.

## Uso

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
bash tools/wsl/05_create_pc_reinstall_package_from_sd.sh F "/mnt/d/R36S/PORT LPTRACKER"
```
