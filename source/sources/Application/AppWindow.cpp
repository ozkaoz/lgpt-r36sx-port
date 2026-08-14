#include "AppWindow.h"
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Commands/EventDispatcher.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Player/TablePlayback.h"
#include "Application/UI/Input/ChordResolver.h"
#include "Application/Utils/char.h"
#include "Application/UI/Views/ModalDialogs/MessageBox.h"
#include "Application/UI/Views/ModalDialogs/SelectProjectDialog.h"
#include "Application/UI/Views/ModalDialogs/AudioDriverModal.h"
#include "Application/UI/Views/BaseClasses/HelpOverlay.h"
#include "Foundation/Variables/WatchedVariable.h"
#include "Player/Player.h"
#include "Services/Midi/MidiService.h"
#include "System/Console/Trace.h"
#include "UIFramework/Interfaces/I_GUIWindowFactory.h"
#include "UI/Views/UIController.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#if defined(PLATFORM_TREEFROG)
extern "C" void TreeFrogChopperOverlayDraw(void);
extern "C" void TreeFrogInstrumentEqOverlayDraw(void);
#endif


// TREEFROG_APP_EVENT_DEBUG_HELPERS_V1
static void treefrog_app_debug_log(const char *where,
                                   int eventType,
                                   int eventValue,
                                   unsigned short mask,
                                   bool dirty) {
#if TREEFROG_INPUT_DEBUG
    FILE *f = fopen("/tmp/r36sx_lgpt_logs/app_debug.log", "a");
    if (!f) return;

    fprintf(f,
            "%lu %s eventType=%d eventValue=%d mask=0x%04x"
            " A=%d B=%d X=%d Y=%d L=%d R=%d START=%d dirty=%d\n",
            (unsigned long)System::GetInstance()->GetClock(),
            where ? where : "app",
            eventType,
            eventValue,
            mask,
            (mask & EPBM_A) ? 1 : 0,
            (mask & EPBM_B) ? 1 : 0,
            (mask & EPBM_X) ? 1 : 0,
            (mask & EPBM_Y) ? 1 : 0,
            (mask & EPBM_L) ? 1 : 0,
            (mask & EPBM_R) ? 1 : 0,
            (mask & EPBM_START) ? 1 : 0,
            dirty ? 1 : 0);

    fclose(f);
#else
    (void)where;
    (void)eventType;
    (void)eventValue;
    (void)mask;
    (void)dirty;
#endif
}

#ifndef TREEFROG_DISABLE_ALL_UI_INVERT
#define TREEFROG_DISABLE_ALL_UI_INVERT 0
#endif


extern "C" const char *TreeFrogU2430AppBuildMarker(void) {
    return "U2430_EVENT_TIMESTAMP_TO_MODAL";
}

extern "C" const char *TreeFrogU2440FrameTickBuildMarker(void) {
    return "U2440_ACTIVE_MODAL_FRAME_TICK";
}


