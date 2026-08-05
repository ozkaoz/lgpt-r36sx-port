
#ifndef _APP_WINDOW_H_
#define _APP_WINDOW_H_

#include "Application/Views/ChainView.h"
#include "Application/Views/ConsoleView.h"
#include "Application/Views/GrooveView.h"
#include "Application/Views/InstrumentView.h"
#include "Application/Views/MixerView.h"
#include "Application/Views/NullView.h"
#include "Application/Views/PhraseView.h"
#include "Application/Views/ProjectView.h"
#include "Application/Views/SongView.h"
#include "Application/Views/TableView.h"
#include "Application/Views/ViewData.h"
#include "Foundation/Observable.h"
#include "System/Process/SysMutex.h"
#include "System/io/Status.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"

#define PROP_INVERT 0x80

class AppWindow : public GUIWindow, I_Observer, Status {
  protected:
    AppWindow(I_GUIWindowImp &imp);
    virtual ~AppWindow();

  public:
    static AppWindow *Create(GUICreateWindowParams &);
    // TREEFROG_PROJECT_RENAME_V1 (H38.5): singleton accessor used by
    // ProjectView to pre-fill the rename dialog with the current name.
    static AppWindow *GetInstance();
    void LoadProject(const Path &path);
    void SaveLastProject(const Path &p);
    Path GetLastProjectPath();
    void CloseProject();
    // TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): arms a render export request
    // for the project that is about to be loaded. mode is 1 = mixdown
    // (master), 2 = stems (multitrack). The render state machine in
    // H35PollExternalExport consumes the request once the project is loaded.
    void RequestExportRender(int mode);

    virtual void Clear(bool all = false);
    virtual void ClearRect(GUIRect &rect);
    virtual void SetColor(ColorDefinition cd);
    // TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1): resolves a palette
    // ColorDefinition to its current RGB565 value (pixel-level layers such
    // as the L/R half-cell mixer bars draw with the exact same colors as
    // the character screen).
    unsigned short ResolveColor565(ColorDefinition cd) const;
    void SetDirty();
    void SynchronizeInputMask(unsigned short mask);

  protected: // GUIWindow implementation
    virtual bool onEvent(GUIEvent &event);
    virtual void onUpdate();
    virtual void LayoutChildren();
    virtual void Flush();
    virtual void Redraw();

    // override draw string to avoid going too far off
    // the screen.
    virtual void DrawString(const char *string, GUIPoint &pos,
                            GUITextProperties &props, bool overlay = false);

    // I_Observer implementation

    virtual void Update(Observable &o, I_ObservableData *d);

    // Status implementation

    virtual void Print(char *);

    void defineColor(const char *colorName, GUIColor &color);

    void onQuitApp();
    void H35PollExternalExport();

  private:
    View *_currentView;
    ViewData *_viewData;
    SongView *_songView;
    ChainView *_chainView;
    PhraseView *_phraseView;
    ProjectView *_projectView;
    InstrumentView *_instrumentView;
    TableView *_tableView;
    GrooveView *_grooveView;
    NullView *_nullView;
    MixerView *_mixerView;

    Path _root;

    bool _isDirty;
    bool _closeProject;
    bool _loadAfterSaveAsProject;
    bool _loadAfterResume;
    bool _shouldQuit;
    unsigned short _mask;
    bool _audioShortcutLatched;
    bool _helpShortcutLatched;
    // TREEFROG_GLOBAL_UNDO_V6 (Bacon 1.1.1 V16): chord-state for L1+X / R1+X.
    // _pendingShoulderPress holds a lone shoulder press one poll while we wait
    // for X to join it; _lastShoulderPress / _chordLastWasRedo disambiguate a
    // latched L|R|X mask (undo chained into redo before L1 fully released).
    unsigned short _pendingShoulderPress;
    unsigned short _lastShoulderPress;
    bool _chordLastWasRedo;
    unsigned long _lastA;
    unsigned long _lastB;
    char _statusLine[80];
    std::string _newProjectToLoad;
    unsigned char _charScreen[1200];
    unsigned char _charScreenProp[1200];
    unsigned char _preScreen[1200];
    unsigned char _preScreenProp[1200];

    static GUIColor backgroundColor_;
    static GUIColor normalColor_;
    static GUIColor borderColor_;
    static GUIColor songviewfeColor_;
    static GUIColor songview00Color_;
    static GUIColor highlight2Color_;
    static GUIColor highlightColor_;
    static GUIColor consoleColor_;
    static GUIColor cursorColor_;
    static GUIColor playColor_;
    static GUIColor recordColor_;
    static AppWindow *instance_;
    static GUIColor muteColor_;
    static GUIColor rownumberColor_;
    static GUIColor rownumber2Color_;
    static GUIColor majorbeatColor_;
    static GUIColor warningColor_;
    static GUIColor errorColor_;
#define LAST_PROJECT_NAME "bin:last_project"

    ColorDefinition colorIndex_;

    static int charWidth_;
    static int charHeight_;

    SysMutex drawMutex_;

};

#endif
