# Prompt para continuar desarrollo desde U2.50 FINAL

Estamos trabajando en un port de LGPT para R36SX/R36S. La versión estable actual aprobada en SD es U2.50 FINAL, basada en U2.46 FINAL Phrase Workflow y U2.50 Mixer/Master Layout.

Archivos/rutas esperadas del usuario:

- Carpeta Windows: `D:\R36S\PORT LPTRACKER`
- Ruta WSL: `/mnt/d/R36S/PORT LPTRACKER`
- Repo GitHub local: `/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port`
- Fuente estable: `LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE.zip`

Reglas de trabajo:

1. Responder en español.
2. Mantener textos internos del port en inglés cuando sean labels/UI.
3. Hacer cambios incrementales y empaquetar como TEST antes de marcar FINAL.
4. No mezclar cambios de DSP, UI y filesystem en una sola prueba salvo que sea inevitable.
5. Para cualquier ZIP generado, entregar también notas, patch y SHA256.
6. No afirmar que se hizo build MIPS si no se ejecutó realmente.

Estado estable U2.50:

- WAV export estable: `lgpt/exports/<ProjectName>/`.
- Phrase con columnas `Note | Vol | Inst | Cmd1 | Param1 | Cmd2 | Param2`.
- Phrase: `Y` preview, `L1+X` selección, `X` cortar, `L2+flechas` navegación, doble `A` abre Pitch/Envelope.
- Mixer: paneo con `R1+LEFT/RIGHT`, centro con `R1+DOWN`, mute/solo con `R1+B`/`R1+A`, tempo con `Y+flechas`.
- Master meters en Project y Mixer rediseñados.
- Commands visibles depurados; `FBYP` oculto pero compatible internamente.

Siguientes ideas posibles:

- Profundizar en commands/FX: delay/reverb/filtros de forma más musical y menos destructiva.
- Añadir pruebas de regresión por proyecto.
- Mejorar documentación de commands por step.
- Evaluar compresor/limiter simple para exportación, cuidando CPU en R36SX.
