#include "PianoSynth.h"

#include "Foundation/Variables/Variable.h"
#include "Application/Instruments/FilterV2.h"
#include "Application/Instruments/CommandList.h"
#include "Services/Audio/Audio.h"
#include "System/System/System.h"

#include <math.h>
#include <string.h>

// PIANO_SYNTH (bacon-1.5, item 7): see PianoSynth.h.
//
// DSP chain per sample:
//   voices (sine partials, 256-entry Q15 table + linear interpolation)
//   -> sum -> channel TPT SVF (FilterV2) -> amp env blend -> equal-power pan
//   -> Haas stereo width -> EQ8 (shared variable contract).

static const char *kPianoModeNames[2] = {
    "EP", "TINE"
};

static const char *kPianoPartialNames[3] = {
    "2P", "3P", "4P"
};

static const char *kPianoFTypeNames[3] = {
    "LP", "HP", "BP"
};

// Per-partial mix (index 0 = fundamental).  timbre (0..100) scales the
// upper partials between half and 1.5x the base gain.
static const float kPartialBaseGain[4] = { 1.0f, 0.50f, 0.28f, 0.16f };

// EP: harmonic partials.  TINE: slightly inharmonic (bell-like).
static const float kPartialRatio[2][4] = {
    { 1.00f, 2.00f, 3.00f, 4.00f },
    { 1.00f, 2.31f, 3.62f, 4.97f }
};

// 256-entry sine table (Q15 fixed), filled once at Init.
static float gSineTable[256];
static bool gSineTableReady = false;

PianoSynth::PianoSynth() {

    mode_ = new Variable("mode", PNP_MODE, kPianoModeNames, 2, 0);
    Insert(mode_);
    partials_ = new Variable("partials", PNP_PARTIALS, kPianoPartialNames, 3, 1);
    Insert(partials_);
    volume_ = new Variable("volume", PNP_VOLUME, 100);
    Insert(volume_);
    pan_ = new Variable("pan", PNP_PAN, 50);
    Insert(pan_);
    width_ = new Variable("width", PNP_WIDTH, 50);
    Insert(width_);
    timbre_ = new Variable("timbre", PNP_TIMBRE, 40);
    Insert(timbre_);
    pdecay_ = new Variable("pdecay", PNP_PDECAY, 50);
    Insert(pdecay_);
    accent_ = new Variable("accent", PNP_ACCENT, 0);
    Insert(accent_);

    // Piano-like amp envelope: fast attack, long decay, sustain 0 (the
    // sustain stage keeps decaying at half rate for a natural tail).
    attack_ = new Variable("attack", PNP_ATTACK, 5);
    Insert(attack_);
    decay_ = new Variable("decay", PNP_DECAY, 40);
    Insert(decay_);
    sustain_ = new Variable("sustain", PNP_SUSTAIN, 0);
    Insert(sustain_);
    release_ = new Variable("release", PNP_RELEASE, 30);
    Insert(release_);

    ftype_ = new Variable("ftype", PNP_FTYPE, kPianoFTypeNames, 3, 0);
    Insert(ftype_);
    fcut_ = new Variable("fcut", PNP_FCUT, 80);
    Insert(fcut_);
    fres_ = new Variable("fres", PNP_FRES, 20);
    Insert(fres_);
    fenv_ = new Variable("fenv", PNP_FENV, 0);
    Insert(fenv_);
    fatk_ = new Variable("f atk", PNP_FATK, 10);
    Insert(fatk_);
    fdec_ = new Variable("f dec", PNP_FDEC, 20);
    Insert(fdec_);
    fsus_ = new Variable("f sus", PNP_FSUS, 60);
    Insert(fsus_);
    frel_ = new Variable("f rel", PNP_FREL, 30);
    Insert(frel_);

    // TREEFROG_INSTRUMENT_SENDS_V1: same names/ranges as the other types.
    dry_ = new Variable("dry", PNP_DRY, 100);
    Insert(dry_);
    dlySend_ = new Variable("dly send", PNP_DLYSEND, -1);
    Insert(dlySend_);
    rvbSend_ = new Variable("rvb send", PNP_RVBSEND, -1);
    Insert(rvbSend_);

    // TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: shared SIP_EQ* contract.
    eqEnable_ = new Variable("eq bypass", SIP_EQEN, 1);
    Insert(eqEnable_);
    eqMask_ = new Variable("eq bands", SIP_EQMASK, 255);
    Insert(eqMask_);
    for (int i = 0; i < 8; i++) {
        char n[8];
        sprintf(n, "eqf%d", i);
        eqFreq_[i] = new Variable(n, (FourCC)(SIP_EQF0 + i),
            FxEngine::InstrumentEq::DefaultBandHz(i) * 100);
        Insert(eqFreq_[i]);
        sprintf(n, "eqt%d", i);
        eqType_[i] = new Variable(n, (FourCC)(SIP_EQT0 + i), 0);
        Insert(eqType_[i]);
        sprintf(n, "eqg%d", i);
        eqGain_[i] = new Variable(n, (FourCC)(SIP_EQG0 + i), 0);
        Insert(eqGain_[i]);
        sprintf(n, "eqq%d", i);
        eqQ_[i] = new Variable(n, (FourCC)(SIP_EQ_Q0 + i), 100);
        Insert(eqQ_[i]);
    }
    memset(eqCache_, 0xFF, sizeof(eqCache_));
    eqRateCache_ = -1;

    table_ = new Variable("table", SIP_TABLE, 0);
    Insert(table_);
    tableAuto_ = new Variable("table automation", SIP_TABLEAUTO, false);
    Insert(tableAuto_);

    memset(voices_, 0, sizeof(voices_));
    memset(widthDelay_, 0, sizeof(widthDelay_));
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
            voices_[i][v].partEnv_[0] = 1.0f;
            voices_[i][v].partEnv_[1] = 1.0f;
            voices_[i][v].partEnv_[2] = 1.0f;
            voices_[i][v].partEnv_[3] = 1.0f;
        }
        lastNote_[i] = -1;
        velocity_[i] = 127;
        bendSemis_[i] = 0.0f;
        liveDly_[i] = -1;
        liveRvb_[i] = -1;
        legatoNext_[i] = false;
        widthIdx_[i] = 0;
    }
}

