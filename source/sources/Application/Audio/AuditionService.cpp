#include "AuditionService.h"

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

// BACON_1.5_AUDITION_GAIN (bacon-1.5, feedback): fixed +6 dB lift on the
// audition note.  The user reported the preview was barely audible next to
// recorded samples; the song channels can sit at full scale while a synth
// voice sits lower, so the preview gets a flat +6 dB with the master clip
// guarding the top (the audition bus is unclipped, like the song buses).
static const fixed kAuditionGain = 0x20000;  // fl2fp(2.0f), 16.16

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
            // BACON_1.5_ANALYZER_MIX: the analyzer tap moved to the master
            // output, so the audition does not feed it anymore.  Apply the
            // fixed gain here, on the audible path only.
            fixed *c = buffer;
            for (int i = 0; i < samplecount * 2; i++) {
                *c = fp_mul(*c, kAuditionGain);
                c++;
            }
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
