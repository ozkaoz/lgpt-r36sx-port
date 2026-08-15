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

    // cycleFxPage golden: SELECT cicla
    // MIX->DELAY->REVERB->EQ->EQ_EXT->COMP->MIX (FXP_MASTER_EQ8 adds the
    // EXT page).
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
    // FXP_DESCRIPTORS_V1 (bacon-1.5, item 1): los parametros continuos se
    // editan SIEMPRE en la vista comun 0..100 % (paso fino 1, grueso 10),
    // convirtiendo al rango natural via la capa FxParamDescriptor (curva
    // LOG2 para frecuencia/tiempo, LINEAL para ganancias/mezclas).  Los
    // switches (kind==SWITCH) conservan el paso 0/1 golden.  FXP_MASTER_EQ8
    // (bacon-1.5, item 2): los discretos multi-valor (TYP 0..6) tambien
    // usan paso 1 (nunca %), via fxIsDiscreteParam.
    static float EditValue(int id, float current, int delta, bool coarse) {
        const FxParamSpec &spec = kFxParams_[id] ;
        if (!fxIsPercentParam(id) || fxIsDiscreteParam(id)) {
            float step = (coarse ? 10.0f : 1.0f) ;
            if (spec.vmax - spec.vmin <= 1.5f || fxIsDiscreteParam(id)) step = 1.0f ;
            float v = current + step * (float)delta ;
            if (v < spec.vmin) v = spec.vmin ;
            if (v > spec.vmax) v = spec.vmax ;
            return v ;
        }
        int step = coarse ? 10 : 1 ;
        int p = fxDspToPercentId(id, current) ;
        p += step * delta ;
        if (p < 0) p = 0 ;
        if (p > 100) p = 100 ;
        return fxPercentToDspId(id, p) ;
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
