#include "HelpRegistry.h"

/*
 * TREEFROG_HELP_REGISTRY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, points
 * 10/12).
 *
 * Context help text for every view.  Lines are intentionally short (keys +
 * action) so the overlay can render them wrapped in the 8-line window.
 */

static const HelpLine kSongLines_[] = {
    {"L/R UP/DN", "move cursor"},
    {"A", "edit / enter chain"},
    {"B", "mute / solo"},
    {"R1+A", "solo chain"},
    {"R1+B", "mute chain"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    // TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): global combos, every view
    {"L1+X R1+X", "undo redo"},
    // TREEFROG_NAV_X_DIR / TREEFROG_NAV_SONG_X_4ROW (Bacon 1.1.1)
    {"X+UP/DN", "jump 4 rows"},
    {"B+UP/DN", "page (16 rows)"},
    {"A+B", "reset option"},
};

static const HelpLine kChainLines_[] = {
    {"L/R", "step cursor"},
    {"UP/DN", "move cursor"},
    {"A", "open phrase"},
    {"B", "cancel / back"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
    {"X+dir", "quick jump"},
    {"A+B", "reset option"},
};

static const HelpLine kPhraseLines_[] = {
    {"L/R UP/DN", "move cursor"},
    {"A", "set note"},
    {"B", "cancel"},
    {"A+UP/DN", "octave"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
    // TREEFROG_NAV_X_DIR (Bacon 1.1.1): phrase quick nav; B+UP/DN pages
    // through the chain's phrases, X+UP/DN jumps 4 rows inside the phrase.
    {"X+UP/DN", "jump 4 rows"},
    {"B+UP/DN", "page (next phrase)"},
    {"A+B", "reset option"},
};

static const HelpLine kProjectLines_[] = {
    {"L/R", "switch page"},
    {"UP/DN", "move cursor"},
    {"A", "open / select"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
    {"X+dir", "quick jump"},
    {"A+B", "reset option"},
};

static const HelpLine kInstrumentLines_[] = {
    {"L/R", "switch page"},
    {"UP/DN", "move cursor"},
    {"L/R", "edit value"},
    {"A+UP/DN", "coarse"},
    {"R2+A", "fx menu"},
    {"R1+RIGHT", "USB record"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"A+B", "reset field"},
    {"L1+X R1+X", "undo redo"},
    {"X+dir", "quick jump"},
};

static const HelpLine kTableLines_[] = {
    {"L/R UP/DN", "move cursor"},
    {"A", "edit step"},
    {"B", "cancel"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
    {"X+dir", "quick jump"},
    {"A+B", "reset option"},
};

static const HelpLine kGrooveLines_[] = {
    {"L/R", "move cursor"},
    {"UP/DN", "edit value"},
    {"A+UP/DN", "coarse"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
    {"X+dir", "quick jump"},
    {"A+B", "reset option"},
};

static const HelpLine kMixerLines_[] = {
    {"L/R", "channel"},
    {"UP/DN", "volume"},
    {"L->MST", "master select"},
    {"L2+L/R", "pan"},
    {"L2+A+L/R", "pan coarse"},
    {"R2", "edit target"},
    {"SELECT", "page cycle"},
    {"R1+A", "solo"},
    {"R1+B", "mute"},
    {"UP/DN", "row"},
    {"L/R", "edit"},
    {"A", "coarse"},
    {"START", "play"},
    {"R+UP", "back to Song"},
    {"L1+A", "master/track menu"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    // TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1)
    {"A+B", "reset vol/pan"},
    {"L2+A+B", "pan center"},
    {"L1+X R1+X", "undo redo"},
};

// TREEFROG_CHOPPER_HELP_V2 (Bacon 1.1.1): full chopper combo list, including
// the split cycle and the trim-mode zero-cross snap combos the user verified.
static const HelpLine kChopperLines_[] = {
    {"L/R", "cursor"},
    {"UP/DN", "zoom"},
    {"A", "add chop"},
    {"Y", "del chop"},
    {"B", "play chop"},
    {"SELECT", "trim mode"},
    {"L1+L/R", "fast cursor"},
    {"R1+L/R", "sample"},
    {"R2+L/R", "chop sel"},
    {"R2+A", "play full"},
    {"R2+Y", "normalize (trim)"},
    {"L1+A", "snap start 0ch"},
    {"L1+B", "split a4/8/16/32/0"},
    {"L1+B", "snap end 0ch (trim)"},
    {"L1+R1", "pitch/env"},
    {"A+B", "range nudge"},
    {"R1+A", "crop apply"},
    {"L2+Y", "del range"},
    {"R1+B", "back"},
    {"L2+B", "stop"},
    {"L1+X R1+X", "undo redo"},
};

// TREEFROG_CHOPPER_HELP_V1: pitch/env submenu of the chopper (shown when
// the modal is in pitch mode; the same section covers both submenus).
static const HelpLine kChopperPitchLines_[] = {
    {"L/R UP/DN", "item value"},
    {"B", "preview"},
    {"A", "apply pitch/env"},
    {"L1+R1", "exit submenu"},
    {"R2+L/R", "chop target"},
    {"SELECT", "trim mode"},
    {"L1+X R1+X", "undo redo"},
};

// BACON_1.5_EQ8_HELP (U2.52.8, feedback (E)): the EQ8 editor is a fullscreen
// view of its own (VT_INSTRUMENT_EQ) with dedicated combos; SELECT+R1 on the
// EQ screen must explain them instead of opening with an empty section.
// U2.65: L2+X 1 Hz lineal, R2+X slope 12..96, B+arrows ordenado, Y Q, etc.
static const HelpLine kEq8Lines_[] = {
    {"L/R", "band 1-8 select"},
    {"X+L/R", "freq log +-1 step"},
    {"X+UP/DN", "gain +-1 dB"},
    // BACON_1.5_EQ8_FAST_COARSE (U2.52.9, feedback #6): L1+X+arrows = fast
    // displacement (~1 octave per step, 10 dB per step) like the rest of
    // the port; no conflict with the L1+X global undo (no arrows).
    {"L1+X L/R", "freq ~1 oct"},
    {"L1+X UP/DN", "gain +-10 dB"},
    {"L2+X L/R", "freq +-1 Hz"},
    // BACON_1.5_ANALYZER_PEAK (U2.61, feedback #13) -> PEAKHIST (U2.62)
    // L2+R2 historical peak, L2+R2+X band->peak, R2+X slope 12..96
    {"L2+R2", "peak marker (hist)"},
    {"L2+R2+X", "band -> peak"},
    {"R2+X UP/DN", "slope 12..96 dB/oct"},
    {"B", "next filter type"},
    {"B+L/R B+UP/DN", "prev/next type order"},
    {"Y+L/R", "Q wider/narrower"},
    {"Y+UP/DN", "intensity all bands"},
    {"A", "band on/off"},
    {"A+B", "reset band default"},
    {"SELECT", "bypass EQ"},
    {"START", "play/stop"},
    {"R+START", "stop"},
    {"R+B", "back to instr"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
};

static const HelpSection kSections_[] = {
    {"SONG", kSongLines_, (int)(sizeof(kSongLines_) / sizeof(HelpLine))},
    {"CHAIN", kChainLines_, (int)(sizeof(kChainLines_) / sizeof(HelpLine))},
    {"PHRASE", kPhraseLines_, (int)(sizeof(kPhraseLines_) / sizeof(HelpLine))},
    {"PROJECT", kProjectLines_, (int)(sizeof(kProjectLines_) / sizeof(HelpLine))},
    {"INSTRUMENT", kInstrumentLines_, (int)(sizeof(kInstrumentLines_) / sizeof(HelpLine))},
    {"TABLE", kTableLines_, (int)(sizeof(kTableLines_) / sizeof(HelpLine))},
    {"GROOVE", kGrooveLines_, (int)(sizeof(kGrooveLines_) / sizeof(HelpLine))},
    {"MIXER", kMixerLines_, (int)(sizeof(kMixerLines_) / sizeof(HelpLine))},
    // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13)
    {"CHOPPER", kChopperLines_,
     (int)(sizeof(kChopperLines_) / sizeof(HelpLine))},
    {"CHOP PITCH", kChopperPitchLines_,
     (int)(sizeof(kChopperPitchLines_) / sizeof(HelpLine))},
    // BACON_1.5_EQ8_HELP (U2.52.8, feedback (E))
    {"EQ8", kEq8Lines_, (int)(sizeof(kEq8Lines_) / sizeof(HelpLine))},
};
static const int kSectionCount_ =
    (int)(sizeof(kSections_) / sizeof(HelpSection));

const HelpSection *HelpRegistry::GetSection(ViewType vt) {
    // Map each ViewType to its canonical section by name, NOT by ordinal:
    // kSections_ (line 170) no esta alineado con el enum ViewType desde que
    // se anadieron TABLE2 (6) y CHOPPER/CHOP PITCH al final del enum, y un
    // indexado directo hacia que el help de MIXER abriera en CHOPPER
    // (reportado en consola).  El orden de navegacion del browser no
    // cambia: GetSectionAt sigue recorriendo kSections_.
    switch (vt) {
        case VT_SONG:
            return &kSections_[0];
        case VT_CHAIN:
            return &kSections_[1];
        case VT_PHRASE:
            return &kSections_[2];
        case VT_PROJECT:
            return &kSections_[3];
        case VT_INSTRUMENT:
            return &kSections_[4];
        case VT_TABLE:
        case VT_TABLE2:
            return &kSections_[5];
        case VT_GROOVE:
            return &kSections_[6];
        case VT_MIXER:
            return &kSections_[7];
        // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): the chopper is a
        // modal, not a regular view type; map it to its help section.
        case VT_CHOPPER:
            return &kSections_[8];
        case VT_CHOPPITCH:
            return &kSections_[9];
        // BACON_1.5_EQ8_HELP (U2.52.8, feedback (E)): EQ8 maps to its own
        // help section (kept at the end, like the VT_* enum member).
        case VT_INSTRUMENT_EQ:
            return &kSections_[10];
        default:
            return 0;
    }
}

int HelpRegistry::GetLineCount(const HelpSection *section) {
    if (!section) return 0;
    return section->lineCount;
}

int HelpRegistry::GetSectionCount() { return kSectionCount_; }

const HelpSection *HelpRegistry::GetSectionAt(int index) {
    if (index < 0 || index >= kSectionCount_) return 0;
    return &kSections_[index];
}