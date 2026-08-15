#include "BassSynth.h"

#include "Foundation/Variables/Variable.h"
#include "Application/Instruments/FilterV2.h"
#include "Application/Instruments/CommandList.h"
#include "Services/Audio/Audio.h"
#include "System/System/System.h"

#include <math.h>
#include <string.h>

// BASS_SYNTH (bacon-1.5, item 6): see BassSynth.h.
//
// DSP chain per sample:
//   osc (PolyBLEP) + sub (-1 oct square) + LFSR noise
//   -> drive (soft clip) -> TPT SVF (FilterV2, per channel)
//   -> amp envelope -> equal-power pan -> EQ8 (shared variable contract).

static const char *kWaveNames[4] = {
    "SAW", "SQUARE", "TRI", "SINE"
};

static const char *kFTypeNames[3] = {
    "LP", "HP", "BP"
};

static const char *kLTargetNames[3] = {
    "CUT", "VOL", "PIT"
};

BassSynth::BassSynth() {

    // ----------------------------------------------------------
    // Exported variables (persisted by name, 0..100 % UI range)
    // ----------------------------------------------------------
    wave_ = new Variable("wave", SBP_WAVE, kWaveNames, 4, 0);
    Insert(wave_);
    sub_ = new Variable("sub", SBP_SUB, 0);
    Insert(sub_);
    noise_ = new Variable("noise", SBP_NOISE, 0);
    Insert(noise_);
    glide_ = new Variable("glide", SBP_GLIDE, 0);
    Insert(glide_);
    volume_ = new Variable("volume", SBP_VOLUME, 100);
    Insert(volume_);
    pan_ = new Variable("pan", SBP_PAN, 50);
    Insert(pan_);

    attack_ = new Variable("attack", SBP_ATTACK, 10);
    Insert(attack_);
    decay_ = new Variable("decay", SBP_DECAY, 20);
    Insert(decay_);
    sustain_ = new Variable("sustain", SBP_SUSTAIN, 60);
    Insert(sustain_);
    release_ = new Variable("release", SBP_RELEASE, 30);
    Insert(release_);

    ftype_ = new Variable("ftype", SBP_FTYPE, kFTypeNames, 3, 0);
    Insert(ftype_);
    fcut_ = new Variable("fcut", SBP_FCUT, 80);
    Insert(fcut_);
    fres_ = new Variable("fres", SBP_FRES, 20);
    Insert(fres_);
    fenv_ = new Variable("fenv", SBP_FENV, 0);
    Insert(fenv_);
    fatk_ = new Variable("f atk", SBP_FATK, 10);
    Insert(fatk_);
    fdec_ = new Variable("f dec", SBP_FDEC, 20);
    Insert(fdec_);
    fsus_ = new Variable("f sus", SBP_FSUS, 60);
    Insert(fsus_);
    frel_ = new Variable("f rel", SBP_FREL, 30);
    Insert(frel_);

    drive_ = new Variable("drive", SBP_DRIVE, 0);
    Insert(drive_);
    accent_ = new Variable("accent", SBP_ACCENT, 0);
    Insert(accent_);
    lrate_ = new Variable("lrate", SBP_LRATE, 0);
    Insert(lrate_);
    ldepth_ = new Variable("ldepth", SBP_LDEPTH, 0);
    Insert(ldepth_);
    ltarget_ = new Variable("ltarget", SBP_LTARGET, kLTargetNames, 3, 0);
    Insert(ltarget_);

    // TREEFROG_INSTRUMENT_SENDS_V1: same names/ranges as SampleInstrument
    // so the sends UI and PlayerChannel behave identically.
    dry_ = new Variable("dry", SBP_DRY, 100);
    Insert(dry_);
    dlySend_ = new Variable("dly send", SBP_DLYSEND, -1);
    Insert(dlySend_);
    rvbSend_ = new Variable("rvb send", SBP_RVBSEND, -1);
    Insert(rvbSend_);

    // TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: same variable contract as
    // SampleInstrument (names + SIP_EQ* FourCCs) so InstrumentEqModal
    // works unchanged through FindVariable().
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
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        voices_[i].noiseState_ = 0x12345678u ^ (unsigned int)(i + 1);
        voices_[i].dlySend_ = -1;
        voices_[i].rvbSend_ = -1;
        lastNote_[i] = -1;
        legatoNext_[i] = false;
    }
}

