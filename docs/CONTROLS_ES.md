# Controles principales — LGPT R36SX (RC3)

Guía de funcionamiento del port: navegación entre vistas, edición y
combinaciones de teclas. La referencia autoritativa de cada vista está
disponible en pantalla pulsando `SELECT+R1` (ayuda contextual).

## Navegación entre vistas

| Combinación | Vista |
|---|---|
| `R1 + LEFT/RIGHT` | Cambiar de vista principal |
| `SELECT + R1` | Abrir/cerrar ayuda contextual (latch) |
| `SELECT + R2` | Diálogo de Audio Driver (modo USB) |
| `START` | Play / Stop |

## Vistas principales

- **Song**: matriz de canciones. `L/R` y `UP/DN` mueven el cursor; `A` edita o
  entra en la cadena; `B` mute/solo.
- **Chain**: pasos de cadena. `L/R` mueve el cursor de paso; `A` abre la
  frase; `B` cancela/vuelve atrás.
- **Phrase**: notas de la frase. `A` fija la nota; `A+UP/DN` octava; `B`
  cancela.
- **Instrument**: parámetros del instrumento por bloques (INSTRUMENT/FILTER/
  BITCRUSHER/PLAYBACK/EFFECT SENDS/AUTOMATION). `L/R` cambia de página;
  `R2+A` abre el menú FX; `R1+RIGHT` graba por USB.
- **Table**: tabla de comandos (FX1/P1/FX2/P2). La descripción del comando
  bajo el cursor se muestra en pantalla.
- **Groove**: patrones de swing. `UP/DN` mueve el cursor de paso.
- **Mixer**: volumen y páginas FX master. `SELECT` cicla
  MIX → DELAY → REVERB → EQ → COMP. `L/R` selecciona canal; `L->MST` envía al
  master; `R1+A` solo; `R1+B` mute; `R2` cambia el objetivo de edición.

## Páginas FX master (Mixer)

En las páginas DELAY/REVERB/EQ/COMP el parámetro `BYPASS` es la primera fila
(semántica `ON = efecto desactivado`).

| Tecla | Acción |
|---|---|
| `UP/DN` | Mover fila |
| `L/R` | Editar valor |
| `A` | Edición gruesa (coarse) |
| `SELECT` | Cambiar página |
| `START` | Play |

## Edición general

- `A` confirma / `B` cancela en menús y diálogos.
- `A + UP/DN` y `A + L/R` edición gruesa (x10 / x1).
- `X + UP/DN` salta cinco caracteres en el editor de texto.

## Sample browser

- Renombrar sample: `L1+X`.
- Eliminar sample: `L1+Y`, confirmar con `A`, cancelar con `B`.
- Editor de nombre: izquierda/derecha mueve el cursor; arriba/abajo cambia el
  carácter; `X+arriba/abajo` salta cinco caracteres; `L1+X` cambia
  mayúsculas/minúsculas; `A` confirma; `B` borra.

## Chopper

- Undo: `L1+X`.
- Redo: `R1+X`.
- Cortar (cut): `A`.
- Play: `B`.
- `L1+R1` modo Pitch/Env; `SELECT` crop; `L1+LR` cursor rápido.
- `R1+LR` selección de sample en el instrumento.
- La forma de onda se dibuja de forma gráfica; los marcos que la delimitan
  son parte del diseño del chopper.

## Record

- Record: Preview, Save y Discard actúan sobre una toma temporal pendiente.

## Ayuda contextual (RC3)

`SELECT+R1` abre un overlay con los controles de la vista activa. Se mantiene
pulsado mientras se consulta; al soltar se cierra sin cambiar de página ni
interferir con el estado. `SELECT+R2` conserva el diálogo de Audio Driver.
