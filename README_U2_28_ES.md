# U2.28 — Panel Pitch/Env centrado y preescucha Instrument

Base esperada: U2.27 validado.

Cambios:

- `Graphical Chopper U2.28`.
- `PITCH/ENV U2.28`.
- Panel Pitch/Envelope centrado, con rectángulo cerrado y sin ayuda dentro del panel.
- Limpieza directa del rectángulo de framebuffer TreeFrog detrás del panel para evitar restos de waveform/barras a la derecha.
- Ayuda de uso movida a líneas inferiores: `UD/LR`, `B preview`, `A apply`, `L2+B stop`, `L1+X undo`, `L1+R1 exit`, `R2+LR chop`.
- Refresh más fuerte del instrumento tras edición destructiva: se notifica de nuevo `SIP_SAMPLE` y se reajustan `START/LOOPSTART/END`.
- `B` en Instrument reproduce directamente el sample/rango activo del instrumento mediante `StartStreamingRangeAt`.

Pendiente deliberado para desarrollo posterior: integrar el campo `slices` como acceso directo al Chopper y diseñar exportación/persistencia de chops renderizados por proyecto.
