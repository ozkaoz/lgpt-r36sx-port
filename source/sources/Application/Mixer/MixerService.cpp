#include "MixerService.h"
#include "Application/Audio/DummyAudioOut.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Model/Config.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioDriver.h"
#include "Services/Midi/MidiService.h"
#include "System/Console/Trace.h"
#include "System/FileSystem/FileSystem.h"

// bacon-1.5 item 8: minimum free bytes required to start an explicit WAV
// export (mixdown or stems).  128 MB covers the full stem set (8 tracks +
// delay return + reverb return + master) for a typical render length.
// GetFreeSpace() returning -1 (unknown, e.g. DummyFileSystem on host tests)
// always allows the export, so behaviour is unchanged there.
static const long long kMinExportFreeBytes = 128LL * 1024 * 1024;

MixerService::MixerService() : out_(0), sync_(0), isRendering_(false),
    captureDelay_(0), captureReverb_(0), captureMaster_(0) {
    mode_ = MSRM_PLAYBACK;
};

MixerService::~MixerService(){};

/*
 * initializes the mixer service, config changes depending if we're in sequencer or render mode
 */
bool MixerService::Init() {
    // create the output depending on rendering mode
    out_ = 0;
	switch (mode_) {
    case MSRM_STEREO:
    case MSRM_STEMS:
        out_ = new DummyAudioOut();
        break;
    default:
        Audio *audio = Audio::GetInstance();
        out_ = audio->GetFirst();
        break;
	}

	for (int i=0;i<MAX_BUS_COUNT;i++) {
		master_.Insert(bus_[i]);
	}

	// TREEFROG_MIXER_STEREO_METERS_V2 (Bacon 1.1.1): channel/stream buses run
	// UNCLIPPED so the master sum -- and its pre-clip meter -- reflects the
	// real level of each channel.  Before this, every bus hard-clipped its
	// output at 1.0, so a single hot channel (e.g. a 0 dBFS kick at volume
	// 127) reached the master as exactly 1.0 and the master bar could never
	// pass 0 dB.  The master bus and the audio out still hard-clip, so the
	// int16 conversion stays guarded; the meters are measured pre-clip.
	for (int i=0;i<MAX_BUS_COUNT;i++) {
		bus_[i].SetClipBypass(true);
	}

	bool result = false;
	if (out_) {
		result = out_->Init();
		if (result) {
			out_->Insert(master_);
		}

        initRendering(mode_);
        out_->AddObserver(*MidiService::GetInstance());
	}

	sync_=SDL_CreateMutex();
	NAssert(sync_);

	if (result) {
		Trace::Log("MixerService", "output initialized");
	} else {
		Trace::Log("MixerService", "failed to initialize output");
	}
	return (result);
};

void MixerService::initRendering(MixerServiceRenderMode mode) {
    switch(mode) {
    case MSRM_PLAYBACK:
        break;
    case MSRM_STEREO:
        out_->SetFileRenderer("project:mixdown.wav");
        break;
    case MSRM_STEMS:
        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            char buffer[1024];
            sprintf(buffer, "project:channel%d.wav", i);
            bus_[i].SetFileRenderer(buffer);
        }
        break;
    }
}

void MixerService::Close() {
	if (out_) {
    out_->RemoveObserver(*MidiService::GetInstance());
		out_->Close() ;
		out_->Empty() ;
		master_.Empty() ;

		switch(mode_) {
        case MSRM_STEMS:
        case MSRM_STEREO:
            break;
        default:
            break;
        }
    }
   for (int i=0;i<MAX_BUS_COUNT;i++) {
	   bus_[i].Empty() ;
   }
	out_=0 ;
	SDL_DestroyMutex(sync_) ;
	sync_=0 ;
} ;

void MixerService::SetRenderMode(int mode) {
    mode_ = MixerServiceRenderMode(mode);
}

bool MixerService::IsRendering() { return isRendering_; }

bool MixerService::Start() {
    MidiService::GetInstance()->Start();
    if (out_) {
        out_->AddObserver(*this);
        out_->Start();
     }
	return true ;
} ;

void MixerService::Stop() {
	MidiService::GetInstance()->Stop() ;
     if (out_) {
      out_->Stop() ;
      out_->RemoveObserver(*this) ;
     }
}

MixBus *MixerService::GetMixBus(int i) {
	return &(bus_[i]) ;
} ;

void MixerService::Update(Observable &o,I_ObservableData *d)  {

  AudioDriver::Event *event=(AudioDriver::Event *)d;
  if (event->type_ == AudioDriver::Event::ADET_BUFFERNEEDED)
  {  
    Lock() ;
    SetChanged() ;
    NotifyObservers() ;

    out_->Trigger();
    Unlock();
  }
}

bool MixerService::Clipped() {
     return out_->Clipped() ;
} ;

