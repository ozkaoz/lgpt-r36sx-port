#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'source/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp').read_text()
h=(root/'source/sources/Application/Views/ModalDialogs/ImportSampleDialog.h').read_text()
assert 'U2522_NESTED_RENAME_FRAME_FORWARDING_CRASH_SAFE' in cpp
assert 'virtual void OnFrameUpdate(unsigned long)' in cpp
assert 'void ImportSampleDialog::OnFrameUpdate(unsigned long frameClock)' in cpp
assert 'if (HasModal()) UpdateActiveModalFrame(frameClock);' in cpp
assert 'virtual void OnFrameUpdate(unsigned long frameClock)' in h
assert 'Physical-edge input is authoritative while this editor is open.' in cpp
# The nested editor must no longer rely on player transport updates.
segment=cpp[cpp.index('class ImportBrowserRenameModal'):cpp.index('bool ImportSampleDialog::initStatic_')]
assert 'virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}' in segment
assert 'virtual void OnFrameUpdate(unsigned long)' in segment
assert segment.index('virtual void OnFrameUpdate(unsigned long)') < segment.index('void processPhysicalInput()')
print('TEST_U2522_NESTED_RENAME_FRAME_FORWARDING_OK')
