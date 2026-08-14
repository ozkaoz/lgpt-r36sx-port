#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
pool=(root/'source/sources/Application/Instruments/SamplePool.cpp').read_text()
header=(root/'source/sources/Application/Instruments/SamplePool.h').read_text()
variable=(root/'source/sources/Application/Instruments/SampleVariable.cpp').read_text()
modal=(root/'source/sources/Application/UI/Views/ModalDialogs/ImportSampleDialog.cpp').read_text()
start=pool.index('void SamplePool::PurgeSample')
end=pool.index('static bool RenamePathWithCaseSupport',start)
body=pool[start:end]
for marker in [
 'two-phase', 'retiredWav_.push_back(retiredSource)',
 'SAFE_FREE(retiredName)', 'SPET_DELETE',
]: assert marker in body, marker
assert 'SAFE_DELETE(wav_[i])' not in body
assert 'std::vector<SoundSource *> retiredWav_' in header
assert 'ReleaseRetiredSources' in pool
assert 'if (value_.index_==e->index_)' in variable
assert 'SetInt(-1)' in variable
assert 'TimeService::GetInstance()->Sleep(60)' in modal
assert modal.index('LGPTChopperOnSamplePoolDelete(projectIndex)') < modal.index('pool->PurgeSample(projectIndex)')
print('TEST_U2521_DEFERRED_SAMPLE_DELETE_OK')
