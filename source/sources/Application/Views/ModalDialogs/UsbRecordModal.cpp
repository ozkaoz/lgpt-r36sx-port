#include "UsbRecordModal.h"

#include "Adapters/TREEFROG/GUI/TreeFrogEventManager.h"
#include "Adapters/TREEFROG/Main/TreeFrogSamplerInput.h"
#include "Application/Instruments/I_Instrument.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/WavFile.h"
#include "Application/Player/Player.h"
#include "Services/Time/TimeService.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const int kDurations[] = { 10, 30, 60, 120 };
static const int kDurationCount = sizeof(kDurations) / sizeof(kDurations[0]);
static const char kFileCharactersUpper[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
static const char kFileCharactersLower[] =
    "abcdefghijklmnopqrstuvwxyz0123456789_-";
static const int kFileCharacterCount = sizeof(kFileCharactersUpper) - 1;
static const int kMaxFileStemLength = 24;
static const char *kRecordDirectory =
    "/mnt/sdcard/lgpt/samples/records";
static const char *kRecordTempDirectory =
    "/tmp/r36sx_lgpt_record";
static const unsigned short kExitChord = EPBM_R | EPBM_LEFT;

static unsigned long long recordNowMs() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((unsigned long long)ts.tv_sec * 1000ULL) +
           ((unsigned long long)ts.tv_nsec / 1000000ULL);
}