PianoSynth::~PianoSynth() {
}

bool PianoSynth::Init() {
    tableState_.Reset();
    if (!gSineTableReady) {
        for (int i = 0; i < 256; i++) {
            gSineTable[i] = sinf(2.0f * 3.14159265f * (float)i / 256.0f);
        }
        gSineTableReady = true;
    }
    return true;
}

void PianoSynth::OnStart() {
    tableState_.Reset();
}

float PianoSynth::noteToFreq(unsigned char note) const {
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

void PianoSynth::resetVoice(int channel) {
    for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
        PianoVoice &pv = voices_[channel][v];
        pv.active_ = false;
        for (int p = 0; p < PIANO_VOICE_COUNT; p++) {
            pv.phase_[p] = 0.0f;
            pv.partEnv_[p] = 1.0f;
            pv.partDecayStep_[p] = 0.0f;
        }
        pv.freq_ = 0.0f;
        pv.baseFreq_ = 0.0f;
        pv.ampEnv_ = 0.0f;
        pv.ampStage_ = SES_ATTACK;
        pv.fEnv_ = 0.0f;
        pv.fStage_ = SES_ATTACK;
        pv.peak_ = 0.0f;
    }
    liveDly_[channel] = -1;
    liveRvb_[channel] = -1;
}

PianoVoice *PianoSynth::allocateVoice(int channel, unsigned char note) {

    PianoVoice *best = 0;
    float bestEnv = 1.0e30f;
    for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
        PianoVoice &pv = voices_[channel][v];
        if (!pv.active_) return &pv;
        if (pv.note_ == note && pv.ampStage_ < SES_RELEASE) return &pv;
        if (pv.ampEnv_ < bestEnv) {
            bestEnv = pv.ampEnv_;
            best = &pv;
        }
    }
    return best;  // steal the most decayed voice
}

