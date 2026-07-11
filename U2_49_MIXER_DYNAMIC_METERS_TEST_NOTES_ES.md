# U2.49 TEST - Mixer dynamic meters and layout fixes

Base: U2.48 TEST.

Objetivo: corregir la prueba de Mixer/Master sin tocar el workflow de Phrase aprobado en U2.46.

## Cambios

### Mixer

- Las barras de los canales se redibujan en `OnPlayerUpdate`, no solo al mover el cursor.
- El medidor debe responder a la señal mientras el playback está activo, aunque el selector no se desplace.
- `ML` y `MR` pasan a barras verticales dinámicas, con el mismo lenguaje visual de los canales.
- Si un canal o el master alcanza/sobrepasa 0 dBFS, se marca con `!` y color de alerta.
- La barra del canal seleccionado usa una variación de color distinta para diferenciar selección de señal normal.
- El tempo en Mixer se resalta visualmente cuando se edita con `Y + flechas`.

### Project / Master

- El medidor de master vuelve a ser vertical.
- Se ubica abajo a la izquierda, para no interferir con los textos del menú Project.
- El medidor mantiene indicador de clipping con `!`.

### Commands

- `FBYP` se mantiene fuera del selector visible.
- La compatibilidad interna con proyectos antiguos se conserva.

## Controles relevantes

```text
Mixer:
A + UP/DOWN       volumen +/- 10
A + LEFT/RIGHT    volumen +/- 1
Y + UP/DOWN       tempo +/- 1
Y + LEFT/RIGHT    tempo +/- 10
R1 + LEFT/RIGHT   pan L/R
R1 + DOWN         center pan
R1 + B            mute
R1 + A            solo / unmute all al repetir
START             play
R1 + START        stop
```

## Prueba recomendada

1. Abrir un proyecto con varios canales activos.
2. Entrar a Mixer.
3. Reproducir sin mover el selector.
4. Confirmar que las barras de canal y ML/MR se mueven en tiempo real.
5. Subir temporalmente niveles para comprobar indicador `!` de clip.
6. Cambiar tempo con `Y + flechas` y confirmar highlight temporal del campo `Txxx`.
7. Entrar a Project y confirmar master L/R vertical abajo a la izquierda.

## Estado

Versión de prueba. No marcar estable hasta validar en SD.