static uint16_t readLe16(const unsigned char *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t readLe32(const unsigned char *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

extern "C" const char *TreeFrogU2520RecordBuildMarker(void) {
    return "U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY";
}

extern "C" const char *TreeFrogH25AndroidRecordBuildMarker(void) {
    return "H25_RECORD_WAV_44100_48000_PREVIEW_SAVE_READY";
}

static bool isSupportedUsbRecordRate(uint32_t rate) {
    return rate == 44100U || rate == 48000U;
}

UsbRecordModal::UsbRecordModal(View &view, int instrumentIndex)
    : ModalView(view),
      instrumentIndex_(instrumentIndex),
      selected_(ITEM_RECORD),
      durationIndex_(1),
      sessionState_(SESSION_BOOT),
      stateSinceMs_(recordNowMs()),
      monitorRequested_(false),
      monitorBeforeRecord_(false),
      previewing_(false),
      editingFile_(false),
      fileLowercaseMode_(false),
      fileEditorInputArmed_(false),
      inputArmed_(false),
      closePending_(false),
      closeSaved_(false),
      activeInputMask_(0),
      fileEditorPhysicalMask_(0),
      fileEditorNeutralFrames_(0),
      neutralInputFrames_(0),
      frameCounter_(0),
      capturePollDivider_(0),
      drawRefreshDivider_(0),
      takeCounter_(0),
      fileCursor_(0),
      validatedDataBytes_(0),
      validatedFrames_(0) {
    memset(&capture_, 0, sizeof(capture_));
    fileStem_[0] = 0;
    fileEditBackup_[0] = 0;
    plannedPath_[0] = 0;
    plannedName_[0] = 0;
    currentTakePath_[0] = 0;
    validatedPath_[0] = 0;
    usbState_[0] = 0;
    status_[0] = 0;

    Player *player = Player::GetInstance();
    if (player) {
        player->Stop();
        player->StopStreaming();
    }

    TreeFrogEventManager::GetInstance()->ClearQueue();
    TreeFrogUac2Bridge_SetUsbMonitor(0);
    TreeFrogUac2Bridge_Prime();

    ensureRecordDirectory();
    makeNextCapturePath();
    updateCaptureSnapshot(true);

    if (capture_.state == TREEFROG_USB_CAPTURE_READY &&
        capture_.path[0]) {
        long bytes = 0;
        int frames = 0;
        char reason[96];
        if (validateCaptureWav(
                capture_.path,
                &bytes,
                &frames,
                reason,
                sizeof(reason))) {
            snprintf(currentTakePath_, sizeof(currentTakePath_), "%s", capture_.path);
            snprintf(validatedPath_, sizeof(validatedPath_), "%s", capture_.path);
            validatedDataBytes_ = bytes;
            validatedFrames_ = frames;

            /* Compatibility with pre-U2518 takes that were recorded directly
             * into records/.  A transactional temporary take has no final name
             * yet, so keep the next available planned filename in that case. */
            if (!currentTakeIsTemporary()) {
                const char *base = strrchr(currentTakePath_, '/');
                base = base ? base + 1 : currentTakePath_;
                size_t length = strlen(base);
                if (length > 4 && strcasecmp(base + length - 4, ".wav") == 0)
                    length -= 4;
                if (length > 0 && length < sizeof(fileStem_)) {
                    memcpy(fileStem_, base, length);
                    fileStem_[length] = 0;
                    updatePlannedPathFromStem();
                }
            }
            setSessionState(SESSION_READY, "Previous take ready: Preview/Save");
        } else {
            setSessionState(SESSION_BOOT, "Release controls; Record is preparing");
        }
    } else {
        setSessionState(SESSION_BOOT, "Release controls; Record is preparing");
    }
}

UsbRecordModal::~UsbRecordModal() {
    Player *player = Player::GetInstance();
    if (player) player->StopStreaming();

    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING ||
        sessionState_ == SESSION_FINALIZING) {
        TreeFrogUac2Bridge_StopUsbCapture();
    }

    TreeFrogUac2Bridge_SetUsbMonitor(0);
    TreeFrogEventManager::GetInstance()->ClearQueue();
}

void UsbRecordModal::setStatus(const char *text) {
    snprintf(status_, sizeof(status_), "%s", text ? text : "");
    isDirty_ = true;
}

void UsbRecordModal::setSessionState(
    SessionState state,
    const char *status) {
    sessionState_ = state;
    stateSinceMs_ = recordNowMs();
    if (status) setStatus(status);
    else isDirty_ = true;
}

const char *UsbRecordModal::sessionStateName() const {
    switch (sessionState_) {
        case SESSION_BOOT: return "BOOT";
        case SESSION_IDLE: return "IDLE";
        case SESSION_ARMING: return "STARTING";
        case SESSION_RECORDING: return "RECORDING";
        case SESSION_FINALIZING: return "FINALIZING";
        case SESSION_READY: return "READY";
        case SESSION_PREVIEWING: return "PREVIEW";
        case SESSION_ERROR: return "ERROR";
        case SESSION_CLOSING: return "CLOSING";
        default: return "UNKNOWN";
    }
}

int UsbRecordModal::countBits(unsigned short value) {
    int count = 0;
    while (value) {
        value &= (unsigned short)(value - 1);
        ++count;
    }
    return count;
}

bool UsbRecordModal::physicalInputNeutral() const {
    TreeFrogSamplerInputSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    TreeFrogSamplerInput_Read(&snapshot);
    return snapshot.selectedPhysical == 0;
}

void UsbRecordModal::updateInputArming() {
    if (physicalInputNeutral()) {
        if (neutralInputFrames_ < 8) ++neutralInputFrames_;
    } else {
        neutralInputFrames_ = 0;
    }

    if (!inputArmed_ && neutralInputFrames_ >= 2) {
        inputArmed_ = true;
        activeInputMask_ = 0;
        TreeFrogEventManager::GetInstance()->ClearQueue();
        if (sessionState_ == SESSION_BOOT)
            setSessionState(SESSION_IDLE, "Record ready; one press equals one action");
        else
            setStatus("Record controls ready");
    }
}

void UsbRecordModal::requestClose(bool saved) {
    closePending_ = true;
    closeSaved_ = saved;
    inputArmed_ = false;
    activeInputMask_ = 0;
    neutralInputFrames_ = 0;
    setSessionState(
        SESSION_CLOSING,
        saved ? "Saved; release controls to return" :
                "Release controls to return");
}

void UsbRecordModal::completeCloseWhenNeutral() {
    if (!closePending_) return;

    if (physicalInputNeutral()) {
        if (neutralInputFrames_ < 8) ++neutralInputFrames_;
    } else {
        neutralInputFrames_ = 0;
    }

    if (neutralInputFrames_ < 2) return;

    TreeFrogUac2Bridge_SetUsbMonitor(0);
    TreeFrogEventManager::GetInstance()->ClearQueue();
    closePending_ = false;
    EndModal(closeSaved_ ? 1 : 0);
}

void UsbRecordModal::updateCaptureSnapshot(bool force) {
    TreeFrogUac2Bridge_GetUsbCaptureSnapshot(
        &capture_,
        force ? 1 : 0);
    snprintf(
        usbState_,
        sizeof(usbState_),
        "%s",
        TreeFrogUac2Bridge_GetUsbStateText());
}

bool UsbRecordModal::snapshotPathMatchesCurrentTake() const {
    if (!currentTakePath_[0] || !capture_.path[0]) return false;
    return strcmp(currentTakePath_, capture_.path) == 0;
}

void UsbRecordModal::applyCaptureSnapshot() {
    const unsigned long long now = recordNowMs();

    if (sessionState_ == SESSION_ARMING) {
        if (capture_.state == TREEFROG_USB_CAPTURE_RECORDING &&
            snapshotPathMatchesCurrentTake()) {
            setSessionState(SESSION_RECORDING, "Recording; A stops");
        } else if (capture_.state == TREEFROG_USB_CAPTURE_ERROR &&
                   stateSinceMs_ > 0 && now - stateSinceMs_ > 300ULL) {
            /* Ignore an ERROR snapshot left by the previous session until the
             * daemon has had one command-poll interval to publish this take. */
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            setSessionState(
                SESSION_ERROR,
                capture_.error[0] ? capture_.error : "Capture start failed");
        } else if (stateSinceMs_ > 0 && now - stateSinceMs_ > 8000ULL) {
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            setSessionState(SESSION_ERROR, "Capture start timeout");
        }
    } else if (sessionState_ == SESSION_RECORDING) {
        if (capture_.state == TREEFROG_USB_CAPTURE_READY &&
            snapshotPathMatchesCurrentTake()) {
            long bytes = 0;
            int frames = 0;
            char reason[96];
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            if (validateCaptureWav(
                    currentTakePath_,
                    &bytes,
                    &frames,
                    reason,
                    sizeof(reason))) {
                snprintf(validatedPath_, sizeof(validatedPath_), "%s", currentTakePath_);
                validatedDataBytes_ = bytes;
                validatedFrames_ = frames;
                setSessionState(SESSION_READY, "Take ready: Preview, Save or Discard");
            } else {
                setSessionState(SESSION_ERROR, reason);
            }
        } else if (capture_.state == TREEFROG_USB_CAPTURE_ERROR) {
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            setSessionState(
                SESSION_ERROR,
                capture_.error[0] ? capture_.error : "Capture failed");
        }
    } else if (sessionState_ == SESSION_FINALIZING) {
        if (capture_.state == TREEFROG_USB_CAPTURE_READY &&
            snapshotPathMatchesCurrentTake()) {
            long bytes = 0;
            int frames = 0;
            char reason[96];
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            if (validateCaptureWav(
                    currentTakePath_,
                    &bytes,
                    &frames,
                    reason,
                    sizeof(reason))) {
                snprintf(validatedPath_, sizeof(validatedPath_), "%s", currentTakePath_);
                validatedDataBytes_ = bytes;
                validatedFrames_ = frames;
                setSessionState(SESSION_READY, "Take finalized: Preview, Save or Discard");
            } else {
                setSessionState(SESSION_ERROR, reason);
            }
        } else if (capture_.state == TREEFROG_USB_CAPTURE_ERROR) {
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            setSessionState(
                SESSION_ERROR,
                capture_.error[0] ? capture_.error : "Capture finalization failed");
        } else if (stateSinceMs_ > 0 && now - stateSinceMs_ > 7000ULL) {
            monitorRequested_ = monitorBeforeRecord_;
            TreeFrogUac2Bridge_SetUsbMonitor(monitorRequested_ ? 1 : 0);
            setSessionState(SESSION_ERROR, "Capture finalization timeout");
        }
    }

    if (previewing_) {
        Player *player = Player::GetInstance();
        if (!player || !player->IsStreaming()) {
            previewing_ = false;
            if (sessionState_ == SESSION_PREVIEWING)
                setSessionState(SESSION_READY, "Preview finished");
        }
    }

    if (closePending_ &&
        sessionState_ != SESSION_ARMING &&
        sessionState_ != SESSION_RECORDING &&
        sessionState_ != SESSION_FINALIZING) {
        completeCloseWhenNeutral();
    }
}

void UsbRecordModal::moveSelection(int delta) {
    selected_ += delta;
    while (selected_ < 0) selected_ += ITEM_COUNT;
    while (selected_ >= ITEM_COUNT) selected_ -= ITEM_COUNT;
    isDirty_ = true;
}

void UsbRecordModal::cycleDuration(int delta) {
    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING ||
        sessionState_ == SESSION_FINALIZING) {
        setStatus("Duration is locked while recording");
        return;
    }
    durationIndex_ += delta;
    while (durationIndex_ < 0) durationIndex_ += kDurationCount;
    while (durationIndex_ >= kDurationCount) durationIndex_ -= kDurationCount;
    setStatus("Capture duration changed");
}

