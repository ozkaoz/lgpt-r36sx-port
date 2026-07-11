#include "SampleManagerDialog.h"
// TREEFROG_U2_34_SAMPLE_MANAGER_PURGE
// TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Model/Song.h"
#include "Application/Player/Player.h"
#include "Services/Time/TimeService.h"
#include "System/Console/Trace.h"
#include "System/FileSystem/FileSystem.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>

extern bool LGPTChopperHasSavedChopsForSampleIndex(int sampleIndex);
extern void LGPTChopperOnSamplePoolDelete(int deletedIndex);

#define SAMPLE_MANAGER_LIST_SIZE 12
#define SAMPLE_MANAGER_WIDTH 36

SampleManagerDialog::SampleManagerDialog(View &view)
    : ModalView(view), selected_(0), topIndex_(0), forceConfirmIndex_(-1) {
    status_[0] = 0;
}

SampleManagerDialog::~SampleManagerDialog() {
    Player::GetInstance()->StopStreaming();
}

void SampleManagerDialog::setStatus(const char *fmt, ...) {
    if (!fmt) {
        status_[0] = 0;
    } else {
        va_list args;
        va_start(args, fmt);
        vsnprintf(status_, sizeof(status_), fmt, args);
        va_end(args);
        status_[sizeof(status_) - 1] = 0;
    }
    isDirty_ = true;
}

void SampleManagerDialog::clampSelection() {
    SamplePool *pool = SamplePool::GetInstance();
    int count = pool ? pool->GetNameListSize() : 0;
    if (count <= 0) {
        selected_ = 0;
        topIndex_ = 0;
        return;
    }
    if (selected_ < 0) selected_ = 0;
    if (selected_ >= count) selected_ = count - 1;
    if (topIndex_ < 0) topIndex_ = 0;
    if (selected_ < topIndex_) topIndex_ = selected_;
    if (selected_ >= topIndex_ + SAMPLE_MANAGER_LIST_SIZE) {
        topIndex_ = selected_ - SAMPLE_MANAGER_LIST_SIZE + 1;
    }
    if (topIndex_ < 0) topIndex_ = 0;
}

int SampleManagerDialog::getUseCount(int sampleIndex) {
    // TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
    // Count real sequence usage, not mere assignment to an instrument slot.
    // A newly imported sample assigned to an unused instrument is shown as --
    // and is eligible for normal purge/delete, matching project-memory cleanup.
    if (!viewData_ || !viewData_->project_ || !viewData_->project_->song_ || sampleIndex < 0) return 0;
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    Song *song = viewData_->project_->song_;
    if (!bank || !song || !song->phrase_) return 0;

    bool counted[MAX_SAMPLEINSTRUMENT_COUNT];
    for (int i = 0; i < MAX_SAMPLEINSTRUMENT_COUNT; i++) counted[i] = false;
    int count = 0;

    for (int instrIndex = 0; instrIndex < MAX_SAMPLEINSTRUMENT_COUNT; instrIndex++) {
        I_Instrument *instr = bank->GetInstrument(instrIndex);
        if (!instr || instr->GetType() != IT_SAMPLE) continue;
        Variable *v = instr->FindVariable(SIP_SAMPLE);
        if (!v || v->GetInt() != sampleIndex) continue;

        for (int p = 0; p < PHRASE_COUNT && !counted[instrIndex]; p++) {
            if (!song->phrase_->IsUsed((uchar)p)) continue;
            for (int r = 0; r < 16; r++) {
                int offset = 16 * p + r;
                if (song->phrase_->note_[offset] != 0xFF &&
                    song->phrase_->instr_[offset] == instrIndex) {
                    counted[instrIndex] = true;
                    count++;
                    break;
                }
            }
        }
    }
    return count;
}

bool SampleManagerDialog::hasChops(int sampleIndex) {
    return LGPTChopperHasSavedChopsForSampleIndex(sampleIndex);
}

bool SampleManagerDialog::canDeleteSample(int sampleIndex, char *reason, int reasonLen) {
    if (reason && reasonLen > 0) reason[0] = 0;
    SamplePool *pool = SamplePool::GetInstance();
    if (!pool || sampleIndex < 0 || sampleIndex >= pool->GetNameListSize()) {
        if (reason && reasonLen > 0) snprintf(reason, reasonLen, "No sample");
        return false;
    }
    if (hasChops(sampleIndex)) {
        if (reason && reasonLen > 0) snprintf(reason, reasonLen, "Has chops");
        return false;
    }
    int uses = getUseCount(sampleIndex);
    if (uses > 0) {
        if (reason && reasonLen > 0) snprintf(reason, reasonLen, "Assigned x%d", uses);
        return false;
    }
    return true;
}