BassSynth::~BassSynth() {
}

bool BassSynth::Init() {
    tableState_.Reset();
    return true;
}

void BassSynth::OnStart() {
    tableState_.Reset();
}

float BassSynth::noteToFreq(unsigned char note) const {
    // Standard 12-TET, A4 = 440 Hz (no root note: synths are MIDI-tuned).
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

void BassSynth::resetVoice(int channel) {
    SynthVoice &v = voices_[channel];
    v.active_ = false;
    v.oscPhase_ = 0.0f;
    v.subPhase_ = 0.0f;
    v.freq_ = 0.0f;
    v.targetFreq_ = 0.0f;
    v.glideTime_ = 0.0f;
    v.ampEnv_ = 0.0f;
    v.ampStage_ = SES_ATTACK;
    v.fEnv_ = 0.0f;
    v.fStage_ = SES_ATTACK;
    v.cutoff_ = 0.0f;
    v.reso_ = 0.0f;
    v.peak_ = 0.0f;
    v.dlySend_ = GetFxDelaySendOverride();
    v.rvbSend_ = GetFxReverbSendOverride();
}

bool BassSynth::Start(int channel, unsigned char note, bool retrigger) {

    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return false;

    SynthVoice &v = voices_[channel];

    float glidePct = (float)glide_->GetInt();
    float glideSec = glidePct * 0.02f; // 0..100 -> 0..2 s
    float newFreq = noteToFreq(note);

    bool activeBefore = v.active_;
    bool legato = (activeBefore && !retrigger);

    // Legato (same channel, no retrigger) keeps the envelopes running and
    // glides the pitch.  A retrigger re-attacks the envelopes (mono).
    if (legato) {
        v.targetFreq_ = newFreq;
        v.glideTime_ = glideSec;
        v.note_ = note;
        lastNote_[channel] = note;
        return true;
    }

    float rate = (float)Audio::GetInstance()->GetSampleRate();
    if (rate <= 0) rate = 44100.0f;

    // Restore live sends from the persisted base (Fase 15 contract).
    v.dlySend_ = GetFxDelaySendOverride();
    v.rvbSend_ = GetFxReverbSendOverride();

    if (!activeBefore) {
        v.oscPhase_ = 0.0f;
        v.subPhase_ = 0.0f;
    }

    if (activeBefore && glideSec > 0.0f) {
        // Mono glide between notes.
        v.targetFreq_ = newFreq;
        v.glideTime_ = glideSec;
    } else {
        v.freq_ = newFreq;
        v.targetFreq_ = newFreq;
        v.glideTime_ = 0.0f;
    }

    // Envelopes: attack to peak, decay to sustain.
    float vol = (float)volume_->GetInt() / 100.0f;
    float accent = (float)accent_->GetInt() / 100.0f;
    v.peak_ = vol * (1.0f + accent * 0.5f);

    v.ampEnv_ = 0.0f;
    v.ampStage_ = SES_ATTACK;
    v.ampStep_ = 1.0f / (0.001f + ((float)attack_->GetInt() * 0.02f) * rate);
    v.ampSustain_ = (float)sustain_->GetInt() / 100.0f;
    v.ampDecayStep_ = (1.0f - v.ampSustain_) /
        (0.001f + ((float)decay_->GetInt() * 0.04f) * rate);
    v.ampReleaseStep_ = 1.0f / (0.001f + ((float)release_->GetInt() * 0.04f) * rate);

    v.fEnv_ = 0.0f;
    v.fStage_ = SES_ATTACK;
    v.fStep_ = 1.0f / (0.001f + ((float)fatk_->GetInt() * 0.02f) * rate);
    v.fSustain_ = (float)fsus_->GetInt() / 100.0f;
    v.fDecayStep_ = (1.0f - v.fSustain_) /
        (0.001f + ((float)fdec_->GetInt() * 0.04f) * rate);
    v.fReleaseStep_ = 1.0f / (0.001f + ((float)frel_->GetInt() * 0.04f) * rate);

    v.note_ = note;
    lastNote_[channel] = note;
    v.active_ = true;

    return true;
}

void BassSynth::Stop(int channel) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return;
    SynthVoice &v = voices_[channel];
    if (!v.active_) return;
    v.ampStage_ = SES_RELEASE;
    v.fStage_ = SES_RELEASE;
}