void UsbRecordModal::toggleMonitor() {
    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING ||
        sessionState_ == SESSION_FINALIZING) {
        setStatus("Monitor is controlled by the active recording");
        return;
    }
    if (previewing_) stopPreview("Preview stopped");
    const bool requested = !monitorRequested_;
    if (!TreeFrogUac2Bridge_SetUsbMonitor(requested ? 1 : 0)) {
        setStatus("Input monitor unavailable; OTG runtime is recovering");
        return;
    }
    monitorRequested_ = requested;
    setStatus(monitorRequested_ ? "USB input monitor ON" :
                                  "USB input monitor OFF");
}

void UsbRecordModal::ensureRecordDirectory() {
    mkdir("/mnt/sdcard/lgpt", 0777);
    mkdir("/mnt/sdcard/lgpt/samples", 0777);
    mkdir(kRecordDirectory, 0777);
    mkdir("/tmp/r36sx_lgpt_record", 0777);
    mkdir(kRecordTempDirectory, 0777);
}

void UsbRecordModal::makeTemporaryCapturePath() {
    ensureRecordDirectory();
    ++takeCounter_;
    snprintf(
        currentTakePath_,
        sizeof(currentTakePath_),
        "%s/take_%ld_%llu_%u.wav",
        kRecordTempDirectory,
        (long)getpid(),
        recordNowMs(),
        takeCounter_);
    /* The name is designed to be unique.  Remove only this process-owned path
     * in the unlikely event of a repeated monotonic timestamp. */
    unlink(currentTakePath_);
}

bool UsbRecordModal::currentTakeIsTemporary() const {
    const size_t prefixLength = strlen(kRecordTempDirectory);
    return currentTakePath_[0] &&
           strncmp(currentTakePath_, kRecordTempDirectory, prefixLength) == 0 &&
           currentTakePath_[prefixLength] == '/';
}

bool UsbRecordModal::promoteCaptureToFinalPath(
    const char *sourcePath,
    const char *destinationPath,
    char *reason,
    int reasonLength) {
    if (reason && reasonLength > 0) reason[0] = 0;
    if (!sourcePath || !sourcePath[0] ||
        !destinationPath || !destinationPath[0]) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Capture path is invalid");
        return false;
    }

    /* A recovered pre-U2518 take can already be in its final location. */
    if (strcmp(sourcePath, destinationPath) == 0) return true;

    struct stat existing;
    if (stat(destinationPath, &existing) == 0) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Duplicate name blocked; edit File name");
        return false;
    }

    int source = open(sourcePath, O_RDONLY);
    if (source < 0) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Temporary take cannot be opened");
        return false;
    }

    /* H32 FAT32 safety: write and validate a same-directory staging file,
     * then publish it with one rename.  Runtime recording remains in tmpfs;
     * FAT32 is touched only when the user explicitly chooses Save. */
    char stagingPath[1024];
    snprintf(
        stagingPath,
        sizeof(stagingPath),
        "%s.h32part.%ld",
        destinationPath,
        (long)getpid());
    unlink(stagingPath);

    int destination = open(
        stagingPath,
        O_WRONLY | O_CREAT | O_EXCL,
        0666);
    if (destination < 0) {
        close(source);
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Final WAV staging file cannot be created");
        return false;
    }

    bool copied = true;
    char buffer[16384];
    for (;;) {
        ssize_t count = read(source, buffer, sizeof(buffer));
        if (count == 0) break;
        if (count < 0) {
            if (errno == EINTR) continue;
            copied = false;
            break;
        }
        ssize_t offset = 0;
        while (offset < count) {
            ssize_t written = write(
                destination,
                buffer + offset,
                (size_t)(count - offset));
            if (written > 0) {
                offset += written;
                continue;
            }
            if (written < 0 && errno == EINTR) continue;
            copied = false;
            break;
        }
        if (!copied) break;
    }

    if (copied && fsync(destination) != 0) copied = false;
    close(source);
    close(destination);

    if (!copied) {
        unlink(stagingPath);
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Final WAV staging copy failed");
        return false;
    }

    long bytes = 0;
    int frames = 0;
    char validation[96];
    if (!validateCaptureWav(
            stagingPath,
            &bytes,
            &frames,
            validation,
            sizeof(validation))) {
        unlink(stagingPath);
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "%s", validation);
        return false;
    }

    if (rename(stagingPath, destinationPath) != 0) {
        unlink(stagingPath);
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Final WAV atomic publish failed");
        return false;
    }

    /* Best-effort directory sync. Some FAT/DrvFs implementations return
     * EINVAL for directory fsync; the published WAV remains valid in that
     * case and the launcher performs a bounded sync on session exit. */
    int dirfd = open(kRecordDirectory, O_RDONLY);
    if (dirfd >= 0) {
        if (fsync(dirfd) != 0 && errno != EINVAL && errno != EROFS) {
            close(dirfd);
            if (reason && reasonLength > 0)
                snprintf(reason, reasonLength, "WAV saved; directory sync warning");
            unlink(sourcePath);
            return true;
        }
        close(dirfd);
    }

    /* A valid committed file must never be deleted because tmpfs cleanup
     * failed.  The cleanup script can safely remove a residual source take. */
    unlink(sourcePath);

    if (reason && reasonLength > 0)
        snprintf(reason, reasonLength, "saved");
    return true;
}

void UsbRecordModal::updatePlannedPathFromStem() {
    if (!fileStem_[0]) snprintf(fileStem_, sizeof(fileStem_), "USBREC_001");
    snprintf(plannedName_, sizeof(plannedName_), "%s.wav", fileStem_);
    snprintf(plannedPath_, sizeof(plannedPath_), "%s/%s", kRecordDirectory, plannedName_);
}

