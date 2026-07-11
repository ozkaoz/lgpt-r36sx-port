# U2.46 FINAL - Phrase Workflow for R36SX

U2.46 consolidates the stable Phrase workflow after SD testing.

## Stable controls

- `R1+A`: solo selected track; press again to unmute/unsolo all.
- `R1+B`: mute selected track.
- `Y` in Phrase: preview current row.
- `L1+X` in Phrase: start selection.
- `X` in Phrase: cut active selection.
- `double A` in Phrase: open Pitch/Envelope for assigned chop.
- `R1+B` in Pitch/Envelope opened from Phrase: return to Phrase.
- `L2+LEFT`: previous used phrase.
- `L2+RIGHT`: next used phrase.
- `L2+UP`: previous phrase assignment in current Song channel.
- `L2+DOWN`: next phrase assignment in current Song channel; create/link next phrase if needed.

## Phrase volume column

Phrase rows include `Vol` between Note and Instrument. Existing projects default to no per-step volume override.

## WAV export base

U2.46 keeps the U2.41 WAV export workflow:

- Project-scoped export folder.
- Song WAV and multitrack WAV export.
- Non-blocking export progress screen.
- Multitrack filenames with project/instrument naming.

## Next planned work

U2.47 should focus on commands/FX and Master view, not Phrase workflow.
