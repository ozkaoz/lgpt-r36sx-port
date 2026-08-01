# Changelog

## H38.6

- Phrase volume fix: every value 0-100 now maps linearly (100 = full, 1 = ~2%, 0/0xFF = silent), no clipping or distortion at any level; new notes default to volume 100.
- Mixer VU meters now refresh in real time even while the player is stopped (same frame cadence as the USB-C Record meter).
- Rename Project moved from the in-project menu (crash when re-entering TreeFrogUI) to the startup menu: R1+A on a selected project.
- Unified text editor input everywhere (USB-C Record, project rename, new project, sample rename): X+UP/DOWN fast, L1+X case, A confirms, B erases, R1+LEFT cancels.
- New project and Save As use the same Record-style text editor (QWERTY keyboard removed).

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