bool UsbRecordModal::recordNameExistsExact(const char *name) const {
    if (!name || !name[0]) return false;
    DIR *directory = opendir(kRecordDirectory);
    if (!directory) return false;

    bool exists = false;
    struct dirent *entry = 0;
    while ((entry = readdir(directory)) != 0) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == 0 ||
             (entry->d_name[1] == '.' && entry->d_name[2] == 0)))
            continue;
        if (strcmp(entry->d_name, name) == 0) {
            exists = true;
            break;
        }
    }
    closedir(directory);
    return exists;
}

int UsbRecordModal::fileEditorViewStart(int width) const {
    if (width <= 0) return 0;
    const int length = (int)strlen(fileStem_);
    if (length <= width || fileCursor_ < width) return 0;
    int start = fileCursor_ - width + 1;
    const int maximum = length > width ? length - width : 0;
    if (start > maximum) start = maximum;
    if (start < 0) start = 0;
    return start;
}

void UsbRecordModal::makeNextCapturePath() {
    ensureRecordDirectory();
    for (int index = 1; index <= 999; ++index) {
        snprintf(fileStem_, sizeof(fileStem_), "USBREC_%03d", index);
        updatePlannedPathFromStem();
        struct stat info;
        if (stat(plannedPath_, &info) != 0) return;
    }
    snprintf(fileStem_, sizeof(fileStem_), "USBREC_999");
    updatePlannedPathFromStem();
}

void UsbRecordModal::beginFileEdit() {
    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING ||
        sessionState_ == SESSION_FINALIZING ||
        sessionState_ == SESSION_CLOSING) {
        setStatus("File name is locked during capture/finalization");
        return;
    }
    if (previewing_) stopPreview("Preview stopped for rename");
    snprintf(fileEditBackup_, sizeof(fileEditBackup_), "%s", fileStem_);
    editingFile_ = true;
    resetFileEditorInputGuard();
    const int length = (int)strlen(fileStem_);
    fileCursor_ = length > 0 ? length - 1 : 0;
    fileLowercaseMode_ = false;
    for (int i = 0; fileStem_[i]; ++i) {
        if (fileStem_[i] >= 'a' && fileStem_[i] <= 'z') {
            fileLowercaseMode_ = true;
            break;
        }
    }
    setStatus("Rename: X+UP/DOWN fast, L1+X case, A confirms");
}

void UsbRecordModal::cancelFileEdit() {
    snprintf(fileStem_, sizeof(fileStem_), "%s", fileEditBackup_);
    updatePlannedPathFromStem();
    editingFile_ = false;
    rearmMenuInputAfterFileEditor();
    setStatus("File name unchanged");
}

void UsbRecordModal::moveFileCursor(int delta) {
    int length = (int)strlen(fileStem_);
    if (length <= 0) {
        snprintf(fileStem_, sizeof(fileStem_), "A");
        length = 1;
    }
    fileCursor_ += delta;
    if (fileCursor_ < 0) fileCursor_ = 0;
    if (fileCursor_ >= length) {
        if (delta > 0 && length < kMaxFileStemLength) {
            fileStem_[length] = fileLowercaseMode_ ? 'a' : 'A';
            fileStem_[length + 1] = 0;
            fileCursor_ = length;
        } else {
            fileCursor_ = length - 1;
        }
    }
    updatePlannedPathFromStem();
    isDirty_ = true;
}

void UsbRecordModal::cycleFileCharacter(int delta) {
    int length = (int)strlen(fileStem_);
    if (length <= 0) {
        snprintf(fileStem_, sizeof(fileStem_), "%c", fileLowercaseMode_ ? 'a' : 'A');
        length = 1;
        fileCursor_ = 0;
    }
    if (fileCursor_ < 0) fileCursor_ = 0;
    if (fileCursor_ >= length) fileCursor_ = length - 1;

    const char *characters = fileLowercaseMode_ ?
        kFileCharactersLower : kFileCharactersUpper;
    int index = 0;
    for (int i = 0; i < kFileCharacterCount; ++i) {
        if (characters[i] == fileStem_[fileCursor_]) {
            index = i;
            break;
        }
    }
    index += delta;
    while (index < 0) index += kFileCharacterCount;
    while (index >= kFileCharacterCount) index -= kFileCharacterCount;
    fileStem_[fileCursor_] = characters[index];
    updatePlannedPathFromStem();
    isDirty_ = true;
}

void UsbRecordModal::toggleFileCharacterCase() {
    int length = (int)strlen(fileStem_);
    if (length <= 0) {
        snprintf(fileStem_, sizeof(fileStem_), "a");
        fileCursor_ = 0;
        fileLowercaseMode_ = true;
    } else {
        if (fileCursor_ < 0) fileCursor_ = 0;
        if (fileCursor_ >= length) fileCursor_ = length - 1;
        unsigned char value = (unsigned char)fileStem_[fileCursor_];
        if (value >= 'A' && value <= 'Z') {
            fileStem_[fileCursor_] = (char)tolower(value);
            fileLowercaseMode_ = true;
        } else if (value >= 'a' && value <= 'z') {
            fileStem_[fileCursor_] = (char)toupper(value);
            fileLowercaseMode_ = false;
        } else {
            fileLowercaseMode_ = !fileLowercaseMode_;
        }
    }
    updatePlannedPathFromStem();
    setStatus(fileLowercaseMode_ ? "Lowercase mode" : "Uppercase mode");
    isDirty_ = true;
}

void UsbRecordModal::deleteFileCharacter() {
    int length = (int)strlen(fileStem_);
    if (length <= 1) {
        setStatus("File name cannot be empty");
        return;
    }
    if (fileCursor_ < 0) fileCursor_ = 0;
    if (fileCursor_ >= length) fileCursor_ = length - 1;
    memmove(
        fileStem_ + fileCursor_,
        fileStem_ + fileCursor_ + 1,
        (size_t)(length - fileCursor_));
    --length;
    if (fileCursor_ >= length) fileCursor_ = length - 1;
    updatePlannedPathFromStem();
    isDirty_ = true;
}

void UsbRecordModal::resetFileEditorInputGuard() {
    fileEditorInputArmed_ = false;
    fileEditorPhysicalMask_ = 0;
    fileEditorNeutralFrames_ = 0;
    activeInputMask_ = 0;
    TreeFrogEventManager::GetInstance()->ClearQueue();
    setStatus("Release controls; filename editor uses physical edges");
}

