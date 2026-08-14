#ifndef _FX_NAVIGATOR_H_
#define _FX_NAVIGATOR_H_

// F3-4d (docs/F3_ARCHITECTURE_ES.md): capa pura de la navegacion y edicion
// de filas de las paginas FX del Mixer.  Declara el estado del cursor
// (pagina, fila y target de edicion del MIX page) y la matematica golden
// de los pasos de edicion (lineal con paso grueso/fino, filas bool-ish a
// paso 1, y edicion en curva musical/log para los parametros wide-range).
// No depende de GUI, audio, Player, SamplePool ni del framebuffer: solo
// FxPages.h (que a su vez solo usa fixed.h y <math.h>).
// Todo el comportamiento es byte-identico al que vivia en MixerView.cpp
// (golden Bacon 1.2.1).
#include "Application/Mixer/FxPages.h"

class FxNavigator {
  public:
    // Estado inicial golden: pagina MIX, fila 0, target VOL (0).
    FxNavigator() : page_(FX_PAGE_MIX), row_(0), editTarget_(0) {}

    // --- Estado del cursor (la vista delega) ---
    FxPage Page() const { return page_ ; }
    int Row() const { return row_ ; }
    // TREEFROG_FX_PAGES_V3 (Fase 9): 0=VOL 1=DLY RET 2=RVB RET en MIX page.
    int EditTarget() const { return editTarget_ ; }

    // JumpToFxPage golden: pagina fuera de rango se ignora; al entrar la
    // fila se resetea a 0.
    void SetPage(FxPage page) {
        if (page < FX_PAGE_MIX || page >= FX_PAGE_COUNT) return ;
        page_ = page ;
        row_ = 0 ;
    }

    // cycleFxPage golden: SELECT cicla MIX->DELAY->REVERB->EQ->COMP->MIX.
    void CyclePage() {
        page_ = (FxPage)((page_ + 1) % FX_PAGE_COUNT) ;
        row_ = 0 ;
    }

    // fxMoveRow golden: wrap de la fila dentro del numero de filas de la
    // pagina; si la pagina no tiene filas (MIX) no mueve nada.
    void MoveRow(int delta) {
        int count = fxCountOnPage(page_) ;
        if (count <= 0) return ;
        row_ += delta ;
        if (row_ < 0) row_ = count - 1 ;
        if (row_ >= count) row_ = 0 ;
    }

    // ACTION_CYCLE_FX_EDIT_TARGET golden: VOL -> DLY RET -> RVB RET.
    void CycleEditTarget() {
        editTarget_ = (editTarget_ + 1) % 3 ;
    }

    // Id de la fila actual en la pagina actual (0 = bypass; -1 si la
    // pagina no tiene filas).
    int IdForRow() const {
        return fxIdForRow(page_, row_) ;
    }

    // --- Matematica de edicion pura (fxEditRow/fxEditCurve/fxResetRow) ---
    // Devuelve el nuevo valor para un paso de edicion del id dado partiendo
    // de current.  Los parametros wide-range (fxUsesCurve) se editan en
    // curva musical (fxEditCurveValue); el resto con paso lineal 1/10
    // (fino/grueso), paso 1 si el rango es bool-ish (<=1.5), clamp final.
    static float EditValue(int id, float current, int delta, bool coarse) {
        const FxParamSpec &spec = kFxParams_[id] ;
        if (fxUsesCurve(id)) {
            return fxEditCurveValue(spec, current, delta, coarse) ;
        }
        float step = (coarse ? 10.0f : 1.0f) ;
        if (spec.vmax - spec.vmin <= 1.5f) step = 1.0f ;
        float v = current + step * (float)delta ;
        if (v < spec.vmin) v = spec.vmin ;
        if (v > spec.vmax) v = spec.vmax ;
        return v ;
    }

    // fxResetRow golden (A+B): valor legacy por defecto (vdef).
    static float ResetValue(int id) {
        return kFxParams_[id].vdef ;
    }

  private:
    FxPage page_ ;
    int row_ ;
    int editTarget_ ;
} ;

#endif
