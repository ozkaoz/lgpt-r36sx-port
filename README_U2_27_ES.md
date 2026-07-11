# U2.27 — Chopper preview stop, selección de chop, UI compacta y refresh Instrument

Base requerida: U2.26 validado.

Cambios:

- Cabecera `Graphical Chopper U2.27`.
- Panel `PITCH/ENV U2.27` más compacto para 320x240.
- Se eliminan las líneas duplicadas de ayuda abajo durante Pitch/Envelope.
- `L2+B` detiene la preescucha también dentro de Pitch/Envelope.
- En `Scope: Chop`, `R2+LEFT/RIGHT` cambia el chop objetivo sin salir del panel.
- Tras CROP, DELETE, Pitch/Env Apply y Undo/Redo, se refrescan los límites START/LOOPSTART/END del instrumento actual para recuperar la preescucha en Instrument.
- Al volver desde Chopper a Instrument, `InstrumentView` reconstruye sus campos para evitar límites stale después de editar el WAV.

No incluye todavía persistencia/exportación de chops renderizados en carpeta del proyecto. Eso queda separado para el siguiente bloque una vez U2.27 quede estable.