bool PianoSynth::Start(int channel, unsigned char note, bool retrigger) {

    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return false;

    float rate = (float)Audio::GetInstance()->GetSampleRate();
    if (rate <= 0) rate = 44100.0f;

    PianoVoice *pv = allocateVoice(channel, note);
    if (!pv) return false;

    int partialCount = 2 + partials_->GetInt();
    if (partialCount < 2) partialCount = 2;
    if (partialCount > 4) partialCount = 4;
    int mode = mode_->GetInt();
    if (mode < 0) mode = 0;
    if (mode > 1) mode = 1;

    float bend = bendSemis_[channel];
    float baseFreq = noteToFreq(note);
    pv->baseFreq_ = baseFreq;
    pv->freq_ = baseFreq * powf(2.0f, bend / 12.0f);
    pv->note_ = note;

    // Partials: phases restart (re-strike), per-partial decay from pdecay
    // (0..100 -> fundamental ring 0.5..6 s); higher partials die faster.
    float ringT = 0.5f + ((float)pdecay_->GetInt() / 100.0f) * 5.5f;
    float timbre = (float)timbre_->GetInt() / 100.0f;
    for (int p = 0; p < 4; p++) {
        pv->phase_[p] = 0.0f;
        pv->partEnv_[p] = 1.0f;
        if (p < partialCount) {
            float t = ringT / (1.0f + 0.9f * (float)p);
            pv->partDecayStep_[p] = 1.0f / (0.001f + t * rate);
        } else {
            pv->partDecayStep_[p] = 0.0f;
        }
    }

    // Amp envelope (piano-like).
    float vel = (float)velocity_[channel] / 127.0f;
    if (vel < 0.02f) vel = 0.02f;
    float velFactor = powf(vel, 1.3f);
    float vol = (float)volume_->GetInt() / 100.0f;
    float accent = (float)accent_->GetInt() / 100.0f;
    pv->peak_ = vol * velFactor * (1.0f + accent * 0.5f);

    pv->ampEnv_ = 0.0f;
    pv->ampStage_ = SES_ATTACK;
    pv->ampStep_ = 1.0f / (0.001f + ((float)attack_->GetInt() * 0.02f) * rate);
    pv->ampSustain_ = (float)sustain_->GetInt() / 100.0f;
    pv->ampDecayStep_ = (1.0f - pv->ampSustain_) /
        (0.001f + ((float)decay_->GetInt() * 0.04f) * rate);
    pv->ampReleaseStep_ = 1.0f / (0.001f + ((float)release_->GetInt() * 0.04f) * rate);

    // Filter envelope.
    pv->fEnv_ = 0.0f;
    pv->fStage_ = SES_ATTACK;
    pv->fStep_ = 1.0f / (0.001f + ((float)fatk_->GetInt() * 0.02f) * rate);
    pv->fSustain_ = (float)fsus_->GetInt() / 100.0f;
    pv->fDecayStep_ = (1.0f - pv->fSustain_) /
        (0.001f + ((float)fdec_->GetInt() * 0.04f) * rate);
    pv->fReleaseStep_ = 1.0f / (0.001f + ((float)frel_->GetInt() * 0.04f) * rate);

    pv->active_ = true;
    lastNote_[channel] = note;
    return true;
}

void PianoSynth::Stop(int channel) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return;
    for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
        PianoVoice &pv = voices_[channel][v];
        if (!pv.active_) continue;
        pv.ampStage_ = SES_RELEASE;
        pv.fStage_ = SES_RELEASE;
    }
}

