#include "MixerService.h"
#include "Application/Audio/DummyAudioOut.h"
#include "Application/Model/Config.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioDriver.h"
#include "Services/Midi/MidiService.h"
#include "Application/Player/SyncMaster.h"
#include "System/Console/Trace.h"
#include "System/FileSystem/FileSystem.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string>
#include <string.h>

static void msRenderDebug(const char *fmt, ...) {
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    line[sizeof(line)-1] = 0;

    I_File *fp = FileSystem::GetInstance()->Open("/mnt/sdcard/lgpt/wav_export_debug.log", "a");
    if (!fp) return;
    fp->Printf("%s\n", line);
    fp->Close();
    SAFE_DELETE(fp);
}

static bool msPathExists(const char *logicalPath) {
    Path path(logicalPath);
    std::string resolved = path.GetPath();
    FileType type = FileSystem::GetInstance()->GetFileType(resolved.c_str());
    msRenderDebug("exists logical=%s resolved=%s type=%d", logicalPath ? logicalPath : "", resolved.c_str(), (int)type);
    return type != FT_UNKNOWN;
}

static bool msDirExists(const char *logicalPath) {
    Path path(logicalPath);
    std::string resolved = path.GetPath();
    return FileSystem::GetInstance()->GetFileType(resolved.c_str()) == FT_DIR;
}

static bool msEnsureDir(const char *logicalPath) {
    if (msDirExists(logicalPath)) {
        Path existing(logicalPath);
        msRenderDebug("dir ok logical=%s resolved=%s", logicalPath ? logicalPath : "", existing.GetPath().c_str());
        return true;
    }
    Path path(logicalPath);
    std::string resolved = path.GetPath();
    Result r = FileSystem::GetInstance()->MakeDir(resolved.c_str());
    if (r.Failed()) {
        Trace::Log("MixerService", "failed to create render directory %s", resolved.c_str());
        msRenderDebug("mkdir failed logical=%s resolved=%s error=%s", logicalPath ? logicalPath : "", resolved.c_str(), r.GetDescription().c_str());
        return false;
    }
    Trace::Log("MixerService", "created render directory %s", resolved.c_str());
    msRenderDebug("mkdir ok logical=%s resolved=%s", logicalPath ? logicalPath : "", resolved.c_str());
    return true;
}

static std::string msUniqueFile(const char *directory, const char *base, const char *extension) {
    char candidate[512];
    sprintf(candidate, "%s/%s%s", directory, base, extension);
    if (!msPathExists(candidate)) return std::string(candidate);

    for (int i = 1; i < 1000; i++) {
        sprintf(candidate, "%s/%s_%03d%s", directory, base, i, extension);
        if (!msPathExists(candidate)) return std::string(candidate);
    }

    sprintf(candidate, "%s/%s_overflow%s", directory, base, extension);
    return std::string(candidate);
}

static std::string msUniqueDir(const char *directory, const char *base) {
    char candidate[512];
    sprintf(candidate, "%s/%s", directory, base);
    if (!msPathExists(candidate)) return std::string(candidate);

    for (int i = 1; i < 1000; i++) {
        sprintf(candidate, "%s/%s_%03d", directory, base, i);
        if (!msPathExists(candidate)) return std::string(candidate);
    }

    sprintf(candidate, "%s/%s_overflow", directory, base);
    return std::string(candidate);
}

static std::string msSanitizeName(const std::string &name) {
    std::string out;
    for (unsigned int i = 0; i < name.size(); i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c) || c == '-' || c == '_') {
            out += (char)c;
        } else {
            out += '_';
        }
    }
    if (out.empty() || out == "project_") out = "current_project";
    return out;
}

static std::string msBasename(const std::string &path) {
    if (path.empty()) return std::string("");
    std::string copy = path;
    while (!copy.empty() && (copy[copy.size() - 1] == '/' || copy[copy.size() - 1] == '\\')) {
        copy.erase(copy.size() - 1);
    }
    if (copy.empty()) return std::string("");
    std::string::size_type slash = copy.find_last_of("/\\");
    if (slash == std::string::npos) return copy;
    return copy.substr(slash + 1);
}

static std::string msCurrentProjectFolderName() {
    Path projectPath("project:");
    std::string resolved = projectPath.GetPath();
    std::string name = msBasename(resolved);
    return msSanitizeName(name);
}