void SampleManagerDialog::deleteSidecarForName(const char *name) {
    if (!name || !name[0]) return;
    std::string logical = "samples:";
    logical += name;
    Path p(logical.c_str());
    std::string sidecar = p.GetPath();
    if (sidecar.empty()) return;
    sidecar += ".u2chop";
    FileSystem::GetInstance()->Delete(sidecar.c_str());
}

void SampleManagerDialog::notifyChopperDelete(int deletedIndex) {
    LGPTChopperOnSamplePoolDelete(deletedIndex);
}

int SampleManagerDialog::unassignSampleFromInstruments(int sampleIndex) {
    if (!viewData_ || !viewData_->project_ || sampleIndex < 0) return 0;
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (!bank) return 0;
    int cleared = 0;
    for (int i = 0; i < MAX_SAMPLEINSTRUMENT_COUNT; i++) {
        I_Instrument *instr = bank->GetInstrument(i);
        if (!instr || instr->GetType() != IT_SAMPLE) continue;
        SampleInstrument *sinstr = (SampleInstrument *)instr;
        if (sinstr->GetSampleIndex() == sampleIndex) {
            sinstr->AssignSample(-1);
            cleared++;
        }
    }
    return cleared;
}

void SampleManagerDialog::deleteSelectedSample() {
    SamplePool *pool = SamplePool::GetInstance();
    if (!pool) return;
    clampSelection();
    int count = pool->GetNameListSize();
    if (count <= 0 || selected_ < 0 || selected_ >= count) {
        setStatus("No samples");
        forceConfirmIndex_ = -1;
        return;
    }

    char reason[32];
    if (!canDeleteSample(selected_, reason, sizeof(reason))) {
        setStatus("Blocked: %s", reason);
        forceConfirmIndex_ = -1;
        return;
    }

    char **names = pool->GetNameList();
    char deletedName[40];
    snprintf(deletedName, sizeof(deletedName), "%s", names && names[selected_] ? names[selected_] : "sample");
    deletedName[sizeof(deletedName) - 1] = 0;

    // Free samples may still be assigned to unused instruments; clear those
    // stale assignments before SamplePool compacts indices.
    unassignSampleFromInstruments(selected_);
    deleteSidecarForName(deletedName);
    pool->PurgeSample(selected_);
    notifyChopperDelete(selected_);
    forceConfirmIndex_ = -1;
    clampSelection();
    setStatus("Deleted %.28s", deletedName);
}

void SampleManagerDialog::forceDeleteSelectedSample() {
    // TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
    // TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
    // Explicit two-step destructive path for samples with chops/assignments.
    // It clears instrument assignments first, avoiding SampleVariable's delete assert.
    SamplePool *pool = SamplePool::GetInstance();
    if (!pool) return;
    clampSelection();
    int count = pool->GetNameListSize();
    if (count <= 0 || selected_ < 0 || selected_ >= count) {
        setStatus("No samples");
        forceConfirmIndex_ = -1;
        return;
    }

    bool chops = hasChops(selected_);
    int uses = getUseCount(selected_);
    if (!chops && uses <= 0) {
        deleteSelectedSample();
        return;
    }

    if (forceConfirmIndex_ != selected_) {
        forceConfirmIndex_ = selected_;
        if (chops && uses > 0) setStatus("X again: del CH+I%d", uses);
        else if (chops) setStatus("X again: del CH");
        else setStatus("X again: del I%d", uses);
        return;
    }

    char **names = pool->GetNameList();
    char deletedName[40];
    snprintf(deletedName, sizeof(deletedName), "%s", names && names[selected_] ? names[selected_] : "sample");
    deletedName[sizeof(deletedName) - 1] = 0;

    int cleared = unassignSampleFromInstruments(selected_);
    deleteSidecarForName(deletedName);
    pool->PurgeSample(selected_);
    notifyChopperDelete(selected_);
    forceConfirmIndex_ = -1;
    clampSelection();
    setStatus("Force del %.18s I%d", deletedName, cleared);
}

void SampleManagerDialog::purgeUnusedSamples() {
    SamplePool *pool = SamplePool::GetInstance();
    if (!pool) return;
    int purged = 0;
    int protectedCount = 0;
    for (int i = pool->GetNameListSize() - 1; i >= 0; i--) {
        char reason[32];
        if (canDeleteSample(i, reason, sizeof(reason))) {
            char **names = pool->GetNameList();
            const char *name = names && names[i] ? names[i] : 0;
            unassignSampleFromInstruments(i);
            deleteSidecarForName(name);
            pool->PurgeSample(i);
            notifyChopperDelete(i);
            purged++;
        } else if (strcmp(reason, "Has chops") == 0) {
            protectedCount++;
        }
    }
    clampSelection();
    if (protectedCount > 0) setStatus("Purged %d, kept %d chops", purged, protectedCount);
    else setStatus("Purged %d unused", purged);
}