bool PianoSynth::Render(int channel, fixed *buffer, int size, bool updateTick) {

    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return false;

    bool anyActive = false;
    for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
        if (voices_[channel][v].active_) { anyActive = true; break; }
    }
    if (!anyActive) return false;

    float rate = (float)Audio::GetInstance()->GetSampleRate();
    if (rate <= 0) rate = 44100.0f;
    float invRate = 1.0f / rate;

    SYS_MEMSET(buffer, 0, size * 2 * sizeof(fixed));

    int partialCount = 2 + partials_->GetInt();
    if (partialCount < 2) partialCount = 2;
    if (partialCount > 4) partialCount = 4;
    int mode = mode_->GetInt();
    if (mode < 0) mode = 0;
    if (mode > 1) mode = 1;
    float timbre = (float)timbre_->GetInt() / 100.0f;
    float partGain[4];
    for (int p = 0; p < 4; p++) {
        float g = kPartialBaseGain[p];
        if (p > 0) g *= (0.5f + timbre);
        if (g > 1.0f) g = 1.0f;
        partGain[p] = g;
    }

    int ftype = ftype_->GetInt();
    if (ftype < 0) ftype = 0;
    if (ftype > 2) ftype = 2;

    float pan = ((float)pan_->GetInt() / 100.0f);
    float panL = cosf(pan * 1.57079633f);
    float panR = sinf(pan * 1.57079633f);

    float baseCut = (float)fcut_->GetInt() / 100.0f;
    float envAmount = (float)fenv_->GetInt() / 100.0f;
    float baseReso = (float)fres_->GetInt() / 100.0f;

    // Filter target at control rate: the strongest (most recent) filter
    // envelope among the active voices drives the shared channel filter.
    float fEnvMax = 0.0f;
    for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
        PianoVoice &pv = voices_[channel][v];
        if (pv.active_ && pv.fEnv_ > fEnvMax) fEnvMax = pv.fEnv_;
    }
    float cutoffTarget = baseCut + envAmount * fEnvMax;
    if (cutoffTarget > 1.0f) cutoffTarget = 1.0f;
    if (cutoffTarget < 0.0f) cutoffTarget = 0.0f;

    // Haas width delay (samples): 0..100 -> 0..~21 ms (0 = untouched).
    int widthSamples = (int)((float)width_->GetInt() / 100.0f *
                             (PIANO_WIDTH_DELAY_MAX - 1));
    float widthMix = ((float)width_->GetInt() / 100.0f) * 0.8f;
    float *ring = widthDelay_[channel];
    int &ringIdx = widthIdx_[channel];

    for (int i = 0; i < size; i++) {

        float sum = 0.0f;

        for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
            PianoVoice &pv = voices_[channel][v];
            if (!pv.active_) continue;

            // ---- amp envelope ----
            if (pv.ampStage_ == SES_ATTACK) {
                pv.ampEnv_ += pv.ampStep_;
                if (pv.ampEnv_ >= 1.0f) {
                    pv.ampEnv_ = 1.0f;
                    pv.ampStage_ = SES_DECAY;
                }
            } else if (pv.ampStage_ == SES_DECAY) {
                pv.ampEnv_ -= pv.ampDecayStep_;
                if (pv.ampEnv_ <= pv.ampSustain_) {
                    pv.ampEnv_ = pv.ampSustain_;
                    pv.ampStage_ = SES_SUSTAIN;
                }
            } else if (pv.ampStage_ == SES_SUSTAIN) {
                // sustain 0: keep decaying at half rate (natural piano tail)
                pv.ampEnv_ -= pv.ampDecayStep_ * 0.5f;
                if (pv.ampEnv_ <= 0.0005f) {
                    pv.ampEnv_ = 0.0f;
                    pv.ampStage_ = SES_DONE;
                    pv.active_ = false;
                }
            } else if (pv.ampStage_ == SES_RELEASE) {
                pv.ampEnv_ -= pv.ampReleaseStep_;
                if (pv.ampEnv_ <= 0.0f) {
                    pv.ampEnv_ = 0.0f;
                    pv.ampStage_ = SES_DONE;
                    pv.active_ = false;
                }
            }
            if (!pv.active_) continue;

            // ---- filter envelope ----
            if (pv.fStage_ == SES_ATTACK) {
                pv.fEnv_ += pv.fStep_;
                if (pv.fEnv_ >= 1.0f) {
                    pv.fEnv_ = 1.0f;
                    pv.fStage_ = SES_DECAY;
                }
            } else if (pv.fStage_ == SES_DECAY) {
                pv.fEnv_ -= pv.fDecayStep_;
                if (pv.fEnv_ <= pv.fSustain_) {
                    pv.fEnv_ = pv.fSustain_;
                    pv.fStage_ = SES_SUSTAIN;
                }
            } else if (pv.fStage_ == SES_RELEASE) {
                pv.fEnv_ -= pv.fReleaseStep_;
                if (pv.fEnv_ <= 0.0f) {
                    pv.fEnv_ = 0.0f;
                    pv.fStage_ = SES_DONE;
                }
            }

            // ---- partial oscillators (sine table, linear interpolation) ----
            float voiceOut = 0.0f;
            float baseInc = pv.freq_ * invRate;
            for (int p = 0; p < partialCount; p++) {
                pv.phase_[p] += baseInc * kPartialRatio[mode][p];
                if (pv.phase_[p] >= 1.0f) pv.phase_[p] -= 1.0f;
                float x = pv.phase_[p] * 256.0f;
                int i0 = (int)x;
                float frac = x - (float)i0;
                int i1 = (i0 + 1) & 255;
                i0 &= 255;
                float s = gSineTable[i0] +
                          (gSineTable[i1] - gSineTable[i0]) * frac;
                voiceOut += s * partGain[p] * pv.partEnv_[p];
                pv.partEnv_[p] -= pv.partDecayStep_[p];
                if (pv.partEnv_[p] < 0.0f) pv.partEnv_[p] = 0.0f;
            }

            sum += voiceOut * pv.ampEnv_ * pv.peak_;
        }

        float out = sum;
        ring[ringIdx] = out * panL;
        int delayIdx = ringIdx - widthSamples;
        if (delayIdx < 0) delayIdx += PIANO_WIDTH_DELAY_MAX;

        buffer[i * 2] += fl2fp(out * panL);
        buffer[i * 2 + 1] += fl2fp(out * panR +
                                  ring[delayIdx] * widthMix);

        ringIdx = (ringIdx + 1) % PIANO_WIDTH_DELAY_MAX;
    }

    // TPT SVF (per channel), control-rate target (smoothing anti-zipper).
    set_filter_v2(channel, (FilterV2Type)ftype, fl2fp(cutoffTarget),
                  fl2fp(baseReso), 255, false, false, (int)rate);

    filter_v2_t *fltv2 = get_filter_v2(channel);
    if (fltv2) {
        for (int i = 0; i < size; i++) {
            fixed l = buffer[i * 2];
            fixed r = buffer[i * 2 + 1];
            buffer[i * 2] = filterv2_process(fltv2, 0, l);
            buffer[i * 2 + 1] = filterv2_process(fltv2, 1, r);
        }
    }

    syncInstrumentEq();
    eqDsp_.Process(channel, buffer, size);

    return true;
}