// PolyBLEP residual (Valimaki & Huovilainen).
static inline float polyblep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

bool BassSynth::Render(int channel, fixed *buffer, int size, bool updateTick) {

    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return false;
    SynthVoice &v = voices_[channel];
    if (!v.active_) return false;

    float rate = (float)Audio::GetInstance()->GetSampleRate();
    if (rate <= 0) rate = 44100.0f;
    float invRate = 1.0f / rate;

    SYS_MEMSET(buffer, 0, size * 2 * sizeof(fixed));

    int wave = wave_->GetInt();
    if (wave < 0) wave = 0;
    if (wave > 3) wave = 3;
    float subLvl = (float)sub_->GetInt() / 100.0f;
    float noiseLvl = (float)noise_->GetInt() / 100.0f;
    if (subLvl + noiseLvl > 1.0f) {
        float s = subLvl + noiseLvl;
        subLvl /= s;
        noiseLvl /= s;
    }
    float driveGain = 1.0f + ((float)drive_->GetInt() / 100.0f) * 4.0f;

    int ftype = ftype_->GetInt();
    if (ftype < 0) ftype = 0;
    if (ftype > 2) ftype = 2;

    // LFO: rate 0..100 -> 0..20 Hz.
    float lfoHz = ((float)lrate_->GetInt() / 100.0f) * 20.0f;
    v.lfoStep_ = 2.0f * 3.14159265f * lfoHz * invRate;
    float lfoDepth = (float)ldepth_->GetInt() / 100.0f;
    int ltarget = ltarget_->GetInt();
    if (ltarget < 0) ltarget = 0;
    if (ltarget > 2) ltarget = 2;

    float pan = ((float)pan_->GetInt() / 100.0f);
    float panL = cosf(pan * 1.57079633f);
    float panR = sinf(pan * 1.57079633f);

    float baseCut = (float)fcut_->GetInt() / 100.0f;
    float envAmount = (float)fenv_->GetInt() / 100.0f;
    float baseReso = (float)fres_->GetInt() / 100.0f;

    float *ampEnv = &v.ampEnv_;
    int *ampStage = &v.ampStage_;
    float *fEnv = &v.fEnv_;
    int *fStage = &v.fStage_;

    // Filter target for the whole buffer, computed at control rate with the
    // envelope and LFO sampled at the buffer boundary.
    float cutoffTarget = baseCut + envAmount * (*fEnv);
    if (ltarget == 0) {
        cutoffTarget += sinf(v.lfoPhase_) * lfoDepth * 0.5f;
    }
    if (cutoffTarget > 1.0f) cutoffTarget = 1.0f;
    if (cutoffTarget < 0.0f) cutoffTarget = 0.0f;

    for (int i = 0; i < size; i++) {

        // ---- glide ----
        if (v.glideTime_ > 0.0f) {
            float k = 1.0f - expf(-1.0f / (v.glideTime_ * rate));
            v.freq_ += (v.targetFreq_ - v.freq_) * k;
            if (fabsf(v.targetFreq_ - v.freq_) < 0.01f) {
                v.freq_ = v.targetFreq_;
                v.glideTime_ = 0.0f;
            }
        }

        // ---- LFO ----
        v.lfoPhase_ += v.lfoStep_;
        float lfo = sinf(v.lfoPhase_) * lfoDepth;

        // ---- pitch modulation ----
        float freq = v.freq_;
        if (ltarget == 2) {
            freq *= powf(2.0f, lfo * 2.0f / 12.0f);
        }

        // ---- oscillators ----
        float dt = freq * invRate;
        v.oscPhase_ += dt;
        if (v.oscPhase_ >= 1.0f) v.oscPhase_ -= 1.0f;

        float p = v.oscPhase_;
        float osc;
        switch (wave) {
            case 1: { // square
                float b = polyblep(p, dt) - polyblep(fmodf(p + 0.5f, 1.0f), dt);
                osc = ((p < 0.5f) ? 1.0f : -1.0f) + b;
                break;
            }
            case 2: { // triangle
                osc = 2.0f * fabsf(2.0f * p - 1.0f) - 1.0f;
                break;
            }
            case 3: { // sine
                osc = sinf(2.0f * 3.14159265f * p);
                break;
            }
            default: { // saw (with PolyBLEP)
                osc = 2.0f * p - 1.0f - polyblep(p, dt);
                break;
            }
        }

        // Sub: square one octave down.
        v.subPhase_ += dt * 0.5f;
        if (v.subPhase_ >= 1.0f) v.subPhase_ -= 1.0f;
        float sub = (v.subPhase_ < 0.5f) ? 1.0f : -1.0f;

        // Noise: xorshift LFSR.
        v.noiseState_ ^= v.noiseState_ << 13;
        v.noiseState_ ^= v.noiseState_ >> 17;
        v.noiseState_ ^= v.noiseState_ << 5;
        float noise = ((float)(v.noiseState_ & 0xFFFF) / 32767.5f) - 1.0f;

        float mix = osc * (1.0f - subLvl - noiseLvl)
                  + sub * subLvl
                  + noise * noiseLvl;

        // ---- drive (soft clip) ----
        mix *= driveGain;
        if (mix > 1.0f) mix = 1.0f;
        if (mix < -1.0f) mix = -1.0f;
        mix = mix - (mix * mix * mix) / 3.0f;

        // ---- envelopes (per sample) ----
        if (*ampStage == SES_ATTACK) {
            *ampEnv += v.ampStep_;
            if (*ampEnv >= 1.0f) {
                *ampEnv = 1.0f;
                *ampStage = SES_DECAY;
            }
        } else if (*ampStage == SES_DECAY) {
            *ampEnv -= v.ampDecayStep_;
            if (*ampEnv <= v.ampSustain_) {
                *ampEnv = v.ampSustain_;
                *ampStage = SES_SUSTAIN;
            }
        } else if (*ampStage == SES_RELEASE) {
            *ampEnv -= v.ampReleaseStep_;
            if (*ampEnv <= 0.0f) {
                *ampEnv = 0.0f;
                *ampStage = SES_DONE;
                v.active_ = false;
            }
        }

        if (*fStage == SES_ATTACK) {
            *fEnv += v.fStep_;
            if (*fEnv >= 1.0f) {
                *fEnv = 1.0f;
                *fStage = SES_DECAY;
            }
        } else if (*fStage == SES_DECAY) {
            *fEnv -= v.fDecayStep_;
            if (*fEnv <= v.fSustain_) {
                *fEnv = v.fSustain_;
                *fStage = SES_SUSTAIN;
            }
        } else if (*fStage == SES_RELEASE) {
            *fEnv -= v.fReleaseStep_;
            if (*fEnv <= 0.0f) {
                *fEnv = 0.0f;
                *fStage = SES_DONE;
            }
        }

        // ---- volume LFO ----
        float amp = *ampEnv;
        if (ltarget == 1) {
            amp *= (1.0f + lfo);
        }

        float out = mix * amp * v.peak_;

        buffer[i * 2] += fl2fp(out * panL);
        buffer[i * 2 + 1] += fl2fp(out * panR);
    }

    // TPT SVF (per channel).  mix=255 => fully wet.  Control-rate update:
    // the TPT one-pole smoothing interpolates the coefficients between
    // buffers, so a per-buffer target is click-free.
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

    // TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: EQ8 on the finished stereo pair
    // (post-pan, pre-FX-send), same contract as SampleInstrument.
    syncInstrumentEq();
    eqDsp_.Process(channel, buffer, size);

    return true;
}

