#include "AppWindow.h"
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Commands/EventDispatcher.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Player/TablePlayback.h"
#include "Application/Utils/char.h"
#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"
#include "Application/Views/ModalDialogs/MessageBox.h"
#include "Application/Views/ModalDialogs/SelectProjectDialog.h"
#include "Foundation/Variables/WatchedVariable.h"
#include "Foundation/Variables/Variable.h"
#include "Player/Player.h"
#include "Services/Midi/MidiService.h"
#include "Services/Time/TimeService.h"
#include "System/Console/Trace.h"
#include "UIFramework/Interfaces/I_GUIWindowFactory.h"
#include "Views/UIController.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#if defined(PLATFORM_TREEFROG)
extern "C" void TreeFrogChopperOverlayDraw(void);
#endif


// TREEFROG_APP_EVENT_DEBUG_HELPERS_V1
static void treefrog_app_debug_log(const char *where,
                                   int eventType,
                                   int eventValue,
                                   unsigned short mask,
                                   bool dirty) {
#if TREEFROG_INPUT_DEBUG
    FILE *f = fopen("/mnt/sdcard/lgpt/app_debug.log", "a");
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


AppWindow *instance = 0;

GUIColor AppWindow::backgroundColor_(0x1D, 0x0A, 0x1F);
GUIColor AppWindow::normalColor_(0xF5, 0xEB, 0xFF);
GUIColor AppWindow::borderColor_(0xFF, 0x00, 0x8C);
GUIColor AppWindow::songviewfeColor_(0xA5, 0x5B, 0x8F);
GUIColor AppWindow::songview00Color_(0x85, 0x3B, 0x6F);
GUIColor AppWindow::highlightColor_(0xB7, 0x50, 0xD1);
GUIColor AppWindow::highlight2Color_(0xDB, 0x33, 0xDB);
GUIColor AppWindow::consoleColor_(0x00, 0xFF, 0x00);
GUIColor AppWindow::cursorColor_(0xFF, 0x00, 0x8C);
GUIColor AppWindow::playColor_(0xFF, 0x00, 0x8C);
GUIColor AppWindow::muteColor_(0xF5, 0xEB, 0xFF);
GUIColor AppWindow::rownumberColor_(0xBA, 0x28, 0xF9);
GUIColor AppWindow::rownumber2Color_(0xFF, 0x00, 0xFF);
GUIColor AppWindow::majorbeatColor_(0xBA, 0x28, 0xF9);

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
    defineColor("MUTECOLOR", muteColor_);
    defineColor("ROWCOLOR1", rownumberColor_);
    defineColor("ROWCOLOR2", rownumber2Color_);
    defineColor("MAJORBEAT", majorbeatColor_);

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


bool AppWindow::SetProjectMasterFromPhysicalVolume(int value, int *outValue) {
#if defined(PLATFORM_TREEFROG)
    if (!_viewData || !_viewData->project_) return false;
    Variable *v = _viewData->project_->FindVariable(VAR_MASTERVOL);
    if (!v) return false;
    int next = value;
    if (next < 10) next = 10;
    if (next > 100) next = 100;
    v->SetInt(next);
    MixerService::GetInstance()->SetMasterVolume(next);
    TreeFrogUac2Bridge_SetProjectMasterVolumePercent(next);
    /* AU9V: physical buttons drive Project > Master. Do not also set mixer here,
       otherwise USB would be attenuated twice by master*mixer. */
    if (outValue) *outValue = next;
    SetDirty();
    return true;
#else
    (void)value; if (outValue) *outValue = 0; return false;
#endif
}

bool AppWindow::AdjustProjectMasterFromPhysicalVolume(int delta, int *outValue) {
#if defined(PLATFORM_TREEFROG)
    if (!_viewData || !_viewData->project_) return false;
    Variable *v = _viewData->project_->FindVariable(VAR_MASTERVOL);
    if (!v) return false;
    return SetProjectMasterFromPhysicalVolume(v->GetInt() + delta, outValue);
#else
    (void)delta; if (outValue) *outValue = 0; return false;
#endif
}


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
        DrawExportOverlay();
        Invalidate();
    }
};