void MixerService::SetPregain(int vol) {
    Mixer *mixer = Mixer::GetInstance();

    fixed masterVolume = fp_mul(i2fp(vol), fl2fp(0.01f));

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        bus_[i].SetVolume(masterVolume);
  }
};

void MixerService::SetSoftclip(int clip, int gain) {
    out_->SetSoftclip(clip, gain);
}

void MixerService::SetMasterVolume(int attn) {
    // TREEFROG_MIXER_STEREO_METERS_V2 (Bacon 1.1.1): the master fader now
    // feeds the master BUS instead of out_.  out_'s damp ran AFTER the bus
    // metering, so turning the fader changed the sound but never the master
    // bar.  With the damp on the bus (applied pre-scan, pre-clip, cached per
    // object) GetMasterPeakL/R reads the real pre-clip level INCLUDING the
    // fader, and the audible output is unchanged: bus damp + master clip ==
    // the old out_ damp + clip (out_ stays at 100 / damp 1.0).
    master_.SetMasterVolume(attn);
}

int MixerService::GetPlayedBufferPercentage() {
	return out_->GetPlayedBufferPercentage() ;
}

void MixerService::toggleRendering(bool enable) {
    isRendering_ = enable;
    switch (mode_) {
    case MSRM_PLAYBACK:
        initRendering(MSRM_PLAYBACK);
        break;
    case MSRM_STEREO:
        initRendering(MSRM_STEREO);
        if (enable && !exportSpaceAvailable()) {
            Trace::Log("MixerService", "mixdown render aborted: low free space");
            break;
        }
        out_->EnableRendering(enable);
        break;
    case MSRM_STEMS:
        initRendering(MSRM_STEMS);
        if (enable) {
            // bacon-1.5 item 8: guard the export against filling the SD,
            // then open the track writers AND the FX return/master stems.
            if (!exportSpaceAvailable()) {
                Trace::Log("MixerService", "stems render aborted: low free space");
                break;
            }
            for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
                bus_[i].EnableRendering(true);
            }
            enableStemsCapture();
        } else {
            for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
                bus_[i].EnableRendering(false);
            }
            disableStemsCapture();
        }
        break;
    }
}

// bacon-1.5 item 8: free-space probe on the project filesystem.  -1 (unknown)
// means "allow"; otherwise require at least kMinExportFreeBytes.
bool MixerService::exportSpaceAvailable() {
    Path p("project:");
    long long free =
        FileSystem::GetInstance()->GetFreeSpace(p.GetPath().c_str());
    return free < 0 || free >= kMinExportFreeBytes;
}

// bacon-1.5 item 8: open the three stems writers and hand them to the
// FxEngine.  On any open failure the whole capture is aborted (no partial
// files left behind).  Control-rate, never in the audio thread.
void MixerService::enableStemsCapture() {
    if (captureMaster_) return; // already open
    WavFileWriter *d = new WavFileWriter("project:delayret.wav");
    WavFileWriter *r = new WavFileWriter("project:reveret.wav");
    WavFileWriter *m = new WavFileWriter("project:master.wav");
    if (!d->IsOpen() || !r->IsOpen() || !m->IsOpen()) {
        m->Close();
        r->Close();
        d->Close();
        delete m;
        delete r;
        delete d;
        Trace::Log("MixerService", "stems capture aborted: cannot open output files");
        return;
    }
    captureDelay_ = d;
    captureReverb_ = r;
    captureMaster_ = m;
    FxEngine::FxEngine::GetInstance().EnableStemsCapture(d, r, m);
}

void MixerService::disableStemsCapture() {
    FxEngine::FxEngine::GetInstance().EnableStemsCapture(0, 0, 0);
    if (captureMaster_) captureMaster_->Close();
    if (captureReverb_) captureReverb_->Close();
    if (captureDelay_) captureDelay_->Close();
    delete captureDelay_;
    captureDelay_ = 0;
    delete captureReverb_;
    captureReverb_ = 0;
    delete captureMaster_;
    captureMaster_ = 0;
}

void MixerService::OnPlayerStart() {
	toggleRendering(true) ;
} ;

void MixerService::OnPlayerStop() {
	toggleRendering(false) ;
} ;

void MixerService::Execute(FourCC id,float value) {
     if (value>0.5) {
        Audio *audio=Audio::GetInstance() ;
        int volume=audio->GetMixerVolume() ;
        switch(id) {
           case TRIG_VOLUME_INCREASE:
                if (volume<100) volume+=1 ;
                break ;
           case TRIG_VOLUME_DECREASE:
                if (volume>0) volume-=1 ;
                break ;                       
        } ;
        audio->SetMixerVolume(volume) ;
     } ;
}

AudioOut *MixerService::GetAudioOut() {
	return out_ ;
} ;


void MixerService::Lock() {
	if (sync_) SDL_LockMutex(sync_) ;
}

void MixerService::Unlock() {
	if (sync_) SDL_UnlockMutex(sync_) ;
}