void UsbRecordModal::rearmMenuInputAfterFileEditor() {
    fileEditorInputArmed_ = false;
    fileEditorPhysicalMask_ = 0;
    fileEditorNeutralFrames_ = 0;
    inputArmed_ = false;
    activeInputMask_ = 0;
    neutralInputFrames_ = 0;
    TreeFrogEventManager::GetInstance()->ClearQueue();
}

void UsbRecordModal::processFileEditorPhysicalInput() {
    if (!editingFile_) return;

    TreeFrogSamplerInputSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    TreeFrogSamplerInput_Read(&snapshot);
    const unsigned int current = snapshot.selectedPhysical;

    if (!fileEditorInputArmed_) {
        fileEditorPhysicalMask_ = current;
        if (current == 0) {
            if (fileEditorNeutralFrames_ < 8) ++fileEditorNeutralFrames_;
        } else {
            fileEditorNeutralFrames_ = 0;
        }
        if (fileEditorNeutralFrames_ >= 2) {
            fileEditorInputArmed_ = true;
            fileEditorPhysicalMask_ = 0;
            setStatus("Filename editor ready; one physical edge per action");
        }
        return;
    }

    const unsigned int previous = fileEditorPhysicalMask_;
    const unsigned int newBits = current & ~previous;
    fileEditorPhysicalMask_ = current;
    if (newBits == 0) return;

    if ((current & TFSP_L1) != 0 && (newBits & TFSP_X) != 0) {
        toggleFileCharacterCase();
        return;
    }

    if ((current & TFSP_X) != 0) {
        const unsigned int fastDirection = newBits & (TFSP_UP | TFSP_DOWN);
        if (fastDirection == TFSP_UP) {
            cycleFileCharacter(5);
            return;
        }
        if (fastDirection == TFSP_DOWN) {
            cycleFileCharacter(-5);
            return;
        }
    }

    const unsigned int actionBits =
        newBits & (TFSP_UP | TFSP_DOWN | TFSP_LEFT | TFSP_RIGHT |
                   TFSP_A | TFSP_B | TFSP_X | TFSP_Y);
    if (actionBits == 0) return;

    unsigned int bits = actionBits;
    int count = 0;
    while (bits) {
        bits &= bits - 1;
        ++count;
    }
    if (count != 1) {
        setStatus("Multiple physical edges ignored; release and retry");
        return;
    }

    if (actionBits == TFSP_LEFT) moveFileCursor(-1);
    else if (actionBits == TFSP_RIGHT) moveFileCursor(1);
    else if (actionBits == TFSP_UP) cycleFileCharacter(1);
    else if (actionBits == TFSP_DOWN) cycleFileCharacter(-1);
    else if (actionBits == TFSP_X)
        setStatus("Hold X then press UP/DOWN for +/-5");
    else if (actionBits == TFSP_Y)
        setStatus("Y is unused in File name");
    else if (actionBits == TFSP_A) confirmFileEdit();
    else if (actionBits == TFSP_B) deleteFileCharacter();
}

void UsbRecordModal::confirmFileEdit() {
    if (!fileStem_[0]) {
        setStatus("File name cannot be empty");
        return;
    }
    updatePlannedPathFromStem();
    if (recordNameExistsExact(plannedName_) &&
        (!currentTakePath_[0] || strcmp(plannedPath_, currentTakePath_) != 0)) {
        setStatus("Duplicate name blocked; choose another name");
        return;
    }
    editingFile_ = false;
    rearmMenuInputAfterFileEditor();
    setStatus("File name updated");
}

bool UsbRecordModal::currentInstrumentIsSample() const {
    if (!viewData_ || !viewData_->project_) return false;
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    I_Instrument *instrument = bank ? bank->GetInstrument(instrumentIndex_) : 0;
    return instrument && instrument->GetType() == IT_SAMPLE;
}

bool UsbRecordModal::validateCaptureWav(
    const char *path,
    long *dataBytes,
    int *frames,
    char *reason,
    int reasonLength) const {
    if (dataBytes) *dataBytes = 0;
    if (frames) *frames = 0;
    if (reason && reasonLength > 0) reason[0] = 0;

    struct stat info;
    if (!path || !path[0] || stat(path, &info) != 0 || info.st_size < 44) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Recording file is missing or empty");
        return false;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Recording file cannot be opened");
        return false;
    }

    unsigned char header[44];
    size_t offset = 0;
    while (offset < sizeof(header)) {
        ssize_t count = read(fd, header + offset, sizeof(header) - offset);
        if (count > 0) {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
    close(fd);

    if (offset != sizeof(header) ||
        memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0 ||
        memcmp(header + 12, "fmt ", 4) != 0 ||
        memcmp(header + 36, "data", 4) != 0) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Recording WAV header is invalid");
        return false;
    }

    const uint16_t format = readLe16(header + 20);
    const uint16_t channels = readLe16(header + 22);
    const uint32_t rate = readLe32(header + 24);
    const uint16_t blockAlign = readLe16(header + 32);
    const uint16_t bits = readLe16(header + 34);
    const uint32_t payload = readLe32(header + 40);
    const long available = (long)info.st_size - 44;

    if (format != 1 ||
        (channels != 1 && channels != 2) ||
        !isSupportedUsbRecordRate(rate) ||
        bits != 16 ||
        blockAlign != (uint16_t)(channels * 2) ||
        payload == 0 ||
        (long)payload > available ||
        (payload % blockAlign) != 0U) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Recording must be PCM 44.1/48kHz 16-bit");
        return false;
    }

    const uint32_t frameCount = payload / blockAlign;
    if (frameCount == 0 || frameCount > 5760000U) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Recording frame count is invalid");
        return false;
    }

    if (dataBytes) *dataBytes = (long)payload;
    if (frames) *frames = (int)frameCount;
    if (reason && reasonLength > 0)
        snprintf(reason, reasonLength, "valid");
    return true;
}

bool UsbRecordModal::captureFileStable(const char *path) const {
    struct stat before;
    struct stat after;
    if (!path || stat(path, &before) != 0) return false;
    TimeService::GetInstance()->Sleep(80);
    if (stat(path, &after) != 0) return false;
    return before.st_size == after.st_size &&
           before.st_mtime == after.st_mtime;
}

