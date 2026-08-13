#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hdr = (root / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.h").read_text()
cpp = (root / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
layer = (root / "source/sources/Application/Views/ModalDialogs/SampleEditHistory.h").read_text()
# F3-3c: los action labels de los flujos de edicion viven en la capa
# ChopperController (host_.PushLogicalUndo); los de pitch en la vista.
cc = (root / "source/sources/Application/Views/ModalDialogs/ChopperController.h").read_text()
bundle = hdr + cpp + cc

for marker in [
    "MAX_LOGICAL_HISTORY = 24",
    "LogicalHistoryState",
    "SampleEditHistory<LogicalHistoryState> editHistory_",
    "undoLastChopperEdit",
    "redoLastChopperEdit",
    "PushLogicalUndo(\"Add cut\")",
    "PushLogicalUndo(\"Merge cuts\")",
    "PushLogicalUndo(\"Move cut start\")",
    "PushLogicalUndo(\"Move cut end\")",
    "PushLogicalUndo(\"Keep logical range\")",
    "pushLogicalUndo(\"Pitch setting\")",
    "pushLogicalUndo(\"Attack setting\")",
    "pushLogicalUndo(\"Sustain setting\")",
    "pushLogicalUndo(\"Release setting\")",
    "pushLogicalUndo(\"Pitch scope\")",
    "U2510_GLOBAL_CHOPPER_HISTORY_24_OVERLAY_SAFE",
]:
    assert marker in bundle, marker

# F3-2: los stacks undo/redo viven en la capa pura SampleEditHistory;
# el modal delega (Push/Undo/Redo/Peek) y mantiene los match de sample.
for marker in [
    "LGPT_HISTORY_MAX_ENTRIES 24",
    "Undo(",
    "Redo(",
    "PeekUndo",
    "PeekRedo",
    "ClearRedo",
]:
    assert marker in layer, marker
assert "editHistory_.Push(" in cpp
assert "if (!editHistory_.Undo(" in cpp
assert "if (!editHistory_.Redo(" in cpp
assert "editHistory_.PeekUndo(" in cpp
assert "editHistory_.PeekRedo(" in cpp

assert "LgptU2510GlobalChopperHistoryBuildMarker" in cpp
assert '__attribute__((used, visibility("default")))' in cpp
assert "static const char *u2510GlobalChopperHistoryMarker" not in cpp
assert "if (undoLogicalEdit())" in cpp
assert "if (redoLogicalEdit())" in cpp
assert "return restoreLastDestructiveEdit(false);" in cpp
assert "return restoreLastDestructiveEdit(true);" in cpp

print("U2510_GLOBAL_CHOP_HISTORY_STATIC_OK")