#if defined(PLATFORM_TREEFROG)
static const char *kH35ExportRequest = "/tmp/r36sx_lgpt_usb/export_request";
static unsigned long long h35ExportNowMs() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)ts.tv_nsec / 1000000ULL;
}
static bool h35AtomicText(const char *path, const char *text) {
    char temp[512];
    snprintf(temp, sizeof(temp), "%s.tmp.%ld", path, (long)getpid());
    int fd = open(temp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;
    size_t len = text ? strlen(text) : 0;
    ssize_t n = len ? write(fd, text, len) : 0;
    bool ok = (size_t)(n < 0 ? 0 : n) == len && fsync(fd) == 0;
    close(fd);
    if (!ok || rename(temp, path) != 0) { unlink(temp); return false; }
    return true;
}
static bool h35FileExists(const char *path) {
    struct stat st; return path && stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 44;
}
static bool h35ReadText(const char *path, char *buffer, size_t size) {
    if (!path || !buffer || size < 2) return false;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    ssize_t n = read(fd, buffer, size - 1);
    close(fd);
    if (n <= 0) return false;
    buffer[n] = 0;
    return true;
}
#endif

AppWindow *instance = 0;

extern "C" void TreeFrogInputTrace_LogView(
    const char *phase,
    int viewType,
    int hasModal,
    unsigned short incomingMask,
    unsigned short activeMask,
    int pressed,
    int audioLatched);

GUIColor AppWindow::backgroundColor_(0x0A, 0x0A, 0x18);
GUIColor AppWindow::normalColor_(0xE8, 0xE4, 0xF8);
GUIColor AppWindow::borderColor_(0x3F, 0x5F, 0xBF);
GUIColor AppWindow::songviewfeColor_(0x2A, 0x3E, 0x8F);
GUIColor AppWindow::songview00Color_(0x1E, 0x2B, 0x66);
GUIColor AppWindow::highlightColor_(0x5B, 0x8C, 0xFF);
GUIColor AppWindow::highlight2Color_(0x9D, 0x5B, 0xFF);
GUIColor AppWindow::consoleColor_(0x40, 0xC8, 0x78);
GUIColor AppWindow::cursorColor_(0x7F, 0xB8, 0xFF);
GUIColor AppWindow::playColor_(0x4A, 0xD8, 0xFF);
GUIColor AppWindow::recordColor_(0xFF, 0x40, 0x40);
GUIColor AppWindow::muteColor_(0x88, 0x90, 0xB0);
GUIColor AppWindow::rownumberColor_(0x5A, 0x7D, 0xF0);
GUIColor AppWindow::rownumber2Color_(0xA8, 0x6B, 0xFF);
GUIColor AppWindow::majorbeatColor_(0x4A, 0x8C, 0xFF);
GUIColor AppWindow::warningColor_(0xF0, 0xA0, 0x30);
GUIColor AppWindow::errorColor_(0xFF, 0x50, 0x50);
AppWindow *AppWindow::instance_ = 0;

int AppWindow::charWidth_ = 8;
int AppWindow::charHeight_ = 8;

// #define _FORCE_SDL_EVENT_

static void ProjectSelectCallback(View &v, ModalView &dialog) {

    SelectProjectDialog &spd = (SelectProjectDialog &)dialog;
    if (dialog.GetReturnCode() > 0) {
        Path selected = spd.GetSelection();
        instance->SaveLastProject(selected);
        instance->LoadProject(selected.GetPath().c_str());
    } else {
        System::GetInstance()->PostQuitMessage();
    }
};

void AppWindow::defineColor(const char *colorName, GUIColor &color) {

    Config *config = Config::GetInstance();
    const char *value = config->GetValue(colorName);
    if (value) {
        unsigned char r;
        char2hex(value, &r);
        unsigned char g;
        char2hex(value + 2, &g);
        unsigned char b;
        char2hex(value + 4, &b);
        color = GUIColor(r, g, b);
    }
}

AppWindow::AppWindow(I_GUIWindowImp &imp) : GUIWindow(imp) {

    instance = this;

    // Init all members

    _statusLine[0] = 0;

    _currentView = 0;
    _viewData = 0;
    _songView = 0;
    _chainView = 0;
    _phraseView = 0;
    _projectView = 0;
    _instrumentView = 0;
    _tableView = 0;
    _nullView = 0;
    _mixerView = 0;
    _grooveView = 0;
    _closeProject = 0;
    _loadAfterSaveAsProject = 0;
    _loadAfterResume = 0;
    _lastA = 0;
    _lastB = 0;
    _mask = 0;
    _audioShortcutLatched = false;
    _helpShortcutLatched = false;
    _pendingShoulderPress = 0;
    _lastShoulderPress = 0;
    _chordLastWasRedo = false;
    colorIndex_ = CD_NORMAL;

    EventDispatcher *ed = EventDispatcher::GetInstance();
    ed->SetWindow(this);

    Status::Install(this);

    // Init midi services
    MidiService::GetInstance()->Init();

    defineColor("BACKGROUND", backgroundColor_);
    defineColor("FOREGROUND", normalColor_);
    defineColor("BORDER", borderColor_);
    defineColor("SONGVIEW_FE", songviewfeColor_);
    defineColor("SONGVIEW_00", songview00Color_);
    defineColor("HICOLOR1", highlightColor_);
    defineColor("HICOLOR2", highlight2Color_);
    defineColor("CURSORCOLOR", cursorColor_);
    defineColor("PLAYCOLOR", playColor_);
    defineColor("RECORDCOLOR", recordColor_);
    defineColor("MUTECOLOR", muteColor_);
    defineColor("ROWCOLOR1", rownumberColor_);
    defineColor("ROWCOLOR2", rownumber2Color_);
    defineColor("MAJORBEAT", majorbeatColor_);
    defineColor("WARNING", warningColor_);
    defineColor("ERROR", errorColor_);

    GUIWindow::Clear(backgroundColor_);

    _nullView = new NullView((*this), 0);
    _currentView = _nullView;
    _nullView->SetDirty(true);

    // TreeFrog/R36SX port policy: always start at the LGPT main menu.
    // Do not auto-load LAST_PROJECT_NAME on boot, even if it exists from a
    // previous session.  SaveLastProject() is still kept for compatibility,
    // but startup must be deterministic for non-technical users.
    Trace::Log("AppWindow", "Startup: showing LGPT main menu");
    SelectProjectDialog *spd = new SelectProjectDialog(*_currentView);
    _currentView->DoModal(spd, ProjectSelectCallback);

    memset(_charScreen, ' ', 1200);
    memset(_preScreen, ' ', 1200);
    memset(_charScreenProp, 0, 1200);
    memset(_preScreenProp, 0, 1200);

    Redraw();
};

AppWindow::~AppWindow() { MidiService::GetInstance()->Close(); }

void AppWindow::DrawString(const char *string, GUIPoint &pos,
                           GUITextProperties &props, bool force) {

    // we know we don't have mode than 40 chars

    char buffer[41];
    int len = strlen(string);
    int offset = (pos._x < 0) ? -pos._x / 8 : 0;
    len -= offset;
    int available = 40 - ((pos._x < 0) ? 0 : pos._x);
    len = MIN(len, available);
    memcpy(buffer, string + offset, len);
    buffer[len] = 0;

    NAssert((pos._x < 40) && (pos._y < 30));
    int index = pos._x + 40 * pos._y;
    memcpy(_charScreen + index, buffer, len);
    #if TREEFROG_DISABLE_ALL_UI_INVERT
    unsigned char prop = colorIndex_;
#else
    unsigned char prop = colorIndex_ + (props.invert_ ? PROP_INVERT : 0);
#endif
    memset(_charScreenProp + index, prop, len);
};

void AppWindow::Clear(bool all) {
    memset(_charScreen, ' ', 1200);
    memset(_charScreenProp, 0, 1200);
    if (all) {
        memset(_preScreen, ' ', 1200);
        memset(_preScreenProp, 0, 1200);
    };
};

void AppWindow::ClearRect(GUIRect &r) {

    int x = r.Left();
    int y = r.Top();
    int w = r.Width();
    int h = r.Height();

    unsigned char *st = _charScreen + x + (40 * y);
    unsigned char *pr = _charScreenProp + x + (40 * y);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            *st++ = ' ';
            *pr++ = 0;
        }
        st += (40 - w);
        pr += (40 - w);
    }
};

//
// Redraws the screen and flush it.
//

void AppWindow::Redraw() {

    SysMutexLocker locker(drawMutex_);

    if (_currentView) {
        _currentView->Redraw();
        Invalidate();
    }
};

//
// Flush current screen to display
//

void AppWindow::Flush() {

    SysMutexLocker locker(drawMutex_);

    Lock();
    long flushStart = System::GetInstance()->GetClock();

    GUITextProperties props;
    GUIPoint pos;

    ColorDefinition color = (ColorDefinition)-1;
    pos._x = 0;
    pos._y = 0;

    int count = 0;

    unsigned char *current = _charScreen;
    unsigned char *previous = _preScreen;
    unsigned char *currentProp = _charScreenProp;
    unsigned char *previousProp = _preScreenProp;
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 40; x++) {
#ifndef _LGPT_NO_SCREEN_CACHE_
            if ((*current != *previous) || (*currentProp != *previousProp)) {
#endif
                props.invert_ = (*currentProp & PROP_INVERT) != 0;
                if (((*currentProp) & 0x7F) != color) {
                    color = (ColorDefinition)((*currentProp) & 0x7F);
                    GUIColor gcolor = normalColor_;
                    switch (color) {
                    case CD_BACKGROUND:
                        gcolor = backgroundColor_;
                        break;
                    case CD_NORMAL:
                        break;
                    case CD_BORDER:
                        gcolor = borderColor_;
                        break;
                    case CD_HILITE1:
                        gcolor = highlightColor_;
                        break;
                    case CD_HILITE2:
                        gcolor = highlight2Color_;
                        break;
                    case CD_CONSOLE:
                        gcolor = consoleColor_;
                        break;
                    case CD_CURSOR:
                        gcolor = cursorColor_;
                        break;
                    case CD_PLAY:
                        gcolor = playColor_;
                        break;
                    case CD_RECORD:
                        gcolor = recordColor_;
                        break;
                    case CD_MUTE:
                        gcolor = muteColor_;
                        break;
                    case CD_SONGVIEWFE:
                        gcolor = songviewfeColor_;
                        break;
                    case CD_SONGVIEW00:
                        gcolor = songview00Color_;
                        break;
                    case CD_ROW:
                        gcolor = rownumberColor_;
                        break;
                    case CD_ROW2:
                        gcolor = rownumber2Color_;
                        break;
                    case CD_MAJORBEAT:
                        gcolor = majorbeatColor_;
                        break;
                    case CD_WARNING:
                        gcolor = warningColor_;
                        break;
                    case CD_ERROR:
                        gcolor = errorColor_;
                        break;
                    default:
                        NAssert(0);
                        break;
                    }
                    GUIWindow::SetColor(gcolor);
                }
                GUIWindow::DrawChar(*current, pos, props);
                count++;
#ifndef _LGPT_NO_SCREEN_CACHE_
            }
#endif
            current++;
            previous++;
            currentProp++;
            previousProp++;
            pos._x += AppWindow::charWidth_;
        }
        pos._y += AppWindow::charHeight_;
        pos._x = 0;
    }
    long flushEnd = System::GetInstance()->GetClock();
