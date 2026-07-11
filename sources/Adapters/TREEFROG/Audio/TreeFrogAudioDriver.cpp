#include "TreeFrogAudioDriver.h"
#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"

#include <string.h>
#include <stdio.h>
#include "Services/Midi/MidiService.h"
#include "System/System/System.h"

#ifndef TREEFROG_AUDIO_MODE
#define TREEFROG_AUDIO_MODE 0
#endif

#ifndef TREEFROG_INPUT_DEBUG
#define TREEFROG_INPUT_DEBUG 0
#endif

#ifndef TREEFROG_AUDIO_DEBUG
#define TREEFROG_AUDIO_DEBUG 0
#endif

#define TREEFROG_AUDIO_FRAME_BYTES ((int)(2 * sizeof(int16_t)))
#define TREEFROG_AUDIO_MAX_LAZY_REQUESTS 2
#define TREEFROG_AUDIO_MAX_BUFFER_BYTES (512 * 1024)

static TreeFrogAudioDriver *g_audio_driver = 0;
static bool g_treefrog_playback_armed_v14 = false;
TreeFrogAudioDriver *TreeFrogGetAudioDriver() { return g_audio_driver; }
extern "C" void TreeFrogAudioSetPlaybackArmed(int armed) {
    g_treefrog_playback_armed_v14 = (armed != 0);
    if (g_audio_driver) g_audio_driver->SetPlaybackArmed(g_treefrog_playback_armed_v14);
}
extern "C" int TreeFrogAudioIsPlaybackArmed(void) { return g_treefrog_playback_armed_v14 ? 1 : 0; }

TreeFrogAudioDriver::TreeFrogAudioDriver(AudioSettings &settings)
: AudioDriver(settings), startClock_(0), currentBufferOffset_(0), rendering_(false), requestingBuffer_(false), lazyRequestsThisRender_(0) {
    g_audio_driver = this;
}

TreeFrogAudioDriver::~TreeFrogAudioDriver() {
    ResetPlaybackState();
    if (g_audio_driver == this) g_audio_driver = 0;
}

void TreeFrogAudioDriver::logAudio(const char *msg) {
#if TREEFROG_AUDIO_DEBUG
    FILE *f = fopen("/mnt/sdcard/lgpt/audio_debug.log", "a");
    if (!f) return;
    fprintf(f, "%lu %s q=%d p=%d off=%d playing=%d rendering=%d lazy=%d\n",
            (unsigned long)System::GetInstance()->GetClock(),
            msg ? msg : "audio",
            poolQueuePosition_, poolPlayPosition_, currentBufferOffset_,
            isPlaying_ ? 1 : 0, rendering_ ? 1 : 0, lazyRequestsThisRender_);
    fclose(f);
#else
    (void)msg;
#endif
}

void TreeFrogAudioDriver::resetQueue() {
    for (int i = 0; i < SOUND_BUFFER_COUNT; ++i) {
        if (pool_[i].buffer_) {
            SYS_FREE(pool_[i].buffer_);
            pool_[i].buffer_ = 0;
        }
        pool_[i].size_ = 0;
        pool_[i].driverData_ = 0;
    }
    poolQueuePosition_ = 0;
    poolPlayPosition_ = 0;
    bufferPos_ = 0;
    hasData_ = false;
    currentBufferOffset_ = 0;
    lazyRequestsThisRender_ = 0;
}

void TreeFrogAudioDriver::ResetPlaybackState() {
    isPlaying_ = false;
    rendering_ = false;
    requestingBuffer_ = false;
    resetQueue();
}


void TreeFrogAudioDriver::SetPlaybackArmed(bool armed) {
    if (armed) {
        logAudio("V14 SetPlaybackArmed.on");
        resetQueue();
        TreeFrogUac2Bridge_ResetTransport();
        startClock_ = System::GetInstance()->GetClock();
        isPlaying_ = true;
    } else {
        logAudio("V14 SetPlaybackArmed.off");
        ResetPlaybackState();
    }
}

bool TreeFrogAudioDriver::IsPlaybackArmed() const {
    return g_treefrog_playback_armed_v14;
}

bool TreeFrogAudioDriver::InitDriver() {
    ResetPlaybackState();
    TreeFrogUac2Bridge_Prime();
    logAudio("InitDriver");
    return true;
}

void TreeFrogAudioDriver::CloseDriver() {
    logAudio("CloseDriver.enter");
    ResetPlaybackState();
    TreeFrogUac2Bridge_Close();
    logAudio("CloseDriver.leave");
}

bool TreeFrogAudioDriver::StartDriver() {
    TreeFrogUac2Bridge_Prime();
    TreeFrogUac2Bridge_ResetTransport();
    logAudio("StartDriver.enter.V14.ready_not_playing");

    /* V14 RenderGuard: AudioOut may call StartDriver during application/audio
     * initialisation. That must not mean LGPT playback is active. Rendering is
     * armed only by Player::Start() through TreeFrogAudioSetPlaybackArmed(1). */
    resetQueue();
    startClock_ = System::GetInstance()->GetClock();
    isPlaying_ = g_treefrog_playback_armed_v14;

    logAudio(isPlaying_ ? "StartDriver.leave.V14.armed" : "StartDriver.leave.V14.idle");
    return true;
}

