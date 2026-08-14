#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'source/sources/Application/UI/Views/ModalDialogs/UsbRecordModal.cpp').read_text()
h=(root/'source/sources/Application/UI/Views/ModalDialogs/UsbRecordModal.h').read_text()
required=[
 'U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY',
 '"/tmp/r36sx_lgpt_record"',
 'makeTemporaryCapturePath();',
 'promoteCaptureToFinalPath(',
 'O_WRONLY | O_CREAT | O_EXCL',
 'fsync(destination)',
 'unlink(sourcePath)',
 'TreeFrogUac2Bridge_CommitUsbCapture();',
 'Save or Discard the current take first',
 'Unsaved take is temporary until Save',
]
for marker in required:
    assert marker in cpp or marker in h, marker
start=cpp.index('void UsbRecordModal::startRecording()')
save=cpp.index('void UsbRecordModal::saveRecording()')
assert cpp.index('makeTemporaryCapturePath();', start, save) > start
assert cpp.index('promoteCaptureToFinalPath(', save) > save
assert cpp.index('TreeFrogUac2Bridge_CommitUsbCapture();', save) > cpp.index('promoteCaptureToFinalPath(', save)
print('TEST_U2520_TRANSACTIONAL_RECORD_SOURCE_OK')
