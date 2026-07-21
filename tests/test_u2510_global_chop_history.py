#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hdr = (root / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.h").read_text()
cpp = (root / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp").read_text()

for marker in [
    "MAX_LOGICAL_HISTORY = 24",
    "LogicalHistoryState",
    "undoHistory_",
    "redoHistory_",
    "undoLastChopperEdit",
    "redoLastChopperEdit",
    "pushLogicalUndo(\"Add cut\")",
    "pushLogicalUndo(\"Merge cuts\")",
    "pushLogicalUndo(\"Move cut start\")",
    "pushLogicalUndo(\"Move cut end\")",
    "pushLogicalUndo(\"Keep logical range\")",
    "pushLogicalUndo(\"Pitch setting\")",
    "pushLogicalUndo(\"Attack setting\")",
    "pushLogicalUndo(\"Sustain setting\")",
    "pushLogicalUndo(\"Release setting\")",
    "pushLogicalUndo(\"Pitch scope\")",
    "U2510_GLOBAL_CHOPPER_HISTORY_24_OVERLAY_SAFE",
]:
    assert marker in hdr or marker in cpp, marker

assert "LgptU2510GlobalChopperHistoryBuildMarker" in cpp
assert '__attribute__((used, visibility("default")))' in cpp
assert "static const char *u2510GlobalChopperHistoryMarker" not in cpp
assert "if (undoLogicalEdit())" in cpp
assert "if (redoLogicalEdit())" in cpp
assert "return restoreLastDestructiveEdit(false);" in cpp
assert "return restoreLastDestructiveEdit(true);" in cpp

print("U2510_GLOBAL_CHOP_HISTORY_STATIC_OK")
