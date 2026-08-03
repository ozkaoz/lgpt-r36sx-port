#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'source/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp').read_text()
h=(root/'source/sources/Application/Views/ModalDialogs/ImportSampleDialog.h').read_text()
assert 'U2522_NESTED_RENAME_FRAME_FORWARDING_CRASH_SAFE' in cpp
assert 'void ImportSampleDialog::OnFrameUpdate(unsigned long frameClock)' in cpp
assert 'if (HasModal()) UpdateActiveModalFrame(frameClock);' in cpp
assert 'virtual void OnFrameUpdate(unsigned long frameClock)' in h
assert 'U2522' in cpp
assert 'physical-edge input FSM never runs' in cpp
segment = cpp[cpp.index('void ImportSampleDialog::OnFrameUpdate'):cpp.index('void ImportSampleDialog::OnFocus()')]
assert 'UpdateActiveModalFrame(frameClock)' in segment
print('TEST_U2522_NESTED_RENAME_FRAME_FORWARDING_OK')
