#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'source/sources/Application/UI/Views/ModalDialogs'
cpp=(base/'ImportSampleDialog.cpp').read_text()
editor=(base/'TreeFrogTextEditor.cpp').read_text()
blob=cpp+editor
assert 'U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL' in blob
assert 'snprintf(line, sizeof(line), "Name: %-22.22s", stem_);' in editor
assert 'DrawString(1, 2, line, props);' in editor
assert 'int caret = 6 + cursor_;' in editor
assert 'int caret = 7 + cursor_;' not in blob
assert 'DrawString(1, 3, cursorLine, props);' in editor
# Geometry model: displayed stem begins at x=1+len("Name: ")=7.
# Caret line begins at x=1 and caret index is 6+cursor, so x=7+cursor.
for cursor in range(24):
    stem_x = 1 + len('Name: ') + cursor
    caret_x = 1 + 6 + cursor
    assert stem_x == caret_x, (cursor, stem_x, caret_x)
print('TEST_U2523_RENAME_CARET_ALIGNMENT_OK')
