#include "TreeFrogAudio.h"
#include "TreeFrogAudioDriver.h"
#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"

#include "Foundation/I_Iterator.h"
#include "Services/Audio/AudioOut.h"
#include "Services/Audio/AudioOutDriver.h"

static int g_treefrog_lgpt_mixer_volume_au9v = 100;

TreeFrogAudio::TreeFrogAudio(AudioSettings &hints) : Audio(hints) {
}

TreeFrogAudio::~TreeFrogAudio() {
}

void TreeFrogAudio::Init() {
    AudioSettings settings;
    settings.audioAPI_ = "libretro";
    settings.audioDevice_ = "picoarch";
    settings.bufferSize_ = GetAudioBufferSize();
    settings.preBufferCount_ = GetAudioPreBufferCount();

    TreeFrogAudioDriver *drv = new TreeFrogAudioDriver(settings);
    AudioOut *out = new AudioOutDriver(*drv);
    Insert(out);
}

void TreeFrogAudio::Close() {
    I_Iterator<AudioOut> *it = GetIterator();

    if (!it) {
        return;
    }

    for (it->Begin(); !it->IsDone(); it->Next()) {
        AudioOut &current = it->CurrentItem();
        current.Close();
    }

    delete it;
}

int TreeFrogAudio::GetMixerVolume() {
    return g_treefrog_lgpt_mixer_volume_au9v;
}

void TreeFrogAudio::SetMixerVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_treefrog_lgpt_mixer_volume_au9v = volume;
    TreeFrogUac2Bridge_SetMixerVolumePercent(volume);
}
