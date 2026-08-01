# Changelog

## H38.5

- Removed FX3 from phrase editing and playback (Phrase: `00 N V I FX1 P FX2 P`; Table: `F1 P1 F2 P2`).
- Centered phrase and table grids with per-column headers.
- Volume and FX intensity scale now treat 1 as 100% (higher values attenuate).
- Dense mixer VU meters in Record style.
- Rename Project action in the Project menu.

## U2.52.3

- Stable bidirectional USB-C OTG audio at 48 kHz.
- Rewritten Record workflow with transactional Preview, Save and Discard.
- Input monitor restricted to Record.
- Chord-aware input handling and Chopper Undo/Redo.
- Safe sample rename and deferred sample deletion.
- Fixed nested rename input forwarding and caret alignment.
- Consolidated source, scripts, tests and bilingual documentation.

Earlier experimental iterations were consolidated into this release and are available through Git history, not as duplicate files in the current tree.