void SampleManagerDialog::DrawView() {
    SetWindow(SAMPLE_MANAGER_WIDTH, SAMPLE_MANAGER_LIST_SIZE + 5);
    GUITextProperties props;
    SamplePool *pool = SamplePool::GetInstance();
    int count = pool ? pool->GetNameListSize() : 0;
    char **names = pool ? pool->GetNameList() : 0;
    clampSelection();

    SetColor(CD_NORMAL);
    props.invert_ = false;
    DrawString(1, 0, "PROJECT SAMPLE MANAGER", props);

    if (count <= 0) {
        DrawString(1, 2, "No loaded project samples", props);
    } else {
        int y = 2;
        for (int i = topIndex_; i < count && i < topIndex_ + SAMPLE_MANAGER_LIST_SIZE; i++) {
            int uses = getUseCount(i);
            bool chops = hasChops(i);
            char line[64];
            const char *name = names && names[i] ? names[i] : "?";
            char tag[8];
            if (chops && uses > 0) snprintf(tag, sizeof(tag), "C%d", uses);
            else if (chops) snprintf(tag, sizeof(tag), "CH");
            else if (uses > 0) snprintf(tag, sizeof(tag), "I%d", uses);
            else snprintf(tag, sizeof(tag), "--");
            snprintf(line, sizeof(line), "%02X %-25.25s %s", i, name, tag);
            SetColor((i == selected_) ? CD_HILITE2 : CD_NORMAL);
            props.invert_ = false;
            DrawString(1, y, line, props);
            y++;
        }
    }

    SetColor(CD_NORMAL);
    props.invert_ = false;
    DrawString(1, SAMPLE_MANAGER_LIST_SIZE + 2, "A del free  X force  Y purge", props);
    DrawString(1, SAMPLE_MANAGER_LIST_SIZE + 3, "CH/Cn protected by purge  B exit", props);
    if (status_[0]) {
        SetColor(CD_HILITE1);
        DrawString(1, SAMPLE_MANAGER_LIST_SIZE + 4, status_, props);
    }
}

void SampleManagerDialog::OnPlayerUpdate(PlayerEventType, unsigned int currentTick) {
    (void)currentTick;
}

void SampleManagerDialog::OnFocus() {
    // AU11U_SAMPLE_MANAGER_NO_LATE_TRANSPORT_STOP
    // Caller owns transport stop.  Avoid late Stop() during modal focus.
    Player *player = Player::GetInstance();
    if (player && player->IsStreaming()) { player->StopStreaming(); Trace::Log("AU11M", "SAMPLE_MANAGER_STOP_STREAM_ONLY"); }
    TimeService::GetInstance()->Sleep(40);
    clampSelection();
    isDirty_ = true;
}

void SampleManagerDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;

    if ((mask & EPBM_L2) && (mask & EPBM_B)) {
        Player::GetInstance()->StopStreaming();
        setStatus("Preview stopped");
        return;
    }

    if (mask == EPBM_B || mask == EPBM_SELECT) {
        EndModal(0);
        return;
    }

    if (mask == EPBM_UP) {
        selected_--;
        forceConfirmIndex_ = -1;
        clampSelection();
        isDirty_ = true;
        return;
    }
    if (mask == EPBM_DOWN) {
        selected_++;
        forceConfirmIndex_ = -1;
        clampSelection();
        isDirty_ = true;
        return;
    }
    if (mask == (EPBM_A | EPBM_UP)) {
        selected_ -= SAMPLE_MANAGER_LIST_SIZE;
        forceConfirmIndex_ = -1;
        clampSelection();
        isDirty_ = true;
        return;
    }
    if (mask == (EPBM_A | EPBM_DOWN)) {
        selected_ += SAMPLE_MANAGER_LIST_SIZE;
        forceConfirmIndex_ = -1;
        clampSelection();
        isDirty_ = true;
        return;
    }

    if (mask == EPBM_A) {
        deleteSelectedSample();
        return;
    }
    if (mask == EPBM_X) {
        forceDeleteSelectedSample();
        return;
    }
    if (mask == EPBM_Y) {
        forceConfirmIndex_ = -1;
        purgeUnusedSamples();
        return;
    }
}
