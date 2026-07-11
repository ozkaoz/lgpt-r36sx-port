# U2.32 — Restauración del flujo Listen/Import

Estado esperado antes de aplicar: árbol U2.31 validado en hardware.

## Objetivo

U2.31 confirmó que la ruta de audio podía reproducir desde `Instrument`, pero dejó el comportamiento invertido respecto al flujo estable. U2.32 restaura el diseño correcto:

- En `Instrument`, `B` no debe iniciar preescucha.
- En `Instrument`, `L2+B` detiene cualquier preescucha activa.
- En `Instrument`, `A` sobre el campo `sample` abre el diálogo `Listen / Import / Exit` como en la base estable.
- En el diálogo `Listen / Import`, `A` sobre `Listen` inicia la preescucha.
- En el diálogo `Listen / Import`, `L2+B` detiene la preescucha.

## Cambios técnicos

- Se elimina la preescucha directa con `B` añadida temporalmente en `InstrumentView`.
- Se elimina el helper de preview interno de `InstrumentView` para evitar divergencia con el flujo estable.
- `ImportSampleDialog::preview()` se vuelve más robusto: abre físicamente el WAV seleccionado, crea `samples:__u2_listen_preview.wav` y reproduce ese WAV interno. Si el render temporal falla, hace fallback a `StartStreaming(element)`.
- Se actualizan las cadenas visibles a `Graphical Chopper U2.32` y `PITCH/ENV U2.32`.

## Archivos tocados

- `sources/Application/Views/InstrumentView.cpp`
- `sources/Application/Views/InstrumentView.h`
- `sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp`
- `sources/Application/Views/ModalDialogs/ImportSampleDialog.h`
- `sources/Application/Views/ModalDialogs/SampleChopperModal.cpp`

## No incluido

No implementa todavía chops renderizados/exportables ni cambios sobre el campo `slices`. Eso queda para la fase posterior a la versión estable.