bool UsbRecordModal::preparePreview(
    const char *path,
    int *frames,
    char *reason,
    int reasonLength) {
    long bytes = 0;
    int frameCount = 0;
    if (!validateCaptureWav(
            path,
            &bytes,
            &frameCount,
            reason,
            reasonLength))
        return false;

    if (!captureFileStable(path)) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "WAV is still changing; retry Preview");
        return false;
    }

    WavFile *wav = WavFile::Open(path);
    if (!wav) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Preview decoder could not open WAV");
        return false;
    }
    const int decoderFrames = wav->GetSize(-1);
    const int decoderChannels = wav->GetChannelCount(-1);
    const int decoderRate = wav->GetSampleRate(-1);
    delete wav;

    if (decoderFrames <= 0 ||
        (decoderChannels != 1 && decoderChannels != 2) ||
        !isSupportedUsbRecordRate((uint32_t)decoderRate)) {
        if (reason && reasonLength > 0)
            snprintf(reason, reasonLength, "Preview decoder rejected WAV format");
        return false;
    }

    if (decoderFrames < frameCount) frameCount = decoderFrames;
    if (frames) *frames = frameCount;
    return true;
}

void UsbRecordModal::startRecording() {
    updateCaptureSnapshot(true);
    applyCaptureSnapshot();

    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING) {
        stopRecording();
        return;
    }
    if (sessionState_ == SESSION_FINALIZING) {
        setStatus("Recording is finalizing; wait");
        return;
    }
    if (sessionState_ == SESSION_READY ||
        sessionState_ == SESSION_PREVIEWING) {
        setStatus("Save or Discard the current take first");
        return;
    }
    if (editingFile_) {
        setStatus("Confirm the file name first");
        return;
    }
    if (!currentInstrumentIsSample()) {
        setSessionState(SESSION_ERROR, "Current instrument must be a sample instrument");
        return;
    }
    if (!TreeFrogUac2Bridge_IsRecordingDaemonReady()) {
        /* H32: Record must never restart an active Android USB session. */
        setSessionState(
            SESSION_ERROR,
            "Recording runtime is not ready; wait or restart LGPT");
        return;
    }
    if (!TreeFrogUac2Bridge_IsUsbReady()) {
        setSessionState(SESSION_ERROR, "USB audio source is not active");
        return;
    }

    updatePlannedPathFromStem();

    stopPreview(0);
    Player *player = Player::GetInstance();
    if (player) player->Stop();

    monitorBeforeRecord_ = monitorRequested_;
    makeTemporaryCapturePath();
    validatedPath_[0] = 0;
    validatedDataBytes_ = 0;
    validatedFrames_ = 0;

    if (!TreeFrogUac2Bridge_StartUsbCapture(
            currentTakePath_,
            kDurations[durationIndex_])) {
        setSessionState(SESSION_ERROR, "Recording command failed");
        return;
    }

    if (TreeFrogUac2Bridge_SetUsbMonitor(1)) {
        monitorRequested_ = true;
        setSessionState(SESSION_ARMING, "Starting recording; waiting for host audio");
    } else {
        monitorRequested_ = false;
        setSessionState(SESSION_ARMING, "Recording starting; input monitor unavailable");
    }
}

void UsbRecordModal::stopRecording() {
    if (sessionState_ != SESSION_ARMING &&
        sessionState_ != SESSION_RECORDING) {
        setStatus("No active recording");
        return;
    }

    if (!TreeFrogUac2Bridge_StopUsbCapture()) {
        setSessionState(SESSION_ERROR, "Stop command failed");
        return;
    }

    setSessionState(SESSION_FINALIZING, "Finalizing WAV; please wait");
}

void UsbRecordModal::stopPreview(const char *status) {
    if (!previewing_) return;
    Player *player = Player::GetInstance();
    if (player) player->StopStreaming();
    previewing_ = false;
    if (sessionState_ == SESSION_PREVIEWING)
        setSessionState(SESSION_READY, status ? status : "Preview stopped");
    else if (status)
        setStatus(status);
}

void UsbRecordModal::previewRecording() {
    updateCaptureSnapshot(true);
    applyCaptureSnapshot();

    if (sessionState_ != SESSION_READY) {
        setStatus("No finalized recording available");
        return;
    }

    const char *path = validatedPath_[0] ? validatedPath_ : currentTakePath_;
    int frames = 0;
    char reason[96];

    TreeFrogUac2Bridge_SetUsbMonitor(0);
    monitorRequested_ = false;

    Player *player = Player::GetInstance();
    if (!player) {
        setSessionState(SESSION_ERROR, "Player is unavailable");
        return;
    }

    player->StopStreaming();
    TimeService::GetInstance()->Sleep(80);

    if (!preparePreview(path, &frames, reason, sizeof(reason))) {
        setSessionState(SESSION_ERROR, reason);
        return;
    }

    Path source(path);
    player->StartStreamingRangeAt(source, 0, frames - 1);
    previewing_ = true;
    setSessionState(SESSION_PREVIEWING, "Preview on console; B stops");
}

void UsbRecordModal::saveRecording() {
    if (sessionState_ != SESSION_READY &&
        sessionState_ != SESSION_PREVIEWING) {
        setStatus("No finalized recording available");
        return;
    }
    if (!currentInstrumentIsSample()) {
        setSessionState(SESSION_ERROR, "Current instrument is not a sample instrument");
        return;
    }
    if (editingFile_) {
        setStatus("Confirm the file name first");
        return;
    }

    stopPreview(0);
    TreeFrogUac2Bridge_SetUsbMonitor(0);
    monitorRequested_ = false;

    const char *sourcePath = validatedPath_[0] ? validatedPath_ : currentTakePath_;
    long bytes = 0;
    int frames = 0;
    char reason[128];
    if (!validateCaptureWav(sourcePath, &bytes, &frames, reason, sizeof(reason))) {
        setSessionState(SESSION_ERROR, reason);
        return;
    }

    updatePlannedPathFromStem();
    if (recordNameExistsExact(plannedName_) &&
        strcmp(sourcePath, plannedPath_) != 0) {
        setStatus("Duplicate name blocked; edit File name");
        return;
    }

    if (!promoteCaptureToFinalPath(
            sourcePath,
            plannedPath_,
            reason,
            sizeof(reason))) {
        setStatus(reason);
        return;
    }

    snprintf(currentTakePath_, sizeof(currentTakePath_), "%s", plannedPath_);
    snprintf(validatedPath_, sizeof(validatedPath_), "%s", plannedPath_);
    validateCaptureWav(
        validatedPath_,
        &validatedDataBytes_,
        &validatedFrames_,
        reason,
        sizeof(reason));

    /* The daemon still owns metadata for the former temporary path.  Clear only
     * that runtime state after the final file has been promoted successfully. */
    TreeFrogUac2Bridge_CommitUsbCapture();

    Path source(validatedPath_);
    const int sampleIndex = SamplePool::GetInstance()->ImportSample(source);
    if (sampleIndex < 0) {
        setSessionState(
            SESSION_READY,
            "WAV saved in records; Import failed, retry Save");
        return;
    }

    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    I_Instrument *instrument = bank ? bank->GetInstrument(instrumentIndex_) : 0;
    if (!instrument || instrument->GetType() != IT_SAMPLE) {
        setSessionState(
            SESSION_READY,
            "WAV saved in records; instrument changed");
        return;
    }

    ((SampleInstrument *)instrument)->AssignSample(sampleIndex);
    requestClose(true);
}