void BassSynth::syncInstrumentEq() {

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

bool BassSynth::IsInitialized() {
    return true;
}

bool BassSynth::IsEmpty() {
    // Like MidiInstrument: a synth slot always exists once the project is
    // saved; the XML loader skips missing instruments by ID.
    return false;
}

const char *BassSynth::GetName() {
    const char *w = (wave_ && wave_->GetInt() >= 0 && wave_->GetInt() < 4)
        ? kWaveNames[wave_->GetInt()] : "SAW";
    sprintf(name_, "SYNTH %s", w);
    return name_;
}

int BassSynth::GetTable() {
    int result = table_->GetInt();
    if (result > TABLE_COUNT) return VAR_OFF;
    return result;
}

bool BassSynth::GetTableAutomation() {
    return tableAuto_->GetBool();
}

void BassSynth::GetTableState(TableSaveState &state) {
    memcpy(state.hopCount_, tableState_.hopCount_, sizeof(uchar) * TABLE_STEPS * 3);
    memcpy(state.position_, tableState_.position_, sizeof(int) * 3);
}

void BassSynth::SetTableState(TableSaveState &state) {
    memcpy(tableState_.hopCount_, state.hopCount_, sizeof(uchar) * TABLE_STEPS * 3);
    memcpy(tableState_.position_, state.position_, sizeof(int) * 3);
}

int BassSynth::GetFxDelaySendOverride() {
    int v = dlySend_->GetInt();
    return (v < 0) ? 0xFF : v;
}

int BassSynth::GetFxReverbSendOverride() {
    int v = rvbSend_->GetInt();
    return (v < 0) ? 0xFF : v;
}

int BassSynth::GetFxDry() {
    int v = dry_->GetInt();
    return (v < 0) ? 0 : v;
}

int BassSynth::GetLiveDelaySend(int channel) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return 0xFF;
    int v = voices_[channel].dlySend_;
    return (v < 0) ? 0xFF : v;
}

