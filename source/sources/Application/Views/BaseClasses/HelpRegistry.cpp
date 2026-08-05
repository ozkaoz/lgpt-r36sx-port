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
};

static const HelpLine kTableLines_[] = {
    {"L/R UP/DN", "move cursor"},
    {"A", "edit step"},
    {"B", "cancel"},
    {"START", "play/stop"},
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    {"L1+X R1+X", "undo redo"},
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
    {"SELECT+R1", "help"},
    {"SELECT+R2", "audio driver"},
    // TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1)
    {"A+B", "reset vol/pan"},
    {"L2+A+B", "pan center"},
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
};
static const int kSectionCount_ =
    (int)(sizeof(kSections_) / sizeof(HelpSection));

const HelpSection *HelpRegistry::GetSection(ViewType vt) {
    if (vt < VT_SONG || vt > VT_MIXER) return 0;
    return &kSections_[vt];
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