static std::string msGetExportRootDir() {
    /* U2.39: force the TreeFrog SD data root for exports.
     * Do not use project: and do not trust root: if a previous runtime has
     * rebound it to the current project. The expected SD path is:
     *   /mnt/sdcard/lgpt/exports
     * Windows view:
     *   F:\lgpt\exports
     */
#if defined(PLATFORM_TREEFROG)
    const char *absoluteExports = "/mnt/sdcard/lgpt/exports";
    if (msEnsureDir(absoluteExports)) {
        return std::string(absoluteExports);
    }
    msRenderDebug("absolute export root unavailable; trying root:exports");
#endif
    const char *rootExports = "root:exports";
    if (msEnsureDir(rootExports)) {
        return std::string(rootExports);
    }
    msRenderDebug("export root unavailable; falling back to root:");
    return std::string("root:");
}

MixerService::MixerService() : out_(0), sync_(0), isRendering_(false), offlineSampleOffset_(0), exportProject_(0), exportProjectName_("current_project") {
    mode_ = MSRM_PLAYBACK;
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        exportChannelActive_[i] = false;
        exportChannelInstrument_[i] = 0xFF;
        exportChannelMultiInstrument_[i] = false;
    }
};

MixerService::~MixerService(){};

void MixerService::SetExportContext(Project *project) {
    exportProject_ = project;
    exportProjectName_ = msCurrentProjectFolderName();
    if (exportProjectName_.empty()) exportProjectName_ = "current_project";

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        exportChannelActive_[i] = false;
        exportChannelInstrument_[i] = 0xFF;
        exportChannelMultiInstrument_[i] = false;
    }

    if (!exportProject_ || !exportProject_->song_) {
        msRenderDebug("export context: no project/song name=%s", exportProjectName_.c_str());
        return;
    }

    Song *song = exportProject_->song_;
    InstrumentBank *bank = exportProject_->GetInstrumentBank();
    if (!bank) {
        msRenderDebug("export context: no instrument bank name=%s", exportProjectName_.c_str());
        return;
    }

    for (int row = 0; row < SONG_ROW_COUNT; row++) {
        for (int channel = 0; channel < SONG_CHANNEL_COUNT; channel++) {
            unsigned char chain = song->data_[SONG_CHANNEL_COUNT * row + channel];
            if (chain == 0xFF) continue;

            for (int chainPos = 0; chainPos < 16; chainPos++) {
                unsigned char phrase = song->chain_->data_[16 * chain + chainPos];
                if (phrase == 0xFF) continue;

                for (int phrasePos = 0; phrasePos < 16; phrasePos++) {
                    int offset = 16 * phrase + phrasePos;
                    unsigned char note = song->phrase_->note_[offset];
                    unsigned char instr = song->phrase_->instr_[offset];

                    if (note == 0xFF || instr == 0xFF || instr >= MAX_INSTRUMENT_COUNT) continue;

                    I_Instrument *instrument = bank->GetInstrument(instr);
                    if (!instrument || instrument->IsEmpty()) continue;

                    if (!exportChannelActive_[channel]) {
                        exportChannelActive_[channel] = true;
                        exportChannelInstrument_[channel] = instr;
                    } else if (exportChannelInstrument_[channel] != instr) {
                        exportChannelMultiInstrument_[channel] = true;
                    }
                }
            }
        }
    }

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        msRenderDebug("export context project=%s channel=%d active=%d instr=%d multi=%d",
                      exportProjectName_.c_str(), i + 1,
                      exportChannelActive_[i] ? 1 : 0,
                      exportChannelInstrument_[i],
                      exportChannelMultiInstrument_[i] ? 1 : 0);
    }
}

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

void MixerService::clearRenderTargets() {
    master_.SetFileRenderer("");
    if (out_) out_->SetFileRenderer("");
    for (int i = 0; i < MAX_BUS_COUNT; i++) {
        bus_[i].SetFileRenderer("");
    }
}

