#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'source/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp').read_text()
h=(root/'source/sources/Application/Views/ModalDialogs/ImportSampleDialog.h').read_text()
pool=(root/'source/sources/Application/Instruments/SamplePool.cpp').read_text()
poolh=(root/'source/sources/Application/Instruments/SamplePool.h').read_text()
required=[
 'U2521_BROWSER_RENAME_DEFERRED_DELETE_CRASH_SAFE',
 'ImportBrowserRenameCallback',
 'L1+X rename   L1+Y delete',
 'const bool renameChord',
 'requestBrowserRename',
 'ConfirmPendingBrowserRename',
 'BrowserRenamePathCaseSafe',
 'Rename requires a .wav name',
 'requestBrowserDelete',
 'Exact duplicate name blocked',
 'CD_HILITE2',
]
for marker in required: assert marker in cpp or marker in h, marker
assert 'bool RenameSample(int i,const char *newName)' in poolh
for marker in ['bool SamplePool::RenameSample', 'SPET_RENAME', 'RenamePathWithCaseSupport']:
    assert marker in pool, marker
assert cpp.index('const bool renameChord') < cpp.index('const bool deleteChord')
print('TEST_U2521_BROWSER_RENAME_OK')
