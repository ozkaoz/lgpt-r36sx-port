# LittleGPTracker_R36SX_TREEFROG

Esta carpeta es el código fuente directo del port.

No es un overlay y no depende de aplicar parches en cada compilación. Es una copia completa de LGPT con la plataforma TreeFrog integrada como código fuente:

- `sources/Adapters/TREEFROG/`
- `projects/Makefile.TREEFROG`
- Cambios comunes validados de la línea funcional V1.4 RenderGuard integrados en `sources/Application/...`

La fuente upstream sin modificar se conserva aparte en:

- `reference/LittleGPTracker_UPSTREAM_ORIGINAL/`

Para auditar qué cambió respecto a LGPT original, usa:

```bash
bash scripts/30_diff_direct_source_against_upstream.sh
```