#if defined(PLATFORM_TREEFROG)
    TreeFrogChopperOverlayDraw();
    TreeFrogInstrumentEqOverlayDraw();
#endif
    // TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1): after the char screen
    // is fully rendered, let the active view paint its pixel-level layer
    // (the L/R half-cell mixer bars).  Runs on every Flush so the bars
    // always sit on top of the repainted cells.
    if (_currentView) _currentView->PostFlushDraw();
    GUIWindow::Flush();
    Unlock();
    memcpy(_preScreen, _charScreen, 1200);
    memcpy(_preScreenProp, _charScreenProp, 1200);
};

void AppWindow::LoadProject(const Path &p) {
    Trace::Log("LoadProject", "%s\n", p.GetPath().c_str());
    _root = p;

    _closeProject = false;
    _loadAfterSaveAsProject = false;

    PersistencyService *persist = PersistencyService::GetInstance();

    TablePlayback::Reset();

    Path::SetAlias("project", _root.GetPath().c_str());
    Path::SetAlias("samples", "project:samples");

    // Load the sample pool

    SamplePool *pool = SamplePool::GetInstance();

    pool->Load();

    Project *project = new Project();

    bool succeeded = persist->Load();
    if (!succeeded) {
        project->GetInstrumentBank()->AssignDefaults();
    };

    // Project

    WatchedVariable::Disable();

    project->GetInstrumentBank()->Init();

    WatchedVariable::Enable();

    ApplicationCommandDispatcher::GetInstance()->Init(project);

    // Create view data

    _viewData = new ViewData(project);

    // Create & observe the player
    Player *player = Player::GetInstance();
    bool playerOK = player->Init(project, _viewData);
    player->AddObserver(*this);

    // Create the controller
    UIController *controller = UIController::GetInstance();
    controller->Init(project, _viewData);

    // Create & observe all views
    _songView = new SongView((*this), _viewData, _root.GetName().c_str());
    _songView->AddObserver((*this));

    _chainView = new ChainView((*this), _viewData);
    _chainView->AddObserver((*this));

    _phraseView = new PhraseView((*this), _viewData);
    _phraseView->AddObserver((*this));

    _projectView = new ProjectView((*this), _viewData);
    _projectView->AddObserver((*this));

    _instrumentView = new InstrumentView((*this), _viewData);
    _instrumentView->AddObserver((*this));

    _tableView = new TableView((*this), _viewData);
    _tableView->AddObserver((*this));

    _grooveView = new GrooveView((*this), _viewData);
    _grooveView->AddObserver(*this);

    _mixerView = new MixerView((*this), _viewData);
    _mixerView->AddObserver(*this);

    _currentView = _songView;
    _currentView->OnFocus();

    // TREEFROG_V40_ESTABLE_APPWINDOW
    // Si PersistencyService::Load() falló, LGPT generó un proyecto por defecto en memoria.
    // En R36SX/TreeFrog, salir sin guardarlo deja una carpeta lgpt_* sin lgptsav.dat;
    // esa carpeta rompe la siguiente ejecución en retro_run(frame=0). Guardamos una
    // persistencia inicial inmediatamente después de completar la inicialización.
    // TREEFROG_SAVE_SYNC_V1 (Bacon 1.1.1): solo se autoguarda cuando NO existe
    // lgptsav.dat.  Un archivo existente que falló al parsear (FAT cortada por
    // apagado brusco, formato antiguo...) no debe ser pisado con un proyecto
    // vacío por defecto: eso destruiría datos recuperables.
    if (!succeeded) {
        Path initialCheck("project:lgptsav.dat");
        if (!initialCheck.Exists()) {
            PersistencyService::GetInstance()->Save();
            Trace::Log("TREEFROG_V40_ESTABLE", "autosaved initial project: %s", _root.GetPath().c_str());
            FILE *fp = fopen("/tmp/r36sx_lgpt_logs/reentry_debug.log", "a");
            if (fp) {
                fprintf(fp, "TREEFROG_V40_ESTABLE_APPWINDOW: autosaved_initial_project path=%s\n", _root.GetPath().c_str());
                fclose(fp);
            }
        } else {
            Trace::Log("TREEFROG_SAVE_SYNC_V1", "load failed but lgptsav.dat exists; NOT overwriting with default project");
        }
    }


    if (!playerOK) {
        MessageBox *mb =
            new MessageBox(*_songView, "Failed to initialize audio", MBBF_OK);
        _songView->DoModal(mb);
    }

    Redraw();
}

void AppWindow::CloseProject() {

    _closeProject = false;
    Player *player = Player::GetInstance();
    player->Stop();
    player->RemoveObserver(*this);

    player->Reset();

    SamplePool *pool = SamplePool::GetInstance();
    pool->Reset();

    TableHolder::GetInstance()->Reset();
    TablePlayback::Reset();

    ApplicationCommandDispatcher::GetInstance()->Close();

    SAFE_DELETE(_songView);
    SAFE_DELETE(_chainView);
    SAFE_DELETE(_phraseView);
    SAFE_DELETE(_projectView);
    SAFE_DELETE(_instrumentView);
    SAFE_DELETE(_tableView);
    SAFE_DELETE(_mixerView);
    SAFE_DELETE(_grooveView);

    UIController *controller = UIController::GetInstance();
    controller->Reset();

    SAFE_DELETE(_viewData);

    _currentView = _nullView;
    _nullView->SetDirty(true);

    SelectProjectDialog *spd = new SelectProjectDialog(*_currentView);
    _currentView->DoModal(spd, ProjectSelectCallback);
};

AppWindow *AppWindow::Create(GUICreateWindowParams &params) {
    I_GUIWindowImp &imp =
        I_GUIWindowFactory::GetInstance()->CreateWindowImp(params);
    AppWindow *w = new AppWindow(imp);
    AppWindow::instance_ = w;
    return w;
};

// TREEFROG_PROJECT_RENAME_V1 (H38.5): singleton accessor.
AppWindow *AppWindow::GetInstance() { return AppWindow::instance_; };