bool MixerService::prepareRenderTargets(MixerServiceRenderMode mode) {
    clearRenderTargets();

    if (mode == MSRM_PLAYBACK) return true;
    if (!out_) return false;

    std::string exportDir = msGetExportRootDir();
    std::string projectName = exportProjectName_.empty() ? msCurrentProjectFolderName() : exportProjectName_;
    if (projectName.empty()) projectName = "current_project";

    std::string projectDir = exportDir + "/" + projectName;
    if (!msEnsureDir(projectDir.c_str())) {
        projectDir = exportDir;
    }

    const char *targetDir = projectDir.c_str();
    Path resolvedTarget(targetDir);
    Trace::Log("MixerService", "WAV export directory logical=%s resolved=%s", targetDir, resolvedTarget.GetPath().c_str());
    msRenderDebug("prepare mode=%d project=%s target logical=%s resolved=%s", (int)mode, projectName.c_str(), targetDir, resolvedTarget.GetPath().c_str());

    switch(mode) {
    case MSRM_STEREO:
    {
        std::string file = msUniqueFile(targetDir, projectName.c_str(), ".wav");
        out_->SetFileRenderer(file.c_str());
        Trace::Log("MixerService", "song WAV target: %s", file.c_str());
        msRenderDebug("song target %s", file.c_str());
        return true;
    }
    case MSRM_STEMS:
    {
        std::string stemDir = std::string(targetDir) + "/multitrack";
        bool haveStemDir = msEnsureDir(stemDir.c_str());
        const char *stemTargetDir = haveStemDir ? stemDir.c_str() : targetDir;
        bool anyStem = false;
        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            if (!exportChannelActive_[i]) {
                bus_[i].SetFileRenderer("");
                msRenderDebug("skip multitrack channel=%d reason=no_explicit_instrument", i + 1);
                continue;
            }

            std::string instrName = "instrument";
            if (exportProject_ && exportChannelInstrument_[i] < MAX_INSTRUMENT_COUNT) {
                I_Instrument *instr = exportProject_->GetInstrumentBank()->GetInstrument(exportChannelInstrument_[i]);
                if (instr && instr->GetName()) instrName = instr->GetName();
            }
            instrName = msSanitizeName(instrName);
            if (exportChannelMultiInstrument_[i]) instrName += "_multi";

            char base[512];
            sprintf(base, "%s_%s_track_%02d", projectName.c_str(), instrName.c_str(), i + 1);
            std::string file = msUniqueFile(stemTargetDir, base, ".wav");
            bus_[i].SetFileRenderer(file.c_str());
            anyStem = true;
            Trace::Log("MixerService", "multitrack WAV target: %s", file.c_str());
            msRenderDebug("stem target channel=%d instr=%s path=%s", i + 1, instrName.c_str(), file.c_str());
        }
        return anyStem;
    }
    case MSRM_PLAYBACK:
    default:
        return true;
    }
}

void MixerService::initRendering(MixerServiceRenderMode mode) {
    prepareRenderTargets(mode);
}

void MixerService::Close() {
    if (out_) {
        out_->RemoveObserver(*MidiService::GetInstance());
        out_->Close() ;
        out_->Empty() ;
        master_.Empty() ;
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
    if (!isRendering_) {
        initRendering(mode_);
    }
}

bool MixerService::IsRendering() { return isRendering_; }


bool MixerService::BeginOfflineRendering(int mode) {
    mode_ = MixerServiceRenderMode(mode);
    offlineSampleOffset_ = 0.0f;
    prepareRenderTargets(mode_);
    toggleRendering(true);
    msRenderDebug("offline begin mode=%d active=%d", mode, isRendering_ ? 1 : 0);
    return isRendering_;
}

void MixerService::EndOfflineRendering() {
    msRenderDebug("offline end active=%d", isRendering_ ? 1 : 0);
    toggleRendering(false);
    offlineSampleOffset_ = 0.0f;
}

void MixerService::RenderOfflineSlice() {
    if (!out_) return;

    SyncMaster *sync = SyncMaster::GetInstance();
    offlineSampleOffset_ += sync->GetPlaySampleCount();
    int sampleCount = (int)offlineSampleOffset_;
    if (sampleCount <= 0) sampleCount = 1;
    offlineSampleOffset_ -= sampleCount;

    fixed *buffer = (fixed *)SYS_MALLOC(sampleCount * 2 * sizeof(fixed));
    if (!buffer) return;
    memset(buffer, 0, sampleCount * 2 * sizeof(fixed));

    Lock();
    SetChanged();
    NotifyObservers();
    out_->Render(buffer, sampleCount);
    Unlock();

    SYS_FREE(buffer);
}

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

void MixerService::SetMasterVolume(int attn) { out_->SetMasterVolume(attn); }

int MixerService::GetPlayedBufferPercentage() {
    return out_->GetPlayedBufferPercentage() ;
}

int MixerService::GetMasterPeakLeft() const {
    return out_ ? out_->GetLastPeakLeft() : 0 ;
}

int MixerService::GetMasterPeakRight() const {
    return out_ ? out_->GetLastPeakRight() : 0 ;
}

void MixerService::toggleRendering(bool enable) {
    if (enable == isRendering_) return;

    switch (mode_) {
    case MSRM_PLAYBACK:
        isRendering_ = false;
        initRendering(MSRM_PLAYBACK);
        break;
    case MSRM_STEREO:
        if (enable) prepareRenderTargets(MSRM_STEREO);
        out_->EnableRendering(enable);
        isRendering_ = enable && out_->IsRendering();
        break;
    case MSRM_STEMS:
    {
        if (enable) prepareRenderTargets(MSRM_STEMS);
        bool anyRendering = false;
        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            bus_[i].EnableRendering(enable);
            anyRendering = anyRendering || bus_[i].IsRendering();
        };
        isRendering_ = enable && anyRendering;
        break;
    }
    }
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
