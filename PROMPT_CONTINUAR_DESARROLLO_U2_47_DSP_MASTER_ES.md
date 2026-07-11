# Prompt para continuar desarrollo desde U2.46 FINAL hacia U2.47

Continuar el desarrollo del port LGPT para R36SX/R36S usando como base exacta el paquete:

```text
LGPT_PORT_U2_46_FINAL_PHRASE_WORKFLOW_SOURCE
```

Contexto estable aprobado:

- U2.46 FINAL es estable en SD.
- No modificar de nuevo el workflow de Phrase salvo corrección explícita.
- Mantener la columna Phrase como `Note | Vol | Inst | Cmd1 | Param1 | Cmd2 | Param2`.
- Mantener navegación Phrase:
  - `L2+LEFT` previous used phrase.
  - `L2+RIGHT` next used phrase.
  - `L2+UP` previous phrase assignment in current Song channel.
  - `L2+DOWN` next phrase assignment in current Song channel, creando/vinculando nuevo phrase si no hay asignación posterior.
- Mantener `Y` como preview en Phrase.
- Mantener `L1+X` para iniciar selección y `X` para cortar selección.
- Mantener doble `A` para abrir Pitch/Envelope de chop asignado.
- Mantener `R1+B` para volver de Pitch/Envelope a Phrase cuando se abrió desde Phrase.

Objetivo U2.47:

Revisar, limpiar y potenciar los commands disponibles en Phrase/Song, especialmente comandos como `ARPG`, `FCUT`, `KILL`, `PTCH` y comandos relacionados con filtros/FX.

Propuestas funcionales:

1. Filtros más claros:
   - Separar o etiquetar claramente Low Pass, High Pass y Bypass.
   - Evitar que un único comando sea ambiguo.
   - Si el motor existente no permite un filtro completo por track, implementar primero como comando seguro/no destructivo.

2. Añadir FX musicales si el motor lo permite sin romper rendimiento:
   - Delay.
   - Reverb.
   - Preferir implementación ligera y estable para MIPS/R36SX.
   - Evitar colas largas o buffers grandes.

3. Purgar o de-priorizar comandos poco útiles o duplicados:
   - El volumen ya tiene columna dedicada en Phrase; no eliminar pitch.
   - Mantener `PTCH` porque sigue siendo musicalmente crítico.
   - Evitar eliminar comandos antiguos sin compatibilidad de carga.

4. Master View:
   - Agregar visual de volumen stereo L/R del proyecto.
   - Puede ser barra horizontal/inclinada de dos canales, pero debe ser legible en pantalla pequeña.
   - Debe leer nivel real de mezcla o aproximación confiable del bus master.

5. Panning desde Master:
   - Agregar combinación de teclas para panear tracks desde Master.
   - Debe poder seleccionar track y ajustar pan L/R sin entrar en otro menú.
   - No romper mute/solo existente.

Restricciones técnicas:

- Mantener export WAV compatible con U2.41+.
- Si se añaden FX, garantizar que export offline use el mismo procesamiento que playback.
- Generar U2.47 TEST primero, no estable.
- Incluir ZIP fuente completo, patch, notas y sha256.
- Validar con `VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh`.
- No afirmar build MIPS real si no fue ejecutado.
