#ifndef TREEFROG_AUDIO_DRIVER_H
#define TREEFROG_AUDIO_DRIVER_H

#include <stdint.h>
#include "Services/Audio/AudioDriver.h"

class TreeFrogAudioDriver: public AudioDriver {
public:
    TreeFrogAudioDriver(AudioSettings &settings);
    virtual ~TreeFrogAudioDriver();

    virtual bool InitDriver();
    virtual void CloseDriver();
    virtual bool StartDriver();
    virtual void StopDriver();
    virtual bool Interlaced();
    virtual int GetPlayedBufferPercentage();
    virtual double GetStreamTime();

    void Render(int16_t *dst, int frames);
    void ResetPlaybackState();
    void SetPlaybackArmed(bool armed);
    bool IsPlaybackArmed() const;

private:
    bool ensureCurrentBuffer();
    bool validateCurrentBuffer();
    void consumeOneFrame(int16_t *dst);
    void silenceOneFrame(int16_t *dst);
    void resetQueue();
    void logAudio(const char *msg);

    unsigned long startClock_;
    int currentBufferOffset_;
    bool rendering_;
    bool requestingBuffer_;
    int lazyRequestsThisRender_;
};

TreeFrogAudioDriver *TreeFrogGetAudioDriver();
extern "C" void TreeFrogAudioSetPlaybackArmed(int armed);
extern "C" int TreeFrogAudioIsPlaybackArmed(void);

#endif