// SD lifecycle U2.54b SHUTDOWN_VISIBLE: public toast hook. retro_run calls
// this while the core still owns the screen at exit, so the user sees the
// SD-sync notice before picoarch returns to TreeFrogUI.
void AppWindow::ShowShutdownNotice(const char *msg) {
    if (_currentView) _currentView->SetNotification(msg);
};

void AppWindow::SetDirty() { _isDirty = true; };

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): writes the same export request the
// Android host would write, so H35PollExternalExport drives the render once
// the project has finished loading. mode 1 = mixdown (master), 2 = stems
// (multitrack); command mapping matches H35 (2=mixdown, 3=stems).
void AppWindow::RequestExportRender(int mode) {
#if defined(PLATFORM_TREEFROG)
    if (mode != 1 && mode != 2) return;
    static unsigned int s_exportSession = 0;
    unsigned int session = ++s_exportSession;
    char line[256];
    snprintf(line, sizeof(line), "REQUEST command=%u format=1 session=%u\n",
             (mode == 1) ? 2 : 3, session);
    h35AtomicText(kH35ExportRequest, line);
#endif
}

// F1b input policy (REFACTOR_ROADMAP_ES.md): los acordes globales del golden
// (helpCombo SELECT+R1 = EPBM_SELECT | EPBM_R, audioCombo SELECT+R2 =
// EPBM_SELECT | EPBM_R2) quedan expresados en el catalogo CTX_GLOBAL
// (ActionMap.cpp) y aqui se consultan por accion semantica. El estado de
// latcheo y la desambiguacion V16 de L1+R1+X (que requiere historia de
// prensas) siguen siendo runtime del adapter, como documenta la tabla.
static bool GlobalHelpChord(unsigned short mask) {
    return UI::Input::ChordResolver_Matches(
        (UI::Input::PadMask)mask, UI::Input::CTX_GLOBAL,
        UI::Input::ACTION_OPEN_HELP);
}
static bool GlobalAudioChord(unsigned short mask) {
    return UI::Input::ChordResolver_Matches(
        (UI::Input::PadMask)mask, UI::Input::CTX_GLOBAL,
        UI::Input::ACTION_OPEN_AUDIO_DRIVER);
}
static bool GlobalHelpReleased(unsigned short mask) {
    return UI::Input::ChordResolver_ChordAbsent(
        (UI::Input::PadMask)mask, UI::Input::CTX_GLOBAL,
        UI::Input::ACTION_OPEN_HELP);
}
static bool GlobalAudioReleased(unsigned short mask) {
    return UI::Input::ChordResolver_ChordAbsent(
        (UI::Input::PadMask)mask, UI::Input::CTX_GLOBAL,
        UI::Input::ACTION_OPEN_AUDIO_DRIVER);
}

void AppWindow::SynchronizeInputMask(unsigned short mask) {
    _mask = mask;
    if (_audioShortcutLatched &&
        !GlobalAudioChord(mask)) {
        _audioShortcutLatched = false;
    }
    // TREEFROG_HELP_OVERLAY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 13):
    // the help latch clears once SELECT+R1 (R1 = EPBM_R) is released.
    if (_helpShortcutLatched &&
        !GlobalHelpChord(mask)) {
        _helpShortcutLatched = false;
    }
}

extern "C" void TreeFrogAppWindow_SynchronizeInputMask(
    unsigned short mask) {
    if (instance)
        instance->SynchronizeInputMask(mask);
}

