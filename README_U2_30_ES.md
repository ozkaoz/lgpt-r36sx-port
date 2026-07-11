# U2.30 - Operation overlay centrado + preescucha Instrument corregida

Base: U2.29 validado en hardware.

Cambios:

- `Graphical Chopper U2.30`.
- `PITCH/ENV U2.30`.
- Pantalla de operación `OK / Press A to continue` redibujada como rectángulo centrado.
- `InstrumentView` preescucha con `B` reescrita para usar `samples:<nombre.wav>`, abrir físicamente el WAV con `WavFile::Open` y no depender únicamente del `SamplePool` en memoria.
- `InstrumentView` acepta `B` como preescucha antes de `FieldView` y agrega `L2+B` como stop de preescucha.

No cambia la lógica de chops renderizados/exportables. Eso queda para el siguiente desarrollo tras validar U2.30.
