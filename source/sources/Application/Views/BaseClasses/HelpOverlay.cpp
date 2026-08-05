#include "HelpOverlay.h"
#include "HelpRegistry.h"
#include "UiDraw.h"

#include <stdio.h>
#include <string.h>

/*
 * TREEFROG_HELP_OVERLAY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 13)
 * + RC4 P0/P1 (PLAN_RC4 sections 11.1/11.2).
 *
 * Navigable context-help overlay.  The window is framed by ModalView::SetWindow
 * and lists the key / action lines for the active view, letting the user
 * browse the whole registry before closing.  It never prevents playback.
 *
 * RC4: the overlay is a real modal and closes deterministically with B or with
 * SELECT+R1 again; every event reaching it is consumed so nothing propagates to
 * the view underneath.  It must call EndModal() (the RC3 version ignored all
 * input and never closed, locking the app once the opening chord was released).
 */

HelpOverlay::HelpOverlay(View &view)
    : ModalView(view),
      helpViewType_(view.GetViewType()),
      sectionIndex_(0),
      lineScroll_(0),
      showIndex_(false) {
    // The browser opens on the section of the view that invoked it.
    StartAt(helpViewType_);
}

HelpOverlay::~HelpOverlay() {}

void HelpOverlay::StartAt(ViewType vt) {
    const HelpSection *sec = HelpRegistry::GetSection(vt);
    int n = HelpRegistry::GetSectionCount();
    sectionIndex_ = 0;
    for (int i = 0; i < n; i++) {
        if (HelpRegistry::GetSectionAt(i) == sec) {
            sectionIndex_ = i;
            break;
        }
    }
    lineScroll_ = 0;
    showIndex_ = false;
    ClampCursor();
};

void HelpOverlay::ClampCursor() {
    int n = HelpRegistry::GetSectionCount();
    if (sectionIndex_ < 0) sectionIndex_ = 0;
    if (sectionIndex_ >= n) sectionIndex_ = n - 1;
    const HelpSection *s = HelpRegistry::GetSectionAt(sectionIndex_);
    if (s) {
        int max = s->lineCount - kMaxWindowLines_;
        if (max < 0) max = 0;
        if (lineScroll_ < 0) lineScroll_ = 0;
        if (lineScroll_ > max) lineScroll_ = max;
    } else {
        lineScroll_ = 0;
    }
}

void HelpOverlay::DrawView() {
    // 36 wide, 11 tall keeps every row inside the 40x30 safe area.
    SetWindow(36, 11);

    const HelpSection *section =
        HelpRegistry::GetSectionAt(sectionIndex_);

    GUITextProperties props;
    char title[48];
    if (showIndex_) {
        snprintf(title, sizeof(title), "HELP REGISTRY");
    } else {
        snprintf(title, sizeof(title), "HELP - %s",
                 section ? section->title : "?");
    }
    // Title bar (top of the overlay hierarchy).
    SetColor(CD_HILITE2);
    props.invert_ = false;
    DrawString(0, 0, title, props);

    SetColor(CD_NORMAL);
    props.invert_ = false;

    if (showIndex_) {
        // Section index: show each registered section name with a highlight
        // on the current one.
        int n = HelpRegistry::GetSectionCount();
        char line[40];
        for (int i = 0; i < n && i < kMaxWindowLines_; i++) {
            const HelpSection *s = HelpRegistry::GetSectionAt(i);
            if (i == sectionIndex_) {
                snprintf(line, sizeof(line), "%s", s ? s->title : "?");
                UiDraw::DrawSelectionRegion(*this, 0, 2 + i, (int)strlen(line), 1);
                DrawString(0, 2 + i, line, props);
            } else {
                props.invert_ = false;
                snprintf(line, sizeof(line), "%s", s ? s->title : "?");
                DrawString(0, 2 + i, line, props);
            }
        }
        props.invert_ = false;
    } else if (section) {
        int total = section->lineCount;
        int shown = total - lineScroll_;
        if (shown > kMaxWindowLines_) shown = kMaxWindowLines_;
        for (int i = 0; i < shown; i++) {
            int li = lineScroll_ + i;
            if (li < 0 || li >= total) break;
            char line[48];
            snprintf(line, sizeof(line), "%s  %s",
                     section->lines[li].keys,
                     section->lines[li].action);
            DrawString(0, 2 + i, line, props);
        }
    }
    SetColor(CD_NORMAL);
    // TREEFROG_HELP_NAV_V1 (Bacon 1.1.1 V13): L1/R1 move between help
    // sections (same keys as the chopper submenus), L2/R2 first/last.
    DrawString(0, 10, "B close  UP/DN  L1/R1 tabs", props);
    // RC4 P6 (PLAN_RC4 11.7): proportional scroll indicator (right edge)
    // when the section overflows, and centered tabs for the section
    // navigation (prev / current / next), reusing the shared primitives.
    if (section && section->lineCount > kMaxWindowLines_) {
        UiDraw::DrawScrollIndicator(*this, lineScroll_ > 0,
                                    (lineScroll_ + kMaxWindowLines_) <
                                        section->lineCount);
    }
    if (HelpRegistry::GetSectionCount() > 1 && !showIndex_) {
        const HelpSection *prevSec =
            HelpRegistry::GetSectionAt((sectionIndex_ + HelpRegistry::GetSectionCount() - 1) % HelpRegistry::GetSectionCount());
        const HelpSection *nextSec =
            HelpRegistry::GetSectionAt((sectionIndex_ + 1) % HelpRegistry::GetSectionCount());
        UiDraw::DrawTabs(*this, 1, prevSec ? prevSec->title : "?",
                         section ? section->title : "?",
                         nextSec ? nextSec->title : "?",
                         true);
    }
}