bool AppWindow::onEvent(GUIEvent &event) {

    // We need to tell the app to quit once we're out of the
    // mixer lock, otherwise the windows driver will never return

    _shouldQuit = false;

    _isDirty = false;

    unsigned short v = 0;
    if (event.GetType() == ET_PADBUTTONDOWN ||
        event.GetType() == ET_PADBUTTONUP) {
        const long buttonValue = event.GetValue();
        if (buttonValue >= 0 && buttonValue < 16)
            v = (unsigned short)(1u << (unsigned)buttonValue);
    }

    // TREEFROG_APP_EVENT_AFTER_VALUE_DEBUG_V1
    treefrog_app_debug_log("onEvent.enter",
                           (int)event.GetType(),
                           (int)event.GetValue(),
                           _mask,
                           _isDirty);

    TreeFrogInputTrace_LogView(
        "AppWindow.enter",
        _currentView ? (int)_currentView->GetViewType() : -1,
        (_currentView && _currentView->HasModal()) ? 1 : 0,
        (unsigned short)(event.GetValue() & 0xffffL),
        _mask,
        event.GetType() == ET_PADMASKDOWN ? 1 : 0,
        _audioShortcutLatched ? 1 : 0);

    MixerService *sm = MixerService::GetInstance();
    sm->Lock();

    switch (event.GetType()) {

    case ET_PADMASKDOWN: {
        /*
         * U2.51.0 ATOMIC_INPUT_MASK
         *
         * The event value is the complete logical LGPT mask.  Replace _mask
         * atomically instead of reconstructing it one button at a time.
         */
        const unsigned short chordOldMask = _mask;
        SynchronizeInputMask(
            (unsigned short)(event.GetValue() & 0xffffL));

        // F1b: los combos de abajo (helpCombo SELECT+R1, audioCombo
        // SELECT+R2) se resuelven contra el catalogo CTX_GLOBAL; el golden
        // los tenia como constantes bit a bit (AppWindow.cpp, Bacon 1.2.1).

        // TREEFROG_HELP_NAV_V15 (Bacon 1.1.1 V15): while a Help overlay is
        // open, L1/R1/L2/R2 presses always reach it for section navigation,
        // even when SELECT is still held from the chord that opened it.  The
        // atomic mask cannot tell "tap R1 for the next section" from
        // "SELECT+R1 again", so navigation wins; the overlay closes with B
        // (as its footer states).  Only the nav bits are forwarded so the
        // SELECT+R1 close chord inside the overlay can never misfire.
        if (_currentView) {
            ModalView *navModal = _currentView->GetModal();
            if (navModal && navModal->IsHelpOverlay()) {
                const unsigned short navPress =
                    _mask & (EPBM_L | EPBM_R | EPBM_L2 | EPBM_R2);
                if (navPress != 0) {
                    _currentView->ProcessButton(
                        navPress, true, event.When());
                    _isDirty = true;
                    break;
                }
            }
        }

        // TREEFROG_GLOBAL_UNDO_V6 (Bacon 1.1.1 V16): reliable L1+X / R1+X
        // chords.  Chaining undo into redo faster than the pad releases the
        // other shoulder yields a latched L|R|X poll; View::ProcessButton
        // rejects both chords there (L|X must not contain R, R|X must not
        // contain L) and redo silently dies.  Resolve using the previous
        // mask: the shoulder that just pressed wins (fresh R -> redo, fresh
        // L -> undo); if neither shoulder is fresh, the most recent shoulder
        // press wins; last resort is the direction of the last chord.  A
        // lone shoulder press is held one poll in case X joins it, so an
        // R1-first R1+X can never leak its R1 as a standalone press (Song
        // R1 = mute toggle) before the chord is recognized.
        if (_currentView) {
            const unsigned short newMask = _mask;
            if (newMask & EPBM_X) {
                const bool hasL = (newMask & EPBM_L) != 0;
                const bool hasR = (newMask & EPBM_R) != 0;
                if (hasL && hasR) {
                    const bool freshL =
                        hasL && ((chordOldMask & EPBM_L) == 0);
                    const bool freshR =
                        hasR && ((chordOldMask & EPBM_R) == 0);
                    unsigned short combo;
                    if (freshR && !freshL) {
                        combo = EPBM_R | EPBM_X;
                    } else if (freshL && !freshR) {
                        combo = EPBM_L | EPBM_X;
                    } else if (_lastShoulderPress == EPBM_L) {
                        combo = EPBM_L | EPBM_X;
                    } else if (_lastShoulderPress == EPBM_R) {
                        combo = EPBM_R | EPBM_X;
                    } else {
                        combo = _chordLastWasRedo
                                    ? (EPBM_R | EPBM_X)
                                    : (EPBM_L | EPBM_X);
                    }
                    _chordLastWasRedo =
                        (combo == (EPBM_R | EPBM_X));
                    _pendingShoulderPress = 0;
                    _currentView->ProcessButton(
                        combo, true, event.When());
                    _isDirty = true;
                    break;
                }
                if (hasL != hasR) {
                    // Unambiguous X+shoulder: record the direction for the
                    // fallbacks, drop any stale pending press, and let the
                    // normal delivery path below handle it.
                    _chordLastWasRedo = hasR;
                    _lastShoulderPress = hasL ? EPBM_L : EPBM_R;
                    _pendingShoulderPress = 0;
                }
            } else {
                // No X in this poll: release a pending lone shoulder press
                // unless it is now part of a SELECT+shoulder chord (help /
                // audio) or an L1+R1 combo, which the mask path delivers.
                if (_pendingShoulderPress != 0) {
                    const unsigned short pend = _pendingShoulderPress;
                    _pendingShoulderPress = 0;
                    if (!((newMask & EPBM_SELECT) &&
                          (newMask & pend))) {
                        _currentView->ProcessButton(
                            pend, true, event.When());
                        _isDirty = true;
                    }
                }
                const unsigned short shoulderOnly =
                    newMask & (EPBM_L | EPBM_R);
                if ((shoulderOnly == EPBM_L ||
                     shoulderOnly == EPBM_R) &&
                    (chordOldMask & shoulderOnly) == 0) {
                    // Fresh lone shoulder: wait one poll for X to join it.
                    _pendingShoulderPress = shoulderOnly;
                    _lastShoulderPress = shoulderOnly;
                    break;
                }
            }
        }

        // TREEFROG_HELP_OVERLAY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md):
        // SELECT+R1 opens the context help overlay (latched, non-modal
        // state change); SELECT+R2 keeps the Audio Driver dialog.
        // RC4 P1 (PLAN_RC4 section 11.3): Help may open over an already-open
        // dialog via PushModal, which suspends and later restores it.
        if (GlobalHelpChord(_mask) &&
            _currentView) {
            if (!_helpShortcutLatched) {
                _helpShortcutLatched = true;
                // TREEFROG_HELP_NAV_V14 (Bacon 1.1.1 V14): never stack a
                // second Help over an open one.
                ModalView *active = _currentView->GetModal();
                if (active && active->IsHelpOverlay()) {
                    _helpShortcutLatched = false;
                    break;
                }
                // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): when a modal
                // (the chopper) is open, show the Help for the modal that
                // actually has focus instead of the base view underneath.
                View *helpTarget = _currentView;
                ModalView *topModal = _currentView->GetModal();
                if (topModal) helpTarget = topModal;
                _currentView->PushModal(
                    new HelpOverlay(*helpTarget),
                    HelpOverlayApplyCallback);
                _isDirty = true;
            }
            break;
        }

        if (GlobalAudioChord(_mask) &&
            _currentView &&
            !_currentView->HasModal()) {
            if (!_audioShortcutLatched) {
                _audioShortcutLatched = true;
                _currentView->ReplaceModal(
                    new AudioDriverModal(*_currentView),
                    AudioDriverModalApplyCallback);
                _isDirty = true;
            }
            break;
        }

        // TREEFROG_HELP_NAV_V14 (Bacon 1.1.1 V14): the shortcut latches used
        // to swallow every press until BOTH chord buttons were released, so
        // L1/R1 section navigation inside an open Help was eaten whenever
        // SELECT stayed pressed (only the mixer felt like it worked).  The
        // latch now only blocks while its own chord is still held.
        if (_helpShortcutLatched || _audioShortcutLatched) {
            if (!GlobalHelpChord(_mask) &&
                !GlobalAudioChord(_mask)) {
                _helpShortcutLatched = false;
                _audioShortcutLatched = false;
            } else {
                break;
            }
        }

        if (_currentView)
            _currentView->ProcessButton(
                _mask,
                true,
                event.When());
        break;
    }

    case ET_PADMASKUP: {
        // TREEFROG_GLOBAL_UNDO_V6: a lone shoulder held one poll for a chord
        // was released without X -- deliver the standalone press now so the
        // view still gets it (Song R1 = mute toggle, ...), then the release.
        const unsigned short upMask =
            (unsigned short)(event.GetValue() & 0xffffL);
        if (_pendingShoulderPress != 0 &&
            (upMask & _pendingShoulderPress) == 0) {
            const unsigned short pend = _pendingShoulderPress;
            _pendingShoulderPress = 0;
            if (_currentView) {
                _currentView->ProcessButton(pend, true, event.When());
                _isDirty = true;
            }
        }
        SynchronizeInputMask(
            (unsigned short)(event.GetValue() & 0xffffL));

        if (_helpShortcutLatched) {
            if (_currentView)
                _currentView->ProcessButton(
                    _mask,
                    false,
                    event.When());
            if (GlobalHelpReleased(_mask)) {
                _helpShortcutLatched = false;
            }
            break;
        }

        if (_audioShortcutLatched) {
            if (_currentView)
                _currentView->ProcessButton(
                    _mask,
                    false,
                    event.When());

            if (GlobalAudioReleased(_mask)) {
                _audioShortcutLatched = false;
            }
            break;
        }

        if (_currentView)
            _currentView->ProcessButton(
                _mask,
                false,
                event.When());
        break;
    }

    case ET_PADBUTTONDOWN: {
        _mask |= v;
        /*
         * U2.42.0 DO_NOT_REPLACE_ACTIVE_MODAL:
         * keep the original global controls and Song combinations untouched.
         * The audio shortcut cannot replace a modal that is already open.
         * SELECT+R1 opens the help overlay with the same latch discipline.
         * RC4 P1 (PLAN_RC4 11.3): Help may open over an active dialog via
         * PushModal (suspend + restore) instead of replacing it.
         */
        // TREEFROG_HELP_NAV_V15: same L1/R1/L2/R2 forwarding as the mask
        // path while a Help overlay is open.
        if (_currentView) {
            ModalView *navModal = _currentView->GetModal();
            if (navModal && navModal->IsHelpOverlay()) {
                const unsigned short navPress =
                    _mask & (EPBM_L | EPBM_R | EPBM_L2 | EPBM_R2);
                if (navPress != 0) {
                    _currentView->ProcessButton(
                        navPress, true, event.When());
                    _isDirty = true;
                    break;
                }
            }
        }
        // TREEFROG_GLOBAL_UNDO_V6: per-button frontends see the same
        // latched L|R|X case; the button that just went down wins.
        if (_currentView && v != 0) {
            if ((v == EPBM_L || v == EPBM_R) &&
                (_mask & EPBM_X) &&
                (_mask & (EPBM_L | EPBM_R)) == (EPBM_L | EPBM_R)) {
                const bool isR = (v == EPBM_R);
                _chordLastWasRedo = isR;
                _lastShoulderPress = v;
                _currentView->ProcessButton(
                    isR ? (EPBM_R | EPBM_X) : (EPBM_L | EPBM_X),
                    true, event.When());
                _isDirty = true;
                break;
            }
            if (v == EPBM_X &&
                (_mask & (EPBM_L | EPBM_R)) == (EPBM_L | EPBM_R)) {
                _currentView->ProcessButton(
                    _chordLastWasRedo ? (EPBM_R | EPBM_X)
                                      : (EPBM_L | EPBM_X),
                    true, event.When());
                _isDirty = true;
                break;
            }
        }
        if (GlobalHelpChord(_mask) &&
            _currentView) {
            if (!_helpShortcutLatched) {
                _helpShortcutLatched = true;
                // TREEFROG_HELP_NAV_V14: never stack Help over Help.
                ModalView *active = _currentView->GetModal();
                if (active && active->IsHelpOverlay()) {
                    _helpShortcutLatched = false;
                    break;
                }
                // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): same modal
                // targeting as the mask path so Help over the chopper opens
                // on the chopper section, not the base view underneath.
                View *helpTarget = _currentView;
                ModalView *topModal = _currentView->GetModal();
                if (topModal) helpTarget = topModal;
                _currentView->PushModal(
                    new HelpOverlay(*helpTarget),
                    HelpOverlayApplyCallback);
                _isDirty = true;
            }
            break;
        }

        if (GlobalAudioChord(_mask) &&
            _currentView && !_currentView->HasModal()) {
            if (!_audioShortcutLatched) {
                _audioShortcutLatched = true;
                _currentView->ReplaceModal(
                    new AudioDriverModal(*_currentView),
                    AudioDriverModalApplyCallback);
                _isDirty = true;
            }
            break;
        }

        // TREEFROG_HELP_NAV_V14: same latch relaxation as the mask path.
        if (_helpShortcutLatched || _audioShortcutLatched) {
            if (!GlobalHelpChord(_mask) &&
                !GlobalAudioChord(_mask)) {
                _helpShortcutLatched = false;
                _audioShortcutLatched = false;
            } else {
                break;
            }
        }

        if (_currentView)
            _currentView->ProcessButton(_mask, true, event.When());
        break;
    }

    case ET_PADBUTTONUP: {
        _mask &= (0xFFFF - v);

        if (_helpShortcutLatched) {
            if (_currentView)
                _currentView->ProcessButton(_mask, false, event.When());
            if (GlobalHelpReleased(_mask)) {
                _helpShortcutLatched = false;
            }
            break;
        }

        if (_audioShortcutLatched) {
            /*
             * Deliver release masks to the newly opened modal so its input
             * barrier can arm only after SELECT and R2 are fully released.
             */
            if (_currentView)
                _currentView->ProcessButton(_mask, false, event.When());
            if (GlobalAudioReleased(_mask)) {
                _audioShortcutLatched = false;
            }
            break;
        }

        if (_currentView)
            _currentView->ProcessButton(_mask, false, event.When());
        break;
    }

    case ET_SYSQUIT:
        _shouldQuit = true;
        break;

        /*		case ET_KEYDOWN:
            if
           (event.GetValue()==EKT_ESCAPE&&!Player::GetInstance()->IsRunning()) {
                if (_currentView!=_listView) {
                    CloseProject() ;
                    _isDirty=true ;
                } else {
                    System::GetInstance()->PostQuitMessage() ;
                };
            } ;*/

    default:
        break;
    }
    // TREEFROG_APP_EVENT_AFTER_SWITCH_DEBUG_V1
    treefrog_app_debug_log("onEvent.afterSwitch",
                           (int)event.GetType(),
                           (int)event.GetValue(),
                           _mask,
                           _isDirty);

    sm->Unlock();

    TreeFrogInputTrace_LogView(
        "AppWindow.leave",
        _currentView ? (int)_currentView->GetViewType() : -1,
        (_currentView && _currentView->HasModal()) ? 1 : 0,
        (unsigned short)(event.GetValue() & 0xffffL),
        _mask,
        event.GetType() == ET_PADMASKDOWN ? 1 : 0,
        _audioShortcutLatched ? 1 : 0);

    if (_shouldQuit) {
        onQuitApp();
    }
    if (_closeProject) {
        CloseProject();
        _isDirty = true;
    }
    if (_loadAfterSaveAsProject) {
        CloseProject();
        _isDirty = true;
        LoadProject(_newProjectToLoad.c_str());
    }
#ifdef _SHOW_GP2X_
    Redraw();
#else
    // TREEFROG_APP_EVENT_BEFORE_REDRAW_DEBUG_V1
    treefrog_app_debug_log(_isDirty ? "onEvent.beforeRedraw.DIRTY" : "onEvent.beforeRedraw.NOT_DIRTY",
                           (int)event.GetType(),
                           (int)event.GetValue(),
                           _mask,
                           _isDirty);
    if (_isDirty)
        Redraw();
#endif
    return false;
};