void UsbRecordModal::discardRecording() {
    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING ||
        sessionState_ == SESSION_FINALIZING) {
        setStatus("Stop and finalize before Discard");
        return;
    }

    stopPreview(0);
    TreeFrogUac2Bridge_DiscardUsbCapture();
    if (currentTakePath_[0]) unlink(currentTakePath_);
    currentTakePath_[0] = 0;
    validatedPath_[0] = 0;
    validatedDataBytes_ = 0;
    validatedFrames_ = 0;
    makeNextCapturePath();
    setSessionState(SESSION_IDLE, "Recording discarded");
}

void UsbRecordModal::exitModal() {
    if (sessionState_ == SESSION_READY ||
        sessionState_ == SESSION_PREVIEWING) {
        setStatus("Save or Discard the current take first");
        return;
    }
    if (sessionState_ == SESSION_ARMING ||
        sessionState_ == SESSION_RECORDING) {
        closePending_ = true;
        closeSaved_ = false;
        stopRecording();
        setStatus("Stopping recording before exit");
        return;
    }
    if (sessionState_ == SESSION_FINALIZING) {
        closePending_ = true;
        closeSaved_ = false;
        setStatus("Waiting for WAV finalization before exit");
        return;
    }

    stopPreview(0);
    TreeFrogUac2Bridge_SetUsbMonitor(0);
    monitorRequested_ = false;
    requestClose(false);
}

bool UsbRecordModal::executeSelectedAction() {
    switch (selected_) {
        case ITEM_MONITOR: toggleMonitor(); return true;
        case ITEM_DURATION: cycleDuration(1); return true;
        case ITEM_FILE: beginFileEdit(); return true;
        case ITEM_RECORD: startRecording(); return true;
        case ITEM_PREVIEW: previewRecording(); return true;
        case ITEM_SAVE: saveRecording(); return true;
        case ITEM_DISCARD: discardRecording(); return true;
        case ITEM_EXIT: exitModal(); return true;
        default: return false;
    }
}

void UsbRecordModal::DrawView() {
    SetWindow(36, 24);
    GUITextProperties props;
    char line[64];

    SetColor(CD_HILITE1);
    props.invert_ = true;
    DrawString(0, 0, "       USB-C RECORD / SAMPLER       ", props);
    props.invert_ = false;

    const bool pendingTake =
        (sessionState_ == SESSION_READY ||
         sessionState_ == SESSION_PREVIEWING) &&
        currentTakePath_[0] != 0;

    SetColor(CD_NORMAL);
    snprintf(line, sizeof(line), "USB: %-29.29s", usbState_);
    DrawString(1, 2, line, props);
    SetColor(pendingTake ? CD_HILITE2 : CD_NORMAL);
    snprintf(line, sizeof(line), "State: %-27.27s", sessionStateName());
    DrawString(1, 3, line, props);
    snprintf(line, sizeof(line), "File: %-28.28s", plannedName_);
    DrawString(1, 4, line, props);

    const int level = capture_.levelPercent;
    int bars = (level * 18 + 50) / 100;
    if (bars < 0) bars = 0;
    if (bars > 18) bars = 18;
    char meter[20];
    for (int i = 0; i < 18; ++i) meter[i] = i < bars ? '|' : ' ';
    meter[18] = 0;
    snprintf(line, sizeof(line), "IN [%s] %3d%%", meter, level);
    DrawString(1, 5, line, props);

    snprintf(
        line,
        sizeof(line),
        "Time %3ds  Data %6ldKB",
        capture_.elapsedSeconds,
        capture_.bytes / 1024L);
    SetColor(CD_NORMAL);
    DrawString(1, 6, line, props);

    if (pendingTake) {
        SetColor(CD_HILITE2);
        props.invert_ = true;
        DrawString(1, 7, "  TAKE PENDING: PREVIEW / SAVE / DISCARD ", props);
        props.invert_ = false;
    } else {
        SetColor(CD_NORMAL);
        DrawString(1, 7, "                                        ", props);
    }

    static const char *labels[ITEM_COUNT] = {
        "Input monitor",
        "Duration",
        "File name",
        "Start / Stop recording",
        "Preview last recording",
        "Save to Instrument",
        "Discard recording",
        "Exit to Instrument"
    };

    for (int item = 0; item < ITEM_COUNT; ++item) {
        props.invert_ = item == selected_;
        if (item == ITEM_FILE && editingFile_)
            SetColor(CD_HILITE2);
        else
            SetColor(item == selected_ ? CD_HILITE2 : CD_NORMAL);

        if (item == ITEM_MONITOR) {
            snprintf(
                line,
                sizeof(line),
                "%c %-20s [%s]",
                item == selected_ ? '>' : ' ',
                labels[item],
                monitorRequested_ ? "ON" : "OFF");
        } else if (item == ITEM_DURATION) {
            snprintf(
                line,
                sizeof(line),
                "%c %-20s [%3ds]",
                item == selected_ ? '>' : ' ',
                labels[item],
                kDurations[durationIndex_]);
        } else if (item == ITEM_FILE) {
            const int viewWidth = 16;
            const int viewStart = editingFile_ ? fileEditorViewStart(viewWidth) : 0;
            char visible[17];
            memset(visible, 0, sizeof(visible));
            snprintf(visible, sizeof(visible), "%-16.16s", fileStem_ + viewStart);
            snprintf(
                line,
                sizeof(line),
                "%c %-12s [%s]",
                item == selected_ ? '>' : ' ',
                labels[item],
                visible);
        } else if (item == ITEM_RECORD) {
            const char *action =
                (sessionState_ == SESSION_ARMING ||
                 sessionState_ == SESSION_RECORDING) ?
                    "Stop recording" : "Start recording";
            snprintf(
                line,
                sizeof(line),
                "%c %-32.32s",
                item == selected_ ? '>' : ' ',
                action);
        } else {
            snprintf(
                line,
                sizeof(line),
                "%c %-32.32s",
                item == selected_ ? '>' : ' ',
                labels[item]);
        }

        if (item == ITEM_RECORD && sessionState_ == SESSION_RECORDING)
            SetColor(CD_RECORD);
        else if (pendingTake &&
                 (item == ITEM_PREVIEW ||
                  item == ITEM_SAVE ||
                  item == ITEM_DISCARD))
            SetColor(CD_HILITE2);
        DrawString(1, 8 + item, line, props);
        props.invert_ = false;
    }

    SetColor(CD_NORMAL);
    snprintf(line, sizeof(line), "%-35.35s", status_);
    DrawString(1, 17, line, props);

    if (editingFile_) {
        const int viewWidth = 23;
        const int viewStart = fileEditorViewStart(viewWidth);
        char visible[24];
        memset(visible, 0, sizeof(visible));
        snprintf(visible, sizeof(visible), "%-23.23s", fileStem_ + viewStart);
        snprintf(
            line,
            sizeof(line),
            "RENAME %s [%s]",
            fileLowercaseMode_ ? "abc" : "ABC",
            visible);
        SetColor(CD_HILITE2);
        DrawString(1, 18, line, props);

        char cursorLine[36];
        memset(cursorLine, ' ', sizeof(cursorLine));
        cursorLine[35] = 0;
        int caret = 12 + (fileCursor_ - viewStart);
        if (caret < 12) caret = 12;
        if (caret > 34) caret = 34;
        cursorLine[caret] = '^';
        SetColor(CD_HILITE1);
        DrawString(1, 19, cursorLine, props);
        SetColor(CD_NORMAL);
        DrawString(1, 20, "UP/DN +/-1  X+UP/DN +/-5", props);
        DrawString(1, 21, "L/R cursor L1+X case A OK B del", props);
    } else if (!inputArmed_ || closePending_) {
        DrawString(1, 19, "Release all controls", props);
        DrawString(1, 20, "Input resumes after neutral", props);
    } else {
        DrawString(1, 19, "A select  UP/DOWN  LEFT/RIGHT", props);
        DrawString(1, 20, "R1+LEFT back; B stops Preview", props);
    }

    DrawString(1, 22, "Preview follows the active audio driver", props);
    DrawString(1, 23, "Unsaved take is temporary until Save", props);
}