void HelpOverlay::ProcessButtonMask(unsigned short mask, bool pressed) {
    // RC4 P0/P1 (PLAN_RC4 sections 11.1/11.2): the overlay is a real modal
    // that must close deterministically and consume every event while open,
    // so nothing propagates to the view underneath.  Navigation only acts on
    // presses (auto-repeat is handled by the view system).
    if (!pressed) return;

    const unsigned short helpCombo = EPBM_SELECT | EPBM_R;
    // B or the opening chord always close the overlay.
    if ((mask & EPBM_B) != 0 ||
        (mask & helpCombo) == helpCombo) {
        EndModal(0);
        return;
    }

    // Navigation (only when the registry is non-empty).
    if (HelpRegistry::GetSectionCount() > 0) {
        if (showIndex_) {
            // In index mode the same controls move the section cursor and
            // A opens the highlighted section.
            if ((mask & EPBM_UP) != 0) {
                sectionIndex_--;
            } else if ((mask & EPBM_DOWN) != 0) {
                sectionIndex_++;
            } else if ((mask & EPBM_A) != 0) {
                showIndex_ = false;
            } else if ((mask & EPBM_L) != 0 ||
                       (mask & EPBM_L2) != 0) {
                sectionIndex_ = 0;
            } else if ((mask & EPBM_R) != 0 ||
                       (mask & EPBM_R2) != 0) {
                sectionIndex_ = HelpRegistry::GetSectionCount() - 1;
            }
            ClampCursor();
        } else {
            // Content mode: UP/DOWN scroll, L/R next/prev section,
            // L1/R1 next/prev section (close to the browser), L2/R2
            // first/last, A toggles the index.
            if ((mask & EPBM_UP) != 0) {
                lineScroll_--;
            } else if ((mask & EPBM_DOWN) != 0) {
                lineScroll_++;
            } else if ((mask & (EPBM_L | EPBM_R)) != 0) {
                if ((mask & EPBM_L) != 0) {
                    sectionIndex_--;
                } else {
                    sectionIndex_++;
                }
                lineScroll_ = 0;
            } else if ((mask & EPBM_L2) != 0) {
                sectionIndex_ = 0;
                lineScroll_ = 0;
            } else if ((mask & EPBM_R2) != 0) {
                sectionIndex_ = HelpRegistry::GetSectionCount() - 1;
                lineScroll_ = 0;
            } else if ((mask & EPBM_A) != 0) {
                showIndex_ = true;
                lineScroll_ = 0;
            }
            ClampCursor();
        }
    }
}

void HelpOverlay::OnPlayerUpdate(PlayerEventType, unsigned int) {}
void HelpOverlay::OnFocus() { isDirty_ = true; }

void HelpOverlayApplyCallback(View &view, ModalView &dialog) {
    (void)view;
    (void)dialog;
}