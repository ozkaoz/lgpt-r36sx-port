#!/usr/bin/env python3

MAX_HISTORY = 24

class History:
    def __init__(self, boundaries):
        self.current = list(boundaries)
        self.undo = []
        self.redo = []

    def edit(self, next_state):
        self.undo.append(list(self.current))
        self.undo = self.undo[-MAX_HISTORY:]
        self.redo = []
        self.current = list(next_state)

    def undo_once(self):
        assert self.undo
        self.redo.append(list(self.current))
        self.current = self.undo.pop()

    def redo_once(self):
        assert self.redo
        self.undo.append(list(self.current))
        self.current = self.redo.pop()

h = History([0, 1000])
h.edit([0, 400, 1000])            # add cut
h.edit([0, 400, 700, 1000])       # add second cut
h.edit([0, 700, 1000])            # Y merges two cuts
assert h.current == [0, 700, 1000]
h.undo_once()
assert h.current == [0, 400, 700, 1000]
h.redo_once()
assert h.current == [0, 700, 1000]
h.undo_once()
h.undo_once()
assert h.current == [0, 400, 1000]
h.redo_once()
h.redo_once()
assert h.current == [0, 700, 1000]

# A new branch invalidates redo.
h.undo_once()
h.edit([0, 350, 700, 1000])
assert not h.redo

# Bounded history.
h = History([0, 1000])
for i in range(40):
    h.edit([0, 100 + i, 1000])
assert len(h.undo) == MAX_HISTORY

print("U2510_GLOBAL_CHOP_HISTORY_MODEL_OK")