void PianoSynth::syncInstrumentEq() {

    int rate = Audio::GetInstance()->GetSampleRate();
    if (rate != eqRateCache_) {
        eqRateCache_ = rate;
        eqDsp_.SetSampleRate(rate);
    }

    int vals[34];
    vals[0] = eqEnable_->GetInt();
    vals[1] = eqMask_->GetInt();
    for (int i = 0; i < 8; i++) {
        vals[2 + 4 * i] = eqFreq_[i]->GetInt();
        vals[3 + 4 * i] = eqGain_[i]->GetInt();
        vals[4 + 4 * i] = eqType_[i]->GetInt();
        vals[5 + 4 * i] = eqQ_[i]->GetInt();
    }
    if (memcmp(vals, eqCache_, sizeof(eqCache_)) == 0) return;
    memcpy(eqCache_, vals, sizeof(eqCache_));

    eqDsp_.SetBypass(vals[0] ? false : true);
    int mask = vals[1];
    for (int band = 0; band < 8; band++) {
        float hz = (float)vals[2 + 4 * band] / 100.0f;
        if (hz < 20.0f) hz = 20.0f;
        if (hz > 20000.0f) hz = 20000.0f;
        float db = (float)vals[3 + 4 * band];
        float q = (float)vals[5 + 4 * band] / 100.0f;
        if (q < 0.1f) q = 0.1f;
        if (q > 10.0f) q = 10.0f;
        int type = vals[4 + 4 * band];
        if (type < 0) type = 0;
        if (type >= (int)FxEngine::InstrumentEq::kTypeCount) type = 0;
        eqDsp_.SetBandFreq(band, fl2fp(hz));
        eqDsp_.SetBandGainDb(band, fl2fp(db));
        eqDsp_.SetBandType(band, (FxEngine::InstrumentEq::BandType)type);
        eqDsp_.SetBandQ(band, fl2fp(q));
        eqDsp_.SetBandEnabled(band, ((mask >> band) & 1) != 0);
    }
}

bool PianoSynth::IsInitialized() {
    return true;
}

