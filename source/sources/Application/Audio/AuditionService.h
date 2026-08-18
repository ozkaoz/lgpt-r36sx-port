#ifndef _AUDITION_SERVICE_H_
#define _AUDITION_SERVICE_H_

#include "Foundation/T_Singleton.h"
#include "Services/Audio/AudioModule.h"
#include "Application/Utils/fixed.h"

class I_Instrument;

/*
 * BACON_1.5_AUDITION (bacon-1.5, item 2): dedicated audition/preview bus
 * architecture.
 *
 * The old preview (Player::PreviewNote) hijacked a REAL PlayerChannel of the
 * song mixer and Player::StopPreview() then stopped ALL 8 song channels.
 * That made previews impossible to hear on muted tracks / zero-volume
 * channels, and killing a preview could kill a running pattern.
 *
 * AuditionService owns a single AuditionChannel, plugged into its own
 * MixBus (MixerService::GetAuditionBus(), inserted into the master tree).
 * The audition note renders the SAME I_Instrument (filter + EQ8 included)
 * with a FIXED full gain, center pan and NO track mute/volume/pan, so it
 * always sounds no matter what the mixer tracks are doing.  It feeds the
 * spectrum analyzer tap at the same post-EQ / pre-gain point as a song
 * channel, targeted to the instrument being previewed.
 *
 * Contract:
 *   - Audition is only started while the sequencer is stopped (the views
 *     guard it), so the dedicated channel index never collides with a song
 *     channel.
 *   - StopPreview() touches ONLY the audition channel.  The 8 PlayerChannels
 *     are never stopped by a preview.
 *   - All states are initialized deterministically in the constructors.
 */

#define AUDITION_CHANNEL_INDEX 0

class AuditionChannel : public AudioModule {
public:
    AuditionChannel();
    virtual ~AuditionChannel() {}
    virtual bool Render(fixed *buffer, int samplecount);
    void SetInstrument(I_Instrument *instr);
    void StartNote(unsigned char note);
    void StopNote();
    I_Instrument *GetInstrument() const { return instr_; }

private:
    I_Instrument *instr_;
    bool noteActive_;
};

class AuditionService : public T_Singleton<AuditionService> {
public:
    AuditionService();
    ~AuditionService() {}

    // Called after MixerService::Init (from Player::Init): plugs the
    // audition channel into the audition bus.
    bool Init();
    void Close();

    void Preview(I_Instrument *instrument, unsigned char note);
    void StopPreview();
    bool IsPreviewing() const { return channel_.GetInstrument() != 0; }

    // Called from Player::UpdatePreview (view frame loop): retires the
    // note on its own after kPreviewDurationMs.
    void Update(unsigned long clock);

    AuditionChannel *GetChannel() { return &channel_; }

private:
    static const unsigned long kPreviewDurationMs = 900;
    AuditionChannel channel_;
    unsigned long previewStopClock_;
    bool plugged_;
};

#endif