void AppWindow::H35PollExternalExport() {
    /* H35_EXPORT_CORE_POLL_TMPFS_SESSION */
#if defined(PLATFORM_TREEFROG)
    enum { IDLE=0, RENDERING=1 };
    static int state=IDLE, command=0, session=0;
    static int oldSongX=0, oldSongY=0, oldSongOffset=0;
    static unsigned long long started=0;
    if (!_viewData || !_viewData->project_) return;

    if (state == IDLE) {
        unsigned format=1, sess=0, cmd=0;
        char line[256]={0};
        if (!h35ReadText(kH35ExportRequest, line, sizeof(line))) return;
        sscanf(line,"REQUEST command=%u format=%u session=%u",&cmd,&format,&sess);
        unlink(kH35ExportRequest); /* consume once; receiver waits on session file */
        command=(int)cmd; session=(int)sess;
        PersistencyService::GetInstance()->Save();
        sync();
        char ready[256], error[256], body[1024];
        snprintf(ready,sizeof(ready),"/tmp/r36sx_lgpt_usb/export_ready.%d",session);
        snprintf(error,sizeof(error),"/tmp/r36sx_lgpt_usb/export_error.%d",session);
        unlink(ready); unlink(error);
        if (command == 1) {
            /* Persist the current in-memory project before exporting its tree.
             * This is a bounded user-requested FAT32 write, not a live runtime log. */
            PersistencyService::GetInstance()->Save();
            sync();
            snprintf(body,sizeof(body),"ROOT=%s\nMODE=PROJECT\nFORMAT=%u\n",
                     _root.GetPath().c_str(),format);
            h35AtomicText(ready,body);
            if (_currentView) _currentView->SetNotification("Android project export ready");
            return;
        }
        if (command != 2 && command != 3) {
            h35AtomicText(error,"ERROR=UNSUPPORTED_COMMAND\n"); return;
        }
        Player *player=Player::GetInstance();
        if (player->IsRunning() || MixerService::GetInstance()->IsRendering()) {
            h35AtomicText(error,"ERROR=PLAYER_BUSY\n");
            if (_currentView) _currentView->SetNotification("Stop playback before Android export");
            return;
        }
        int mode=(command==2)?1:2;
        char stale[1024];
        if (command==2) {
            snprintf(stale,sizeof(stale),"%s/mixdown.wav",_root.GetPath().c_str());
            unlink(stale);
        } else {
            for (int i=0;i<8;i++) {
                snprintf(stale,sizeof(stale),"%s/channel%d.wav",_root.GetPath().c_str(),i);
                unlink(stale);
            }
        }
        Variable *render=_viewData->project_->FindVariable(VAR_RENDER);
        if (render) render->SetInt(mode,false);
        _viewData->renderMode_=mode;
        MixerService::GetInstance()->SetRenderMode(mode);
        oldSongX=_viewData->songX_; oldSongY=_viewData->songY_;
        oldSongOffset=_viewData->songOffset_;
        _viewData->songX_=0; _viewData->songY_=0; _viewData->songOffset_=0;
        player->Start(PM_SONG,false);
        started=h35ExportNowMs(); state=RENDERING;
        if (_currentView) _currentView->SetNotification(command==2 ? "Rendering mixdown export..." : "Rendering multitrack export...");
        return;
    }

    Player *player=Player::GetInstance();
    unsigned long long now=h35ExportNowMs();
    if (state==RENDERING && !player->IsRunning() && now-started>250) {
        char ready[256], error[256], body[1024], test[1024];
        snprintf(ready,sizeof(ready),"/tmp/r36sx_lgpt_usb/export_ready.%d",session);
        snprintf(error,sizeof(error),"/tmp/r36sx_lgpt_usb/export_error.%d",session);
        bool ok=true;
        if (command==2) { snprintf(test,sizeof(test),"%s/mixdown.wav",_root.GetPath().c_str()); ok=h35FileExists(test); }
        else { for(int i=0;i<8;i++){ snprintf(test,sizeof(test),"%s/channel%d.wav",_root.GetPath().c_str(),i); if(!h35FileExists(test)){ok=false;break;} } }
        if (ok) {
            snprintf(body,sizeof(body),"ROOT=%s\nMODE=%s\nFORMAT=WAV\n",
                     _root.GetPath().c_str(),command==2?"MIXDOWN":"STEMS");
            h35AtomicText(ready,body);
            if (_currentView) _currentView->SetNotification(command==2 ? "Mixdown export ready" : "Multitrack export ready");
        } else {
            h35AtomicText(error,"ERROR=RENDER_OUTPUT_MISSING\n");
            if (_currentView) _currentView->SetNotification("Export render failed or song is empty");
        }
        Variable *render=_viewData->project_->FindVariable(VAR_RENDER);
        if (render) render->SetInt(0,false);
        _viewData->renderMode_=0; MixerService::GetInstance()->SetRenderMode(0);
        _viewData->songX_=oldSongX; _viewData->songY_=oldSongY; _viewData->songOffset_=oldSongOffset;
        state=IDLE; command=0; session=0;
    } else if (state==RENDERING && now-started>1200000ULL) {
        player->Stop();
        char error[256]; snprintf(error,sizeof(error),"/tmp/r36sx_lgpt_usb/export_error.%d",session);
        h35AtomicText(error,"ERROR=RENDER_TIMEOUT\n");
        MixerService::GetInstance()->SetRenderMode(0); state=IDLE;
        if (_currentView) _currentView->SetNotification("Android export timed out");
    }
#endif
}

