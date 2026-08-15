// F3-4d: FxNavigator - oraculos golden de la navegacion/edicion de las
// paginas FX del Mixer (header-only).  Compila en host (g++ ASAN/UBSAN)
// sin ningun include de GUI/audio/Player.
#include "Application/Mixer/FxNavigator.h"
#include <stdio.h>
#include <math.h>

static int g_failures = 0 ;
static int g_checks = 0 ;

static void check(bool cond, const char *what) {
    g_checks++ ;
    if (!cond) {
        g_failures++ ;
        printf("FAIL: %s\n", what) ;
    }
}

static bool feq(float a, float b) {
    return fabsf(a - b) < 1e-3f ;
}

int main() {
    // 1. Estado inicial golden: MIX / fila 0 / target VOL.
    FxNavigator nav ;
    check(nav.Page() == FX_PAGE_MIX, "initial page MIX") ;
    check(nav.Row() == 0, "initial row 0") ;
    check(nav.EditTarget() == 0, "initial edit target VOL") ;

    // 2. SetPage golden: rango, reset de fila, ignorar fuera de rango.
    nav.SetPage(FX_PAGE_DELAY) ;
    check(nav.Page() == FX_PAGE_DELAY, "SetPage DELAY") ;
    check(nav.Row() == 0, "SetPage resets row") ;
    nav.MoveRow(3) ;
    check(nav.Row() == 3, "MoveRow to 3") ;
    nav.SetPage(FX_PAGE_REVERB) ;
    check(nav.Page() == FX_PAGE_REVERB, "SetPage REVERB") ;
    check(nav.Row() == 0, "SetPage resets row again") ;
    nav.SetPage((FxPage)(FX_PAGE_COUNT + 2)) ;
    check(nav.Page() == FX_PAGE_REVERB, "SetPage ignores out-of-range hi") ;
    nav.SetPage((FxPage)(FX_PAGE_MIX - 1)) ;
    check(nav.Page() == FX_PAGE_REVERB, "SetPage ignores out-of-range lo") ;

    // 3. CyclePage golden: MIX->DELAY->REVERB->EQ->COMP->MIX, fila 0.
    nav.SetPage(FX_PAGE_MIX) ;
    nav.MoveRow(2) ;
    nav.CyclePage() ;
    check(nav.Page() == FX_PAGE_DELAY, "CyclePage MIX->DELAY") ;
    check(nav.Row() == 0, "CyclePage resets row") ;
    nav.CyclePage() ;
    check(nav.Page() == FX_PAGE_REVERB, "CyclePage DELAY->REVERB") ;
    nav.CyclePage() ;
    check(nav.Page() == FX_PAGE_EQ, "CyclePage REVERB->EQ") ;
    nav.CyclePage() ;
    check(nav.Page() == FX_PAGE_COMP, "CyclePage EQ->COMP") ;
    nav.CyclePage() ;
    check(nav.Page() == FX_PAGE_MIX, "CyclePage COMP->MIX") ;

    // 4. MoveRow wrap golden por pagina (DELAY 7, REVERB 7, EQ 13, COMP 9).
    nav.SetPage(FX_PAGE_DELAY) ;
    nav.MoveRow(-1) ;
    check(nav.Row() == 6, "MoveRow wrap up DELAY") ;
    nav.MoveRow(1) ;
    check(nav.Row() == 0, "MoveRow wrap down DELAY") ;
    nav.MoveRow(1) ;
    check(nav.Row() == 1, "MoveRow forward") ;
    nav.SetPage(FX_PAGE_EQ) ;
    nav.MoveRow(-1) ;
    check(nav.Row() == 12, "MoveRow wrap up EQ 13 rows") ;
    nav.SetPage(FX_PAGE_COMP) ;
    nav.MoveRow(-1) ;
    check(nav.Row() == 8, "MoveRow wrap up COMP 9 rows") ;
    nav.SetPage(FX_PAGE_MIX) ;
    nav.MoveRow(5) ;
    check(nav.Row() == 0, "MoveRow no-op on MIX page") ;

    // 5. IdForRow golden: bypass primero, resto en orden de tabla.
    nav.SetPage(FX_PAGE_DELAY) ;
    check(nav.IdForRow() == FX_P_DLY_BYP, "DELAY row0 = bypass") ;
    nav.MoveRow(1) ;
    check(nav.IdForRow() == FX_P_DLY_TIME, "DELAY row1 = DLY TIME") ;
    nav.SetPage(FX_PAGE_REVERB) ;
    check(nav.IdForRow() == FX_P_RVB_BYP, "REVERB row0 = bypass") ;
    nav.SetPage(FX_PAGE_EQ) ;
    check(nav.IdForRow() == FX_P_EQ_BYP, "EQ row0 = bypass") ;
    nav.MoveRow(1) ;
    check(nav.IdForRow() == FX_P_EQ_LOW_EN, "EQ row1 = LOW EN") ;
    nav.SetPage(FX_PAGE_COMP) ;
    check(nav.IdForRow() == FX_P_CMP_BYP, "COMP row0 = bypass") ;
    nav.SetPage(FX_PAGE_MIX) ;
    check(nav.IdForRow() == -1, "MIX IdForRow = -1") ;

    // 6. CycleEditTarget golden: VOL->DLY RET->RVB RET->VOL.
    nav.SetPage(FX_PAGE_MIX) ;
    check(nav.EditTarget() == 0, "edit target VOL") ;
    nav.CycleEditTarget() ;
    check(nav.EditTarget() == 1, "edit target DLY RET") ;
    nav.CycleEditTarget() ;
    check(nav.EditTarget() == 2, "edit target RVB RET") ;
    nav.CycleEditTarget() ;
    check(nav.EditTarget() == 0, "edit target wraps to VOL") ;

    // 7. EditValue percent golden (FXP_DESCRIPTORS_V1: los continuos se
    // editan SIEMPRE en la vista comun 0..100 %; paso fino 1, grueso 10,
    // clamps 0..100; el rango natural se convierte via el descriptor).
    // DLY FBK (0..0.98 LINEAR): 0.5 -> p51, +1 fino = p52 -> 0.5096.
    float v = FxNavigator::EditValue(FX_P_DLY_FBK, 0.5f, 1, false) ;
    check(feq(v, 0.5096f), "percent fine +1 DLY FBK") ;
    // DLY FBK grueso +1: p97+10 -> clamp p100 -> vmax.
    v = FxNavigator::EditValue(FX_P_DLY_FBK, 0.95f, 1, true) ;
    check(feq(v, 0.98f), "percent coarse +1 clamps vmax") ;
    // DLY FBK fino -1: p2-1 = p1 -> 0.0098.
    v = FxNavigator::EditValue(FX_P_DLY_FBK, 0.02f, -1, false) ;
    check(feq(v, 0.0098f), "percent fine -1 DLY FBK") ;
    // CMP THR (-60..0 LINEAR): -50 -> p17, +10 = p27 -> -43.8.
    v = FxNavigator::EditValue(FX_P_CMP_THR, -50.0f, 1, true) ;
    check(feq(v, -43.8f), "percent coarse +1 CMP THR") ;
    // CMP THR grueso -1 bajo el rango: p8-10 -> clamp p0 -> vmin.
    v = FxNavigator::EditValue(FX_P_CMP_THR, -55.0f, -1, true) ;
    check(feq(v, -60.0f), "percent coarse -1 clamps vmin") ;

    // 8. Rows switch (kind==SWITCH): paso 1 incluso en grueso.
    // DLY PP (0..1).
    v = FxNavigator::EditValue(FX_P_DLY_PP, 0.0f, 1, true) ;
    check(feq(v, 1.0f), "switch row coarse +1 steps 1") ;
    v = FxNavigator::EditValue(FX_P_DLY_PP, 1.0f, -1, true) ;
    check(feq(v, 0.0f), "switch row coarse -1 steps 1") ;

    // 9. EditValue percent curva LOG2 golden (frecuencia/tiempo).
    // EQ LOW FRQ (20..20000, default 100): p23 +1 fino = p24 -> 20*10^0.72.
    v = FxNavigator::EditValue(FX_P_EQ_LOW_FRQ, 100.0f, 1, false) ;
    check(feq(v, 104.9615f), "log2 fine +1 EQ LOW FRQ") ;
    // Octava hacia abajo (grueso -1): p23-10 = p13 -> 20*1000^0.13.
    v = FxNavigator::EditValue(FX_P_EQ_LOW_FRQ, 100.0f, -1, true) ;
    check(feq(v, 49.0943f), "log2 coarse -1 EQ LOW FRQ") ;
    // Desde 0 (default DLY TIM): clamp a vmin -> p0 +1 = p1 -> 10.54.
    v = FxNavigator::EditValue(FX_P_DLY_TIME, 0.0f, 1, false) ;
    check(feq(v, 10.5441f), "log2 from 0 starts at vmin") ;
    // RVB PRE (0..100 LINEAR): desde 0 +1 = p1 -> 1.0.
    v = FxNavigator::EditValue(FX_P_RVB_PRE, 0.0f, 1, false) ;
    check(feq(v, 1.0f), "linear from 0 uses 1% of range") ;
    // Clamp superior de curva.
    v = FxNavigator::EditValue(FX_P_DLY_TIME, 1990.0f, 1, true) ;
    check(feq(v, 2000.0f), "percent clamps vmax") ;

    // 10. ResetValue golden (A+B -> vdef).
    check(feq(FxNavigator::ResetValue(FX_P_DLY_MIX), 1.0f), "reset DLY MIX") ;
    check(feq(FxNavigator::ResetValue(FX_P_CMP_THR), -24.0f), "reset CMP THR") ;
    check(feq(FxNavigator::ResetValue(FX_P_EQ_MID_FRQ), 1000.0f),
          "reset EQ MID FRQ") ;
    check(feq(FxNavigator::ResetValue(FX_P_RVB_BYP), 0.0f), "reset RVB BYP") ;

    if (g_failures == 0) {
        printf("ALL OK (%d checks)\n", g_checks) ;
        return 0 ;
    }
    printf("%d/%d checks FAILED\n", g_failures, g_checks) ;
    return 1 ;
}
