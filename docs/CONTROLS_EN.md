# Controls — LGPT R36SX (RC3)

Port usage guide: view navigation, editing and key combinations. The
authoritative per-view reference is available on screen with `SELECT+R1`
(contextual help).

## View navigation

| Combination | View |
|---|---|
| `R1 + LEFT/RIGHT` | Switch main view |
| `SELECT + R1` | Open/close contextual help (latched) |
| `SELECT + R2` | Audio Driver dialog (USB mode) |
| `START` | Play / Stop |

## Main views

- **Song**: song matrix. `L/R` and `UP/DN` move the cursor; `A` edits or
  enters the chain; `B` mute/solo.
- **Chain**: chain steps. `L/R` moves the step cursor; `A` opens the phrase;
  `B` cancels/back.
- **Phrase**: phrase notes. `A` sets the note; `A+UP/DN` octave; `B` cancel.
- **Instrument**: instrument parameters in blocks (INSTRUMENT/FILTER/
  BITCRUSHER/PLAYBACK/EFFECT SENDS/AUTOMATION). `L/R` switches page;
  `R2+A` opens the FX menu; `R1+RIGHT` USB record.
- **Table**: command table (FX1/P1/FX2/P2). The description of the command
  under the cursor is shown on screen.
- **Groove**: swing patterns. `UP/DN` moves the step cursor.
- **Mixer**: volume and master FX pages. `SELECT` cycles
  MIX → DELAY → REVERB → EQ → COMP. `L/R` selects channel; `L->MST` routes to
  master; `R1+A` solo; `R1+B` mute; `R2` switches the edit target.

## Master FX pages (Mixer)

On the DELAY/REVERB/EQ/COMP pages the `BYPASS` parameter is the first row
(`ON = effect disabled`).

| Key | Action |
|---|---|
| `UP/DN` | Move row |
| `L/R` | Edit value |
| `A` | Coarse edit |
| `SELECT` | Switch page |
| `START` | Play |

## General editing

- `A` confirm / `B` cancel in menus and dialogs.
- `A + UP/DN` and `A + L/R` coarse editing (x10 / x1).
- `X + UP/DN` jumps five characters in the text editor.

## Sample browser

- Rename sample: `L1+X`.
- Delete sample: `L1+Y`, confirm with `A`, cancel with `B`.
- Name editor: left/right moves the cursor; up/down changes the character;
  `X+up/down` jumps five characters; `L1+X` toggles case; `A` confirms;
  `B` deletes.

## Chopper

- Undo: `L1+X`.
- Redo: `R1+X`.
- Cut: `A`.
- Play: `B`.
- `L1+R1` Pitch/Env mode; `SELECT` crop; `L1+LR` fast cursor.
- `R1+LR` sample selection on the instrument.
- The waveform is rendered graphically; the frames delimiting it are part of
  the chopper design.

## Record

- Record: Preview, Save and Discard act on a pending temporary take.

## Contextual help (RC3)

`SELECT+R1` opens an overlay with the controls of the active view. Keep it
pressed while consulting; releasing closes it without changing page or
interfering with state. `SELECT+R2` keeps the Audio Driver dialog.