int BassSynth::GetLiveReverbSend(int channel) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return 0xFF;
    int v = voices_[channel].rvbSend_;
    return (v < 0) ? 0xFF : v;
}

void BassSynth::ProcessCommand(int channel, FourCC cc, ushort value) {

    if (channel < 0 || channel >= SONG_CHANNEL_COUNT) return;

    switch (cc) {

        case I_CMD_VOLM: {
            // 0..255 -> 0..100 live volume (kept per channel).
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
            // Center 0x80 = no bend; other values transpose the running
            // voice in semitones via the glide target.
            SynthVoice &v = voices_[channel];
            if (!v.active_) break;
            int bend = (int)value - 0x80;
            float semis = ((float)bend / 0x80) * 12.0f;
            float base = noteToFreq(lastNote_[channel]);
            v.targetFreq_ = base * powf(2.0f, semis / 12.0f);
            v.glideTime_ = 0.05f;
            break;
        }

        case I_CMD_LEGA: {
            legatoNext_[channel] = (value != 0);
            break;
        }

        case I_CMD_DLYS: {
            voices_[channel].dlySend_ = (int)value;
            break;
        }

        case I_CMD_RVBS: {
            voices_[channel].rvbSend_ = (int)value;
            break;
        }

        default:
            break;
    }
}

void BassSynth::Purge() {
    IteratorPtr<Variable> it(GetIterator());
    for (it->Begin(); !it->IsDone(); it->Next()) {
        Variable &v = it->CurrentItem();
        v.Reset();
    }
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        resetVoice(i);
        lastNote_[i] = -1;
        legatoNext_[i] = false;
    }
}