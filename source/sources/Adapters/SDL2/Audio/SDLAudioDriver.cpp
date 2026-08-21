#include "SDLAudioDriver.h"
#include "Services/Midi/MidiService.h"
#include "Services/Time/TimeService.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"

// BUG2 FIX (Bacon 1.5 FX): SDL2 API - SDLCALL
void SDLCALL sdl_callback(void *userdata, Uint8 *stream, int len) {
    SDLAudioDriver *sound = (SDLAudioDriver *)userdata;
    sound->OnChunkDone(stream, len);
};

SDLAudioDriverThread::SDLAudioDriverThread(SDLAudioDriver *driver) {
    semaphore_=SysSemaphore::Create(0,1024);
    driver_ = driver;
};

bool SDLAudioDriverThread::Execute() {
    while (!shouldTerminate()) {
        semaphore_->Wait();
        driver_->OnNewBufferNeeded();
    };
    SysSemaphore *semaphore = semaphore_;
    semaphore_ = 0;
    delete semaphore;
    return true;
};

void SDLAudioDriverThread::Notify() {
    if (semaphore_) semaphore_->Post();
};

void SDLAudioDriverThread::RequestTermination() {
    SysThread::RequestTermination();
    semaphore_->Post();
    SDL_Delay(10);
}

//-------------------------------------------------------------------------------------------------

SDLAudioDriver::SDLAudioDriver(AudioSettings &settings)
    : AudioDriver(settings), devId_(0), unalignedMain_(0), miniBlank_(0) {
    isPlaying_ = false;
    thread_ = 0;
}

SDLAudioDriver::~SDLAudioDriver() {}

bool SDLAudioDriver::InitDriver() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        Trace::Error("Couldn't init SDL audio subsystem: %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = settings_.bufferSize_;
    want.callback = sdl_callback;
    want.userdata = this;

    SDL_SetHint("APP_NAME", "LittleGPTracker");
    SDL_SetHint("AUDIO_DEVICE_APP_NAME", "LittleGPTracker");

    devId_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (devId_ == 0) {
        Trace::Error("Couldn't open sdl audio: %s\n", SDL_GetError());
        return false;
    }
    const char *driverName = SDL_GetCurrentAudioDriver();
    fragSize_ = have.size;
    unalignedMain_ = (char *)SYS_MALLOC(fragSize_ + SOUND_BUFFER_MAX);
#ifdef _64BIT
    mainBuffer_ = (char *)unalignedMain_;
#else
    mainBuffer_ = (char *)((((int)unalignedMain_) + 1) & (0xFFFFFFFC));
#endif
    Trace::Log("AUDIO", "%s successfully opened with %d samples %d", driverName, fragSize_ / 4, have.freq);
    miniBlank_ = (char *)malloc(fragSize_);
    SYS_MEMSET(miniBlank_, 0, fragSize_);
    return true;
};

void SDLAudioDriver::CloseDriver() {
    if (miniBlank_) { SYS_FREE(miniBlank_); miniBlank_ = 0; }
    if (unalignedMain_) { SYS_FREE(unalignedMain_); unalignedMain_ = 0; };
    if (devId_ != 0) { SDL_CloseAudioDevice(devId_); devId_ = 0; }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
};

bool SDLAudioDriver::StartDriver() {
    thread_ = new SDLAudioDriverThread(this);
    thread_->Start();
    short blank[4000];
    SYS_MEMSET(blank, 0, 4000);
    bufferPos_ = 0;
    bufferSize_ = 0;
    for (int i = 0; i < settings_.preBufferCount_; i++) {
        AddBuffer((short *)miniBlank_, fragSize_ / 4);
        MidiService::GetInstance()->AdvancePlayQueue();
    }
    if (settings_.preBufferCount_ == 0) thread_->Notify();
    SDL_PauseAudioDevice(devId_, 0);
    startTime_ = SDL_GetTicks();
    return 1;
};

void SDLAudioDriver::StopDriver() {
    if (thread_) {
        thread_->RequestTermination();
        SysThread *thread = thread_;
        thread_ = 0;
        SDL_PauseAudioDevice(devId_, 1);
        delete thread;
    };
};

double SDLAudioDriver::GetStreamTime() { return (SDL_GetTicks() - startTime_) / 1000.0; }

void SDLAudioDriver::OnChunkDone(Uint8 *stream, int len) {
    while (bufferSize_ - bufferPos_ < len) {
        memmove(mainBuffer_, mainBuffer_ + bufferPos_, bufferSize_ - bufferPos_);
        if (pool_[poolPlayPosition_].buffer_ == 0) {
            SYS_MEMCPY(mainBuffer_+bufferSize_-bufferPos_, miniBlank_, fragSize_);
            bufferSize_=bufferSize_-bufferPos_+fragSize_ ;
            bufferPos_ = 0;
        } else {
            memcpy(mainBuffer_ + bufferSize_ - bufferPos_, pool_[poolPlayPosition_].buffer_, pool_[poolPlayPosition_].size_);
            bufferSize_ = bufferSize_ - bufferPos_ + pool_[poolPlayPosition_].size_;
            bufferPos_ = 0;
            SYS_FREE(pool_[poolPlayPosition_].buffer_);
            pool_[poolPlayPosition_].buffer_ = 0;
            poolPlayPosition_ = (poolPlayPosition_ + 1) % SOUND_BUFFER_COUNT;
            if (thread_) thread_->Notify();
            onAudioBufferTick();
            MidiService::GetInstance()->Flush() ;
        }
    }
    SYS_MEMCPY(stream, (short *)(mainBuffer_ + bufferPos_), len);
    onAudioBufferTick();
    bufferPos_ += len;
}

int SDLAudioDriver::GetPlayedBufferPercentage() { return 0; };
