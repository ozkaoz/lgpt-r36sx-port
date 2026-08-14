// TREEFROG_TEXT_EDITOR_V1 (H38.6)
// Generic single-line text editor sharing the exact input logic of the
// USB-C Record file name editor and the sample rename modal:
//   X+UP/X+DOWN  cycle character by +/-5 (fast)
//   UP/DOWN      cycle character by +/-1
//   LEFT/RIGHT   move cursor
//   L1+X         toggle case
//   B            erase character under cursor
//   A            confirm (EndModal(1))
//   R1+LEFT      cancel  (EndModal(0))
// Physical-edge input is authoritative while the editor is open.
#ifndef _TREEFROG_TEXT_EDITOR_H_
#define _TREEFROG_TEXT_EDITOR_H_

#include "Application/UI/Views/BaseClasses/ModalView.h"

class TreeFrogTextEditor : public ModalView {
public:
    // title: inverted header line. initialName: optional starting text.
    // maxStem: maximum number of characters in the edited stem.
    // suffix: optional extension (e.g. ".wav") appended by GetFinalName().
    TreeFrogTextEditor(View &view,
                       const char *title,
                       const char *initialName = 0,
                       int maxStem = 24,
                       const char *suffix = 0);
    virtual ~TreeFrogTextEditor();

    // Edited text without the suffix.
    const char *GetName() const { return stem_; }
    // Edited text with the configured suffix appended.
    const char *GetFinalName();

    virtual void DrawView();
    virtual void OnFocus();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void OnFrameUpdate(unsigned long);
    virtual void ProcessButtonMask(unsigned short, bool) {}

protected:
    // Re-seeds the edited text (used by NewProjectDialog::SetInitialName).
    void setInitialText(const char *text);

private:
    void setStatus(const char *text);
    void moveCursor(int delta);
    void cycleCharacter(int delta);
    void toggleCase();
    void eraseCharacter();
    void processPhysicalInput();

    char title_[24];
    char stem_[32];
    char finalName_[40];
    char status_[40];
    int maxStem_;
    const char *suffix_;
    int cursor_;
    bool lowercase_;
    bool armed_;
    unsigned int physicalMask_;
    int neutralFrames_;
};

#endif
