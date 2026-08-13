/*
 * help_overlay_host_stubs.cpp -- F2: stubs de link para el harness host
 * del HelpOverlay. Solo aportan CUERPOS para los simbolos que tiran los
 * .cpp reales (View/ModalView/HelpOverlay/HelpRegistry/UiDraw); no se
 * enlaza nada de la app real.
 */
#include "Application/Model/Config.h"
#include "Application/Player/Player.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include "Application/AppWindow.h"

/* --- AppWindow: lo unico que usa View.cpp via (AppWindow &)w_ --- */
AppWindow::AppWindow(I_GUIWindowImp &imp) : GUIWindow(imp) {}
AppWindow::~AppWindow() {}
void AppWindow::Clear(bool all) { (void)all; }
void AppWindow::ClearRect(GUIRect &rect) { (void)rect; }
void AppWindow::SetColor(ColorDefinition cd) { (void)cd; }
void AppWindow::SetDirty() {}
void AppWindow::onUpdate() {}
void AppWindow::LayoutChildren() {}
void AppWindow::Flush() {}
bool AppWindow::onEvent(GUIEvent &) { return false; }
void AppWindow::Redraw() {}
void AppWindow::DrawString(const char *, GUIPoint &, GUITextProperties &, bool) {}
void AppWindow::Update(Observable &o, I_ObservableData *d) {
    (void)o;
    (void)d;
}
void AppWindow::Print(char *) {}

/* --- GUIWindow: cuerpos que piden View/ModalView/HelpOverlay --- */
GUIWindow::GUIWindow(I_GUIWindowImp &imp) : _imp(&imp) {}
GUIWindow::~GUIWindow() {}

GUIRect GUIWindow::GetRect() { return GUIRect(0, 0, 40, 30); }

void GUIWindow::Clear(GUIColor &, bool) {}
void GUIWindow::ClearRect(GUIRect &) {}
void GUIWindow::SetColor(GUIColor &) {}
void GUIWindow::DrawChar(const char, GUIPoint &, GUITextProperties &) {}
void GUIWindow::DrawString(const char *, GUIPoint &, GUITextProperties &,
                           bool) {}
void GUIWindow::Invalidate() {}
void GUIWindow::Flush() {}
void GUIWindow::Lock() {}
void GUIWindow::Unlock() {}
void GUIWindow::Update() {}
bool GUIWindow::DispatchEvent(GUIEvent &) { return false; }
I_GUIGraphics *GUIWindow::GetDC() { return 0; }
I_GUIGraphics *GUIWindow::GetGraphics() { return 0; }

GUIRect::GUIRect(long x0, long y0, long x1, long y1)
    : _topLeft(GUIPoint(x0, y0)), _bottomRight(GUIPoint(x1, y1)) {}

/* --- Config: View ctor lee ALTROWNUMBER (GetInstance vive en
 * T_Singleton; solo el GetValue es de la app). --- */
Config::Config() {}
Config::~Config() {}
const char *Config::GetValue(const char *key) {
    (void)key;
    return 0;
}

/* --- Observable: View hereda de el. --- */
Observable::Observable() {}
Observable::~Observable() {}

/* --- Player: drawNotes los referencia; la parada virtual Update
 * es la key function de su vtable. --- */
Player::Player() {}
void Player::Update(Observable &o, I_ObservableData *d) {
    (void)o;
    (void)d;
}
Player *Player::GetInstance() {
    static Player instance;
    return &instance;
}

/* --- Path / SysMutex / VariableContainer / Variable: bases y
 * miembros del layout de Config y Player. --- */
Path::Path() {}
Path::Path(const char *) {}
Path::Path(const std::string &) {}
Path::Path(const Path &) {}
Path::~Path() {}
Path &Path::operator=(const Path &other) { return *this; }

SysMutex::SysMutex() : mutex_(0) {}
SysMutex::~SysMutex() {}

VariableContainer::VariableContainer() {}
VariableContainer::~VariableContainer() {}

Variable::~Variable() {}
bool Player::IsRunning() { return false; }
char *Player::GetPlayedNote(int) {
    static char n[3] = "..";
    return n;
}
char *Player::GetPlayedOctive(int) {
    static char n[3] = "..";
    return n;
}
char *Player::GetPlayedInstrument(int) {
    static char n[3] = "..";
    return n;
}