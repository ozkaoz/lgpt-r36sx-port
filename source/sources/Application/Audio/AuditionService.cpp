#include "AuditionService.h"

#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Instruments/I_Instrument.h"
#include "Application/Mixer/MixerService.h"
#include "System/System/System.h"

AuditionChannel::AuditionChannel() : instr_(0), noteActive_(false) {}

void AuditionChannel::SetInstrument(I_Instrument *instr) {
    if (instr_ && instr_ != instr) {
        instr_->Stop(AUDITION_CHANNEL_INDEX);
    }
    instr_ = instr;
}

void AuditionChannel::StartNote(unsigned char note) {
    if (!instr_) return;
    if (instr_->Start(AUDITION_CHANNEL_INDEX, note, true)) {
        noteActive_ = true;
    }
}

void AuditionChannel::StopNote() {
    if (instr_) {
        instr_->Stop(AUDITION_CHANNEL_INDEX);
    }
    noteActive_ = false;
}

bool AuditionChannel::Render(fixed *buffer, int samplecount) {
    bool audible = false;
    if (instr_ && noteActive_) {
        // Same render contract as a song channel (the instrument does its
        // own filter + EQ8), with a FIXED full gain and center pan: the
        // audition must sound even when the track is muted / volume 0.
        // No FX sends from the preview bus (dry only, no tail artifacts).
        bool status =
            instr_->Render(AUDITION_CHANNEL_INDEX, buffer, samplecount, false);
        audible = status;
        if (audible) {
            // BACON_1.5_ANALYZER_TAP: post-EQ / pre-gain targeted tap, same
            // point as PlayerChannel.
            SpectrumAnalyzer::Get().FeedChannel(AUDITION_CHANNEL_INDEX,
                                                instr_, buffer, samplecount);
        }
    }
    return audible;
}

AuditionService::AuditionService()
    : previewStopClock_(0), plugged_(false) {}

bool AuditionService::Init() {
    if (plugged_) return true;
    MixerService *ms = MixerService::GetInstance();
    MixBus *bus = ms->GetAuditionBus();
    if (bus) {
        bus->Insert(channel_);
        plugged_ = true;
    }
    return plugged_;
}

void AuditionService::Close() {
    if (!plugged_) return;
    StopPreview();
    MixerService *ms = MixerService::GetInstance();
    MixBus *bus = ms->GetAuditionBus();
    if (bus) {
        bus->Remove(channel_);
    }
    plugged_ = false;
}

void AuditionService::Preview(I_Instrument *instrument, unsigned char note) {
    if (!instrument) return;
    StopPreview();
    channel_.SetInstrument(instrument);
    channel_.StartNote(note);
    previewStopClock_ =
        System::GetInstance()->GetClock() + kPreviewDurationMs;
}

void AuditionService::StopPreview() {
    channel_.StopNote();
    channel_.SetInstrument(0);
}

void AuditionService::Update(unsigned long clock) {
    if (!IsPreviewing()) return;
    if (clock >= previewStopClock_) {
        StopPreview();
    }
}
