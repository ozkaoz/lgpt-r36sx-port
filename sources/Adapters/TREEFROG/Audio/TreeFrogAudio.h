#ifndef TREEFROG_AUDIO_H
#define TREEFROG_AUDIO_H

#include "Services/Audio/Audio.h"

class TreeFrogAudio: public Audio {
public:
    TreeFrogAudio(AudioSettings &hints);
    virtual ~TreeFrogAudio();
    virtual void Init();
    virtual void Close();
    virtual int GetMixerVolume();
    virtual void SetMixerVolume(int volume);
};

#endif