bool PianoSynth::IsEmpty() {
    // Like MidiInstrument/BassSynth: piano slots always exist once saved.
    return false;
}

const char *PianoSynth::GetName() {
    const char *m = (mode_ && mode_->GetInt() >= 0 && mode_->GetInt() < 2)
        ? kPianoModeNames[mode_->GetInt()] : "EP";
    sprintf(name_, "PIANO %s", m);
    return name_;
}

int PianoSynth::GetTable() {
    int result = table_->GetInt();
    if (result > TABLE_COUNT) return VAR_OFF;
    return result;
}

bool PianoSynth::GetTableAutomation() {
    return tableAuto_->GetBool();
}

void PianoSynth::GetTableState(TableSaveState &state) {
    memcpy(state.hopCount_, tableState_.hopCount_, sizeof(uchar) * TABLE_STEPS * 3);
    memcpy(state.position_, tableState_.position_, sizeof(int) * 3);
}

void PianoSynth::SetTableState(TableSaveState &state) {
    memcpy(tableState_.hopCount_, state.hopCount_, sizeof(uchar) * TABLE_STEPS * 3);
    memcpy(tableState_.position_, state.position_, sizeof(int) * 3);
}

int PianoSynth::GetFxDelaySendOverride() {
    int v = dlySend_->GetInt();
    return (v < 0) ? 0xFF : v;
}

int PianoSynth::GetFxReverbSendOverride() {
    int v = rvbSend_->GetInt();
    return (v < 0) ? 0xFF : v;
}

int PianoSynth::GetFxDry() {
    int v = dry_->GetInt();
    return (v < 0) ? 0 : v;
}

int PianoSynth::GetLiveDelaySend(int channel) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return 0xFF;
    int v = liveDly_[channel];
    return (v < 0) ? 0xFF : v;
}

int PianoSynth::GetLiveReverbSend(int channel) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return 0xFF;
    int v = liveRvb_[channel];
    return (v < 0) ? 0xFF : v;
}

void PianoSynth::ProcessCommand(int channel, FourCC cc, ushort value) {

    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return;

    switch (cc) {

        case I_CMD_VOLM: {
            int v = (int)value * 100 / 255;
            volume_->SetInt(v);
            break;
        }

        case I_CMD_PAN_: {
            int v = (int)value * 100 / 255;
            pan_->SetInt(v);
            break;
        }

        case I_CMD_FCUT: {
            int v = (int)value * 100 / 255;
            fcut_->SetInt(v);
            break;
        }

        case I_CMD_FRES: {
            int v = (int)value * 100 / 255;
            fres_->SetInt(v);
            break;
        }

        case I_CMD_PTCH: {
            // Center 0x80 = no bend; retune all active voices instantly
            // (piano: no glide).
            int bend = (int)value - 0x80;
            float semis = ((float)bend / 0x80) * 12.0f;
            bendSemis_[channel] = semis;
            for (int v = 0; v < PIANO_VOICE_COUNT; v++) {
                PianoVoice &pv = voices_[channel][v];
                if (!pv.active_) continue;
                pv.freq_ = pv.baseFreq_ * powf(2.0f, semis / 12.0f);
            }
            break;
        }

        case I_CMD_LEGA: {
            legatoNext_[channel] = (value != 0);
            break;
        }

        case I_CMD_DLYS: {
            liveDly_[channel] = (int)value;
            break;
        }

        case I_CMD_RVBS: {
            liveRvb_[channel] = (int)value;
            break;
        }

        case I_CMD_MVEL: {
            // 0..255 -> 0..127 (same mapping as MidiInstrument).
            velocity_[channel] = (int)value / 2;
            if (velocity_[channel] > 127) velocity_[channel] = 127;
            break;
        }

        default:
            break;
    }
}

void PianoSynth::Purge() {
    IteratorPtr<Variable> it(GetIterator());
    for (it->Begin(); !it->IsDone(); it->Next()) {
        Variable &v = it->CurrentItem();
        v.Reset();
    }
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        resetVoice(i);
        lastNote_[i] = -1;
        velocity_[i] = 127;
        bendSemis_[i] = 0.0f;
        legatoNext_[i] = false;
        widthIdx_[i] = 0;
    }
    memset(widthDelay_, 0, sizeof(widthDelay_));
}