#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'source/sources/Application/UI/Views/ModalDialogs/UsbRecordModal.cpp').read_text()
required=[
 'U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY',
 'const bool pendingTake',
 'TAKE PENDING: PREVIEW/SAVE/DISCARD',
 'item == ITEM_PREVIEW',
 'item == ITEM_SAVE',
 'item == ITEM_DISCARD',
 'SetColor(CD_HILITE2)',
 'SetColor(CD_RECORD)',
]
for marker in required: assert marker in cpp, marker
assert cpp.index('TAKE PENDING: PREVIEW/SAVE/DISCARD') < cpp.index('static const char *labels[ITEM_COUNT]')
print('TEST_U2520_PENDING_TAKE_INDICATOR_OK')
