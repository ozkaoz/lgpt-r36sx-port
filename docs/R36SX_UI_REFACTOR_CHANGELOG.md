# R36SX UI refactor changelog

## Step 01 - UI comment cleanup

Scope:
- sources/Application/Views/BaseClasses/View.cpp
- sources/Application/Views/ModalDialogs/SelectProjectDialog.cpp

Intent:
- Normalize TreeFrog/R36SX UI comments.
- Preserve validated behavior.
- No executable code changes.

Preserved behavior:
- Selector inicial abre en root:projects.
- P G / SCPI / TT conservan bloque de fondo.
- Selección de mapa conserva CD_HILITE2 + invert_.
- Barras inferiores conservan props.invert_.
- No changes to audio, input, Player, samples, projects or SD layout.

## Step 02 - SelectProjectDialog initial folder constant

Scope:
- sources/Application/Views/ModalDialogs/SelectProjectDialog.cpp

Intent:
- Replace the literal root:projects with a named file-scope constant.
- Keep validated behavior unchanged: project selector opens directly in projects.
- No SD layout changes.
- No audio/input/Player changes.

Expected behavior:
- Initial project browser folder remains root:projects.

## Step 03 - Exit menu English text

Scope:
- UI text only.

Intent:
- Replace remaining Spanish exit-menu labels with English labels.
- "Volver al menu principal" / "Volver al menú principal" -> "Return to Main Menu"
- "Salir a TreeFrogUI" -> "Exit to TreeFrogUI"

Behavior:
- No navigation logic changes.
- No audio/input/Player changes.
- No SD layout changes.

## Step 03b - Exit menu English text fix

Scope:
- sources/Application/Views/ProjectView.cpp

Intent:
- Complete Project exit-menu translation:
  - Return to Main Menu
  - Exit to TreeFrogUI
  - Cancel
- Remove temporary in-source backup files from the failed 62 script.

Behavior:
- UI text only.
- No navigation logic changes.
- No audio/input/Player changes.
- No SD layout changes.

## Step 04a - Groove selection uses Song-style highlight

Scope:
- sources/Application/Views/GrooveView.cpp

Intent:
- Fix Groove selected cell appearing white.
- Use the same validated visual semantics as Song:
  CD_HILITE2 + props.invert_ = true.

Behavior:
- UI rendering only.
- No model/data/editing changes.
- No audio/input/Player changes.
- No SD layout changes.

## Step 04b - Chain selection uses Song-style highlight

Scope:
- sources/Application/Views/ChainView.cpp

Intent:
- Make Chain selected cell visually closer to Song.
- Use CD_HILITE2 + props.invert_ = true.

Behavior:
- UI rendering only.
- No model/data/editing changes.
- No audio/input/Player changes.
- No SD layout changes.

## Step 04c - Phrase selection uses Song-style highlight

Scope:
- sources/Application/Views/PhraseView.cpp

Intent:
- Make Phrase selected cell visually consistent with Song.
- Use CD_HILITE2 + props.invert_ = true.

Behavior:
- UI rendering only.
- No model/data/editing changes.
- No audio/input/Player changes.
- No SD layout changes.

## Step 04d - Table selection uses Song-style highlight

Scope:
- sources/Application/Views/TableView.cpp

Intent:
- Make Table selected cell visually consistent with Song.
- Use CD_HILITE2 + props.invert_ = true.

Behavior:
- UI rendering only.
- No model/data/editing changes.
- No audio/input/Player changes.
- No SD layout changes.

## Step 05 - Disable TreeFrog version badge

Scope:
- Build configuration only.

Intent:
- Remove the V/version badge shown at the top-right of the screen.
- Build with TREEFROG_PORT_VERSION_BADGE=0.

Behavior:
- UI overlay only.
- No source behavior changes.
- No audio/input/Player changes.
- No SD layout changes.