void UsbRecordModal::ProcessButtonMask(
    unsigned short mask,
    bool pressed) {
    const unsigned short previous = activeInputMask_;
    activeInputMask_ = mask;

    /* U2.51.9: File name editing is driven only by the authoritative physical
     * snapshot in OnFrameUpdate(). Queued frontend events are ignored while
     * editing so monitor/capture activity cannot duplicate or suppress keys. */
    if (editingFile_) return;
    if (!inputArmed_ || closePending_) return;
    if (!pressed) return;

    const unsigned short newBits =
        (unsigned short)(mask & (unsigned short)~previous);
    if (newBits == 0) return;

    if ((mask & kExitChord) == kExitChord &&
        (newBits & kExitChord) != 0) {
        if (editingFile_) cancelFileEdit();
        exitModal();
        return;
    }

    /* Only menu-action bits participate in the one-action rule.  Shoulder,
     * START or SELECT bits arriving in the same atomic frontend snapshot must
     * not make A/B/direction appear ignored.  R1+LEFT was handled above. */
    const unsigned short actionBits =
        (unsigned short)(newBits &
            (EPBM_UP | EPBM_DOWN | EPBM_LEFT | EPBM_RIGHT |
             EPBM_A | EPBM_B | EPBM_X | EPBM_Y));
    if (actionBits == 0) return;

    if (previewing_ && (actionBits & EPBM_B) != 0) {
        stopPreview("Preview stopped");
        return;
    }

    if (countBits(actionBits) != 1) {
        setStatus("Multiple menu actions ignored; release and retry");
        return;
    }

    if (actionBits == EPBM_UP) {
        moveSelection(-1);
        return;
    }
    if (actionBits == EPBM_DOWN) {
        moveSelection(1);
        return;
    }
    if (actionBits == EPBM_LEFT) {
        if (selected_ == ITEM_DURATION) cycleDuration(-1);
        else setStatus("LEFT changes Duration or edits File name");
        return;
    }
    if (actionBits == EPBM_RIGHT) {
        if (selected_ == ITEM_DURATION) cycleDuration(1);
        else setStatus("RIGHT changes Duration or edits File name");
        return;
    }
    if (actionBits == EPBM_A) {
        executeSelectedAction();
        return;
    }
    if (actionBits == EPBM_B) {
        setStatus("Use R1+LEFT to return");
        return;
    }
}

void UsbRecordModal::OnFrameUpdate(unsigned long frameClock) {
    (void)frameClock;
    ++frameCounter_;

    if (editingFile_) processFileEditorPhysicalInput();
    else updateInputArming();

    const unsigned int pollInterval =
        (sessionState_ == SESSION_ARMING ||
         sessionState_ == SESSION_RECORDING ||
         sessionState_ == SESSION_FINALIZING) ? 2U : 6U;

    ++capturePollDivider_;
    if (capturePollDivider_ >= pollInterval) {
        capturePollDivider_ = 0;
        updateCaptureSnapshot(false);
        applyCaptureSnapshot();
    }

    if (closePending_) completeCloseWhenNeutral();

    ++drawRefreshDivider_;
    if (drawRefreshDivider_ >= 4) {
        drawRefreshDivider_ = 0;
        isDirty_ = true;
    }
}

void UsbRecordModal::OnPlayerUpdate(
    PlayerEventType,
    unsigned int) {
    if (previewing_) isDirty_ = true;
}

void UsbRecordModal::OnFocus() {
    isDirty_ = true;
}
