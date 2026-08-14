// TREEFROG_V42_NO_WHITE_BOX_UI
#include "ModalView.h"
#include "UiDraw.h"

ModalView::ModalView(View &v)
    : View(v.w_, v.viewData_), finished_(false), returnCode_(0), topOffset_(0),
      width_(0), height_(0){};

ModalView::~ModalView(){};

void ModalView::SetWindowOffset(int dy) {
    topOffset_ = dy;
    isDirty_ = true;
};

int ModalView::GetReturnCode() { return returnCode_; };

bool ModalView::IsFinished() { return finished_; };

void ModalView::EndModal(int returnCode) {
    returnCode_ = returnCode;
    finished_ = true;
};

void ModalView::ClearRect(int x, int y, int w, int h) {
    View::ClearRect(x + left_, y + top_, w, h);
}
void ModalView::DrawString(int x, int y, const char *txt,
                           GUITextProperties &props) {
    View::DrawString(x + left_, y + top_, txt, props);
};

void ModalView::SetWindow(int width, int height) {

    if (width > 36) {
        width = 36;
    };
    if (height > 26) {
        height = 26;
    };

    left_ = (UiDraw::kScreenWidth - width) / 2;
    // RC5 (40x30 grid): the window centers vertically on the full 30-row
    // screen (previously it centered on a 20-row grid, pushing windows
    // toward the top).  The top border lives at row top_-2 and the bottom
    // border at top_+height+1, so top_ is clamped to keep both borders
    // inside rows 0..29.
    top_ = (UiDraw::kScreenHeight - height) / 2 + topOffset_;
    if (top_ < 2) {
        top_ = 2;
    }
    int maxTop = UiDraw::kScreenHeight - 1 - height - 1;
    if (top_ > maxTop) {
        top_ = maxTop;
    }
    width_ = width;
    height_ = height;
    ClearRect(-1, -1, width + 2, height + 2);

    SetColor(CD_BORDER);
    GUITextProperties props;
    props.invert_ = false;
    char line[41];
    memset(line, ' ', 40);
    line[width + 4] = 0;
    DrawString(-2, -2, line, props);
    DrawString(-2, height + 1, line, props);
    line[1] = 0;
    for (int i = 0; i < height + 2; i++) {
        DrawString(-2, i - 1, line, props);
        DrawString(width + 1, i - 1, line, props);
    }
};
