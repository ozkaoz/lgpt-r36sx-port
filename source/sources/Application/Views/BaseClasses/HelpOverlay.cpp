#include "HelpOverlay.h"
#include "HelpRegistry.h"

#include <stdio.h>

/*
 * TREEFROG_HELP_OVERLAY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 13).
 *
 * Rendering of the context-help overlay.  The window is framed by
 * ModalView::SetWindow, titled with the context name, and lists the key /
 * action lines for the active view.  It never prevents playback and never
 * forwards input while latched.
 */

HelpOverlay::HelpOverlay(View &view)
    : ModalView(view), helpViewType_(view.GetViewType()) {}

HelpOverlay::~HelpOverlay() {}

void HelpOverlay::DrawView() {
    // 36 wide, 11 tall keeps every row inside the 40x30 safe area.
    SetWindow(36, 11);

    const HelpSection *section = HelpRegistry::GetSection(helpViewType_);

    GUITextProperties props;
    char title[48];
    snprintf(title, sizeof(title), "HELP - %s",
             section ? section->title : "SONG");
    // Title bar (top of the overlay hierarchy).
    SetColor(CD_HILITE2);
    props.invert_ = false;
    DrawString(0, 0, title, props);

    SetColor(CD_NORMAL);
    props.invert_ = false;

    if (section) {
        int n = HelpRegistry::GetLineCount(section);
        if (n > 8) n = 8;
        for (int i = 0; i < n; i++) {
            char line[40];
            snprintf(line, sizeof(line), "%s  %s", section->lines[i].keys,
                     section->lines[i].action);
            DrawString(0, 2 + i, line, props);
        }
    }
    SetColor(CD_NORMAL);
    DrawString(0, 10, "Release SELECT+R1 to close", props);
}

void HelpOverlay::ProcessButtonMask(unsigned short mask, bool pressed) {
    // The overlay is purely informational: it closes when the originating
    // chord is released.  All button traffic while open is ignored so it
    // can never change state underneath the user.
    (void)mask;
    (void)pressed;
}

void HelpOverlay::OnPlayerUpdate(PlayerEventType, unsigned int) {}
void HelpOverlay::OnFocus() { isDirty_ = true; }

void HelpOverlayApplyCallback(View &view, ModalView &dialog) {
    (void)view;
    (void)dialog;
}