void AppWindow::onUpdate() {
    H35PollExternalExport();
    if (_loadAfterResume) {
        _loadAfterResume = false;
        _isDirty = true;
        LoadProject(_newProjectToLoad.c_str());
        return;
    }

    /*
     * U2.44.0 ACTIVE_MODAL_FRAME_TICK
     *
     * TreeFrog calls GUIWindow::Update once per retro frame. Forward that
     * cadence only to the active modal. This is independent of Player
     * transport, so the sampler IN meter keeps moving while playback is
     * stopped and its private input FSM can read raw source consensus.
     */
    if (_currentView) {
        _currentView->UpdateActiveModalFrame(
            (unsigned long)System::GetInstance()->GetClock());
        // TREEFROG_MIXER_LIVE_VU_V2 (H38.6): forward the same frame cadence
        // to the active view so live meters (Mixer VU) keep moving even while
        // the player is stopped, mirroring the USB-C record meter behavior.
        _currentView->OnFrameUpdate(
            (unsigned long)System::GetInstance()->GetClock());
    }

    if (_isDirty) {
        Redraw();
        _isDirty = false;
    }

    Flush();
};

void AppWindow::LayoutChildren() {};

void AppWindow::Update(Observable &o, I_ObservableData *d) {

    ViewEvent *ve = (ViewEvent *)d;

    switch (ve->GetType()) {

    case VET_SWITCH_VIEW: {
        ViewType *vt = (ViewType *)ve->GetData();
        if (_currentView) {
            _currentView->LooseFocus();
        }
        switch (*vt) {
        case VT_SONG:
            _currentView = _songView;
            break;
        case VT_CHAIN:
            _currentView = _chainView;
            break;
        case VT_PHRASE:
            _currentView = _phraseView;
            break;
        case VT_PROJECT:
            _currentView = _projectView;
            break;
        case VT_INSTRUMENT:
            _currentView = _instrumentView;
            break;
        case VT_TABLE:
            _currentView = _tableView;
            break;
        case VT_TABLE2:
            _currentView = _tableView;
            break;
        case VT_GROOVE:
            _currentView = _grooveView;
            break;
        case VT_MIXER:
            _currentView = _mixerView;
            break;
        }
        _currentView->SetFocus(*vt);
        _isDirty = true;
        GUIWindow::Clear(backgroundColor_, true);
        Clear(true);
        Redraw();
        break;
    }

    case VET_PLAYER_POSITION_UPDATE: {
        PlayerEvent *pt = (PlayerEvent *)ve;

        if (_currentView) {
            SysMutexLocker locker(drawMutex_);
            _currentView->OnPlayerUpdate(pt->GetType(), pt->GetTickCount());
            Invalidate();
        }

        break;
    }

        /*	  case VET_LIST_SELECT:
              {
                char *name=(char*)ve->GetData() ;
                LoadProject(name) ;
                break ;
              } */

    case VET_SAVEAS_PROJECT: {
        char *name = (char *)ve->GetData();
        _loadAfterSaveAsProject = true;
        _newProjectToLoad = name;
        break;
    }

    case VET_QUIT_PROJECT: {
        // defer event to after we got out of the view
        _closeProject = true;
        break;
    }
    case VET_QUIT_APP:
        _shouldQuit = true;
        break;
    }
}

