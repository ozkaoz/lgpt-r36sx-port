#ifndef _HELP_REGISTRY_H_
#define _HELP_REGISTRY_H_

#include "Application/Views/BaseClasses/View.h"

/*
 * TREEFROG_HELP_REGISTRY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, points
 * 10/12).
 *
 * Central registry of context help for the whole UI.  Each view registers a
 * small block of help lines; HelpOverlay renders the block for the active
 * view when SELECT+R1 is held.  Keeping the text here (instead of in each
 * view's DrawView) is what lets us retire the permanent hint lines at the
 * bottom of every page (plan point 26).
 */

struct HelpLine {
    const char *keys;   // e.g. "L/R" or "A+UP"
    const char *action; // e.g. "move cursor"
};

struct HelpSection {
    const char *title;          // e.g. "SONG"
    const HelpLine *lines;
    int lineCount;
};

class HelpRegistry {
  public:
    // Returns the help section for a view type, or 0 if the view has none.
    static const HelpSection *GetSection(ViewType vt);

    // Number of lines in a section (bounded for the overlay window).
    static int GetLineCount(const HelpSection *section);

    // RC4 P1 (PLAN_RC4 section 11.2): navigable help browses every section
    // by index.  GetSectionCount returns the number of registered sections
    // and GetSectionAt(i) returns section i (0-based) for the browser.
    static int GetSectionCount();
    static const HelpSection *GetSectionAt(int index);
};

#endif  // _HELP_REGISTRY_H_