void TreeFrogAudioDriver::StopDriver() {
    logAudio("StopDriver.enter.V14");
    g_treefrog_playback_armed_v14 = false;
    ResetPlaybackState();
    TreeFrogUac2Bridge_Close();
    logAudio("StopDriver.leave.V14");
}

bool TreeFrogAudioDriver::Interlaced() {
    return true; /* libretro wants L,R,L,R int16 frames */
}

int TreeFrogAudioDriver::GetPlayedBufferPercentage() {
    AudioBufferData &slot = pool_[poolPlayPosition_];
    if (!slot.buffer_ || slot.size_ <= 0) return 0;
    if (currentBufferOffset_ < 0) return 0;
    if (currentBufferOffset_ > slot.size_) return 100;
    return (currentBufferOffset_ * 100) / slot.size_;
}

double TreeFrogAudioDriver::GetStreamTime() {
    unsigned long now = System::GetInstance()->GetClock();
    return (double)(now - startClock_) / 1000.0;
}

bool TreeFrogAudioDriver::validateCurrentBuffer() {
    AudioBufferData &slot = pool_[poolPlayPosition_];

    if (!slot.buffer_) return false;

    if (slot.size_ < TREEFROG_AUDIO_FRAME_BYTES ||
        slot.size_ > TREEFROG_AUDIO_MAX_BUFFER_BYTES ||
        (slot.size_ % TREEFROG_AUDIO_FRAME_BYTES) != 0) {
        logAudio("invalid-buffer.drop");
        SYS_FREE(slot.buffer_);
        slot.buffer_ = 0;
        slot.size_ = 0;
        currentBufferOffset_ = 0;
        poolPlayPosition_ = (poolPlayPosition_ + 1) % SOUND_BUFFER_COUNT;
        return false;
    }

    if (currentBufferOffset_ < 0 || currentBufferOffset_ >= slot.size_) {
        currentBufferOffset_ = 0;
    }

    return true;
}

bool TreeFrogAudioDriver::ensureCurrentBuffer() {
    if (validateCurrentBuffer()) return true;

    if (requestingBuffer_ || lazyRequestsThisRender_ >= TREEFROG_AUDIO_MAX_LAZY_REQUESTS) {
        return false;
    }

    ++lazyRequestsThisRender_;
    requestingBuffer_ = true;
    logAudio("lazy-buffer.request.before-OnNewBufferNeeded.v11");
    OnNewBufferNeeded();
    logAudio("lazy-buffer.request.after-OnNewBufferNeeded.v11");
    requestingBuffer_ = false;

    return validateCurrentBuffer();
}

void TreeFrogAudioDriver::silenceOneFrame(int16_t *dst) {
    dst[0] = 0;
    dst[1] = 0;
}

void TreeFrogAudioDriver::consumeOneFrame(int16_t *dst) {
    if (!ensureCurrentBuffer()) {
        silenceOneFrame(dst);
        return;
    }

    AudioBufferData &slot = pool_[poolPlayPosition_];
    if (!slot.buffer_ || currentBufferOffset_ + TREEFROG_AUDIO_FRAME_BYTES > slot.size_) {
        silenceOneFrame(dst);
        return;
    }

    int16_t *src = (int16_t *)(slot.buffer_ + currentBufferOffset_);
    dst[0] = src[0];
    dst[1] = src[1];
    currentBufferOffset_ += TREEFROG_AUDIO_FRAME_BYTES;

    if (currentBufferOffset_ >= slot.size_) {
        SYS_FREE(slot.buffer_);
        slot.buffer_ = 0;
        slot.size_ = 0;
        currentBufferOffset_ = 0;
        poolPlayPosition_ = (poolPlayPosition_ + 1) % SOUND_BUFFER_COUNT;
        onAudioBufferTick();
        MidiService::GetInstance()->Flush();
    }
}

void TreeFrogAudioDriver::Render(int16_t *dst, int frames) {
    if (!dst || frames <= 0) return;

    if (rendering_) {
        memset(dst, 0, frames * 2 * sizeof(int16_t));
        return;
    }

    rendering_ = true;
    lazyRequestsThisRender_ = 0;

#if TREEFROG_AUDIO_MODE == 1
    /* Diagnostic mode: accept playback state changes but keep the libretro
     * stream silent.  Useful to isolate non-audio START crashes. */
    memset(dst, 0, frames * 2 * sizeof(int16_t));
    rendering_ = false;
    return;
#endif

    if (!isPlaying_ || !g_treefrog_playback_armed_v14) {
        /* AU11_USB_MONITOR_IDLE_RENDER: keep the audio callback alive for
         * USB-C RECORD prelisten even when the sequencer/project transport is
         * stopped. This makes the console behave like a sampler input monitor:
         * silence is rendered first, then USB input monitor is mixed locally. */
        memset(dst, 0, frames * 2 * sizeof(int16_t));
        TreeFrogUac2Bridge_MixUsbCaptureMonitorStereo44100(dst, frames);
        rendering_ = false;
        return;
    }

    for (int i = 0; i < frames; ++i) {
        consumeOneFrame(dst + i * 2);
    }

    TreeFrogUac2Bridge_SubmitStereo44100(dst, frames);
    if (TreeFrogUac2Bridge_ShouldMuteLocal()) {
        memset(dst, 0, sizeof(int16_t) * frames * 2);
    }
    TreeFrogUac2Bridge_MixUsbCaptureMonitorStereo44100(dst, frames);

    rendering_ = false;
}
