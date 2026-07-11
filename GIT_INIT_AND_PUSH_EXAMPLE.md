# Comandos para subir a GitHub

Desde la carpeta del repositorio:

```bash
git init
git add .
git commit -m "AU11Z6 stable source base for R36SX LGPT port"
git branch -M main
git remote add origin https://github.com/USUARIO/REPOSITORIO.git
git push -u origin main
```

Para continuar OTG sin contaminar la base estable:

```bash
git checkout -b otg-sidecar-from-au11z6
```
