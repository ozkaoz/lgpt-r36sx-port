# Actualizar GitHub con U2.46 FINAL desde WSL

Este paquete no puede hacer `git push` por sí mismo. Debe actualizarse un repo local ya clonado.

Ejemplo recomendado:

```bash
cd ~

rm -rf lgpt_u246_final_github
mkdir -p lgpt_u246_final_github
cd lgpt_u246_final_github

unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_46_FINAL_PHRASE_WORKFLOW_SOURCE.zip"

cd LGPT_PORT_U2_46_FINAL_PHRASE_WORKFLOW_SOURCE

bash scripts/UPDATE_GITHUB_U2_46_FROM_WSL.sh "/mnt/d/R36S/PORT LPTRACKER/GITHUB/<tu_repo_lgpt>" u2.46-final-phrase-workflow
```

Luego revisar y subir:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/<tu_repo_lgpt>"

git status --short
git diff --stat

git add -A
git commit -m "U2.46: final Phrase workflow for R36SX"
git push -u origin u2.46-final-phrase-workflow
```

Tag opcional:

```bash
git tag -a u2.46-final-phrase-workflow -m "U2.46 final Phrase workflow for R36SX"
git push origin u2.46-final-phrase-workflow
```
