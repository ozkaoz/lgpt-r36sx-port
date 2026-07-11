# U2.22 — Pitch Screen + Clean Operation UI + Stable Candidate

Aplicar sobre U2.21 validado.

Cambios principales:

- `L1 + R1` abre/cierra una pantalla dedicada `PITCH SAMPLE`.
- En la pantalla de pitch, `LEFT/RIGHT` o `UP/DOWN` ajustan manualmente el pitch en pasos de 1 semitono entre `-12` y `+12`.
- `B` genera una preescucha temporal del pitch seleccionado sin aplicar el cambio al WAV original.
- `A` aplica físicamente el pitch seleccionado al WAV.
- Se elimina el pitch físico automático con `R1 + flechas` desde CROP SAMPLE.
- La pantalla de progreso/OK ya no usa la barra verde superpuesta; se muestra texto centrado: operación, porcentaje, `OK`, `Press A to continue`.
- Se conserva la persistencia de chops por sidecar `sample.wav.u2chop` validada en U2.20/U2.21.

Nota: la preescucha de pitch crea temporalmente `samples:__u2_pitch_preview.wav`.