void AppWindow::onQuitApp() {
    Player *player = Player::GetInstance();
    player->Stop();
    player->RemoveObserver(*this);

    /* H35: a normal user exit is an explicit persistence boundary. Flush the
     * project and FAT metadata before picoarch is terminated by the launcher.
     * Runtime logs and state remain in tmpfs. */
    PersistencyService::GetInstance()->Save();
    sync();
    usleep(200000);

    player->Reset();
    System::GetInstance()->PostQuitMessage();
}
void AppWindow::Print(char *line) {

    //	GUIWindow::Clear(View::backgroundColor_,true) ;
    Clear();
    const char *safeLine=line?line:"" ;
    snprintf(_statusLine,sizeof(_statusLine),"%.40s",safeLine) ;
    // The TreeFrog text surface is 40 columns wide.
    int position = (40-(int)strlen(_statusLine))/2 ;
    if (position<0) position=0 ;
    GUIPoint pos(position, 12);
    //
    GUITextProperties props;
    SetColor(CD_NORMAL);
    DrawString(_statusLine, pos, props);
    char buildString[80];
    snprintf(buildString,sizeof(buildString),"Piggy build %s.%s.%s",PROJECT_NUMBER,
             PROJECT_RELEASE,BUILD_COUNT);
    pos._y = 28;
    pos._x = (40 - strlen(buildString)) / 2;
    DrawString(buildString, pos, props);
    Flush();
};

void AppWindow::SetColor(ColorDefinition cd) { colorIndex_ = cd; };

// TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1): maps a ColorDefinition to
// the RGB565 the character screen would render it with (same switch as
// Flush(), same palette statics, 565 conversion identical to
// TreeFrogGUIWindowImp::rgb565).
unsigned short AppWindow::ResolveColor565(ColorDefinition cd) const {
    GUIColor gcolor = normalColor_;
    switch (cd) {
    case CD_BACKGROUND: gcolor = backgroundColor_; break;
    case CD_NORMAL: break;
    case CD_BORDER: gcolor = borderColor_; break;
    case CD_HILITE1: gcolor = highlightColor_; break;
    case CD_HILITE2: gcolor = highlight2Color_; break;
    case CD_CONSOLE: gcolor = consoleColor_; break;
    case CD_CURSOR: gcolor = cursorColor_; break;
    case CD_PLAY: gcolor = playColor_; break;
    case CD_RECORD: gcolor = recordColor_; break;
    case CD_MUTE: gcolor = muteColor_; break;
    case CD_SONGVIEWFE: gcolor = songviewfeColor_; break;
    case CD_SONGVIEW00: gcolor = songview00Color_; break;
    case CD_ROW: gcolor = rownumberColor_; break;
    case CD_ROW2: gcolor = rownumber2Color_; break;
    case CD_MAJORBEAT: gcolor = majorbeatColor_; break;
    case CD_WARNING: gcolor = warningColor_; break;
    case CD_ERROR: gcolor = errorColor_; break;
    default: break;
    }
    unsigned short r = (unsigned short)((gcolor._r & 0xff) >> 3);
    unsigned short g = (unsigned short)((gcolor._g & 0xff) >> 2);
    unsigned short b = (unsigned short)((gcolor._b & 0xff) >> 3);
    return (unsigned short)((r << 11) | (g << 5) | b);
};

Path AppWindow::GetLastProjectPath() {
    Path lastProjectFile(LAST_PROJECT_NAME);
    FileSystem *fs = FileSystem::GetInstance();
    I_File *file = fs->Open(lastProjectFile.GetPath().c_str(), "r");

    if (!file) {
        return Path();
    }

    // Get file size
    file->Seek(0, SEEK_END);
    int length = file->Tell();
    
    if (length <= 0) {
        file->Close();
        delete file;
        return Path();
    }

    // Allocate buffer and seek back to start
    char *buffer = (char *)SYS_MALLOC(length + 1);
    memset(buffer, 0, length + 1);

    file->Seek(0, SEEK_SET); // Seek back to start
    int bytes = file->Read(buffer, 1, length); // Read full length
    file->Close();
    delete file;

    if (bytes <= 0) {
        Trace::Error("GetLastProject: Failed to read last project file");
        SYS_FREE(buffer);
        return Path();
    }

    buffer[bytes] = 0; // Null terminate

    // Remove newline if present
    int len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = 0;
    }

    Path result;
    if (strlen(buffer) > 0) {
        if (strstr(buffer, "lgpt_") != NULL) { // Ensure it's an lgpt project
            result = Path(buffer);
        } else {
            Trace::Error("GetLastProject: Invalid project path format: %s",
                         buffer);
        }
    }
    if (!result.IsDirectory()) {
        Trace::Error("GetLastProject: path does not exist: %s", result.GetPath().c_str());
    }

    SYS_FREE(buffer);
    return result;
}

void AppWindow::SaveLastProject(const Path &p) {
    Path lastProjectFile(LAST_PROJECT_NAME);
    FileSystem *fs = FileSystem::GetInstance();
    I_File *file = fs->Open(lastProjectFile.GetPath().c_str(), "w");

    if (!file) {
        Trace::Error("SaveLastProject: Failed to open %s for writing",
                     LAST_PROJECT_NAME);
        return;
    }

    std::string pathStr = p.GetPath();
    file->Write(pathStr.c_str(), 1, pathStr.length());
    file->Write("\n", 1, 1);
    file->Close();
    delete file;

    Trace::Log("SaveLastProject", "Saved last project: %s",
               p.GetPath().c_str());
}