void AppWindow::DrawExportOverlay() {
    Player *player = Player::GetInstance();
    if (!player || !player->IsExportingSong()) return;

    GUITextProperties props;
    props.invert_ = true;
    SetColor(CD_HILITE1);

    char msg[64];
    int percent = player->GetExportProgressPercent();
    const char *label = (player->GetExportRenderMode() == 2) ? "Exporting multitrack" : "Exporting song WAV";
    GUIPoint pos;
    const int x = 3;
    const int y = 10;

    pos._x = x; pos._y = y + 0; DrawString("+--------------------------------+", pos, props);
    snprintf(msg, sizeof(msg), "| %-30.30s |", label);
    pos._x = x; pos._y = y + 1; DrawString(msg, pos, props);
    snprintf(msg, sizeof(msg), "|              %3d%%              |", percent);
    msg[34] = 0;
    pos._x = x; pos._y = y + 2; DrawString(msg, pos, props);
    pos._x = x; pos._y = y + 3; DrawString("|          Please wait           |", pos, props);
    pos._x = x; pos._y = y + 4; DrawString("|       Writing WAV files        |", pos, props);
    pos._x = x; pos._y = y + 5; DrawString("| START cancels export           |", pos, props);
    pos._x = x; pos._y = y + 6; DrawString("+--------------------------------+", pos, props);
}

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
#endif
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
    if (!succeeded) {
        PersistencyService::GetInstance()->Save();
        Trace::Log("TREEFROG_V40_ESTABLE", "autosaved initial project: %s", _root.GetPath().c_str());
        FILE *fp = fopen("/mnt/sdcard/lgpt/reentry_debug.log", "a");
        if (fp) {
            fprintf(fp, "TREEFROG_V40_ESTABLE_APPWINDOW: autosaved_initial_project path=%s\n", _root.GetPath().c_str());
            fclose(fp);
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
    return w;
};

void AppWindow::SetDirty() { _isDirty = true; };

bool AppWindow::onEvent(GUIEvent &event) {

    // We need to tell the app to quit once we're out of the
    // mixer lock, otherwise the windows driver will never return

    _shouldQuit = false;

    _isDirty = false;

    unsigned short v = 1 << event.GetValue();

    // TREEFROG_APP_EVENT_AFTER_VALUE_DEBUG_V1
    treefrog_app_debug_log("onEvent.enter",
                           (int)event.GetType(),
                           (int)event.GetValue(),
                           _mask,
                           _isDirty);

    MixerService *sm = MixerService::GetInstance();
    sm->Lock();

    switch (event.GetType()) {

    case ET_PADBUTTONDOWN:

        _mask |= v;
        if (_currentView)
            _currentView->ProcessButton(_mask, true);
        break;

    case ET_PADBUTTONUP:

        _mask &= (0xFFFF - v);
        if (_currentView)
            _currentView->ProcessButton(_mask, false);
        break;

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

void AppWindow::onUpdate() {
#if defined(PLATFORM_TREEFROG)
    static unsigned au10y_rec_tick = 0;
    au10y_rec_tick++;
    if ((au10y_rec_tick % 1) == 0 && access("/tmp/r36sx_au10y_usb_rec_modal", F_OK) == 0) {
        _isDirty = true;
        if (_currentView) _currentView->SetDirty(true);
        Redraw();
    }
#endif
    if (_loadAfterResume) {
        _loadAfterResume = false;
        _isDirty = true;
        LoadProject(_newProjectToLoad.c_str());
        return;
    }

    Player *player = Player::GetInstance();
    if (player && player->IsExportingSong()) {
        player->ContinueSongExport(4);
        if (_viewData) _viewData->isRendering_ = player->IsExportingSong();
        if (!player->IsExportingSong()) {
            if (_currentView) _currentView->SetNotification(player->LastExportSucceeded() ? "WAV export written" : "WAV export failed; see log");
        }
        _isDirty = true;
        Redraw();
    }

    Flush();
};

void AppWindow::LayoutChildren() {};

void AppWindow::Update(Observable &o, I_ObservableData *d) {

    ViewEvent *ve = (ViewEvent *)d;

    switch (ve->GetType()) {

    case VET_SWITCH_VIEW: {
        ViewType *vt = (ViewType *)ve->GetData();
        /* AU11M_ALLOW_INSTRUMENT_VIEW_WHILE_PLAYING
           Navigation to Instrument must not stop the project.
           Only modal/destructive sample actions are guarded. */
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
            _currentView->PlayerUpdate(pt->GetType(), pt->GetTickCount());
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

    player->Reset();
    System::GetInstance()->PostQuitMessage();
}
void AppWindow::Print(char *line) {

    //	GUIWindow::Clear(View::backgroundColor_,true) ;
    Clear();
    strcpy(_statusLine, line);
    // unwrapped for gcc
    int position = 40;
    position -= strlen(_statusLine);
    position /= 2;
    GUIPoint pos(position, 12);
    //
    GUITextProperties props;
    SetColor(CD_NORMAL);
    DrawString(_statusLine, pos, props);
    char buildString[80];
    sprintf(buildString, "Piggy build %s.%s.%s", PROJECT_NUMBER,
            PROJECT_RELEASE, BUILD_COUNT);
    pos._y = 28;
    pos._x = (40 - strlen(buildString)) / 2;
    DrawString(buildString, pos, props);
    Flush();
};

void AppWindow::SetColor(ColorDefinition cd) { colorIndex_ = cd; };

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
