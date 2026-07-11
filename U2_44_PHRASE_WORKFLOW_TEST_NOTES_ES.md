# U2.44 TEST - Phrase workflow fixes

Base: U2.43 Phrase Volume Test.

Estado: paquete de prueba para SD. No marcar como estable hasta validar en consola.

## Cambios implementados

### Phrase

- `R1+A`: solo / unmute-all sigue igual que en U2.43 y se mantiene validado.
- `R1+B`: mute sigue igual que en U2.43 y se mantiene validado.
- Preview de fila cambia de `B` a `Y`.
  - `Y`: preescucha la fila actual.
  - `B` vuelve a quedar libre para navegación/acciones históricas del menú.
- Selección/corte corregidos:
  - `L1+X`: inicia selección.
  - `X`: corta la selección activa.
  - `L1+X` mientras la selección está activa ya no corta accidentalmente; muestra `Selection active: X cuts`.
- Doble `A` sobre chop asignado:
  - Abre Pitch/Envelope solo si el player no está reproduciendo ni streameando.
  - Si hay playback activo, muestra `Stop playback first` y no abre el modal.
- Navegación entre phrases usados por Song:
  - `L2+LEFT` / `L2+UP`: phrase anterior asignado en el canal actual de Song.
  - `L2+RIGHT` / `L2+DOWN`: phrase siguiente asignado en el canal actual de Song.
  - La navegación busca phrases alcanzables desde los chains usados por el canal actual en Song.

### Pitch/Envelope abierto desde Phrase con doble A

- `R1+B`: vuelve al menú Phrase cuando Pitch/Envelope fue abierto desde Phrase con doble `A`.
- La ayuda inferior cambia a `R1+B back Phrase` en ese contexto.
- Cuando Pitch/Envelope se abre desde Chopper normal, conserva `L1+R1 exit`.

## Pendiente / no incluido en U2.44 TEST

No se modificaron todavía los commands DSP (`ARPG`, `FCUT`, `FLTR`, `DLAY`, etc.) ni se añadió reverb/delay nuevo ni el medidor stereo del Master. Eso debe hacerse en una rama separada porque toca playback/mixer/export y conviene validarlo con audio incremental.

Propuesta para U2.45:

- Auditoría de commands existentes: mantener `PTCH`, `ARPG`, `KILL`, `FCUT`, `FRES`, `PAN`, `TABL`, `DLAY`, `RTRG`, `STOP`, `TMPO`; esconder o mover comandos poco usados del selector normal.
- Revisión de `FLTR`: definir parámetro con modo `Bypass / Low-pass / High-pass` y mantener compatibilidad con proyectos antiguos.
- Delay/Reverb: implementar como FX de mixer o bus, no como operación destructiva de sample.
- Master: medidor stereo L/R y combinación para pan de tracks desde Master/Mixer.

## Protocolo rápido de prueba

1. Abrir un proyecto con phrases asignados en Song.
2. En Phrase:
   - Probar `Y` sobre una fila con nota/chop.
   - Probar `L1+X`, mover cursor, luego `X` para cortar selección.
   - Probar `L2+LEFT/RIGHT` y verificar que recorre phrases asignados en Song.
3. Sobre una fila con chop asignado:
   - Con playback detenido, doble `A` debe abrir Pitch/Envelope.
   - En Pitch/Envelope, `R1+B` debe volver a Phrase.
   - Con playback activo, doble `A` debe bloquearse con `Stop playback first`.

