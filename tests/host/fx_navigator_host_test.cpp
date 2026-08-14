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

    // 7. EditValue lineal golden (paso fino 1 / grueso 10, clamps).
    // DLY FBK (0..0.98): fino +1 -> 1.5 clamp a vmax.
    float v = FxNavigator::EditValue(FX_P_DLY_FBK, 0.5f, 1, false) ;
    check(feq(v, 0.98f), "linear fine +1 DLY FBK clamps vmax") ;
    // DLY FBK grueso +1 por encima del rango -> clamp vmax.
    v = FxNavigator::EditValue(FX_P_DLY_FBK, 0.95f, 1, true) ;
    check(feq(v, 0.98f), "linear coarse +1 clamps vmax") ;
    // DLY FBK fino -1 bajo el rango -> clamp vmin.
    v = FxNavigator::EditValue(FX_P_DLY_FBK, 0.02f, -1, false) ;
    check(feq(v, 0.0f), "linear fine -1 clamps vmin") ;
    // CMP THR (-60..0): grueso +1.
    v = FxNavigator::EditValue(FX_P_CMP_THR, -50.0f, 1, true) ;
    check(feq(v, -40.0f), "linear coarse +1 CMP THR") ;
    // CMP THR grueso -1 bajo el rango -> clamp vmin.
    v = FxNavigator::EditValue(FX_P_CMP_THR, -55.0f, -1, true) ;
    check(feq(v, -60.0f), "linear coarse -1 clamps vmin") ;

    // 8. Rows bool-ish (rango <=1.5): paso 1 incluso en grueso.
    // DLY PP (0..1).
    v = FxNavigator::EditValue(FX_P_DLY_PP, 0.0f, 1, true) ;
    check(feq(v, 1.0f), "bool row coarse +1 steps 1") ;
    v = FxNavigator::EditValue(FX_P_DLY_PP, 1.0f, -1, true) ;
    check(feq(v, 0.0f), "bool row coarse -1 steps 1") ;

    // 9. EditValue curva golden (musical/log).
    // EQ LOW FRQ (20..20000, default 100): semitono x2^(1/12) hacia arriba.
    v = FxNavigator::EditValue(FX_P_EQ_LOW_FRQ, 100.0f, 1, false) ;
    check(feq(v, 100.0f * 1.05946309436f), "curve semitone up") ;
    // Octava hacia abajo /2.
    v = FxNavigator::EditValue(FX_P_EQ_LOW_FRQ, 100.0f, -1, true) ;
    check(feq(v, 50.0f), "curve octave down") ;
    // Desde 0 (default DLY TIM): primer paso parte del 1% del rango.
    v = FxNavigator::EditValue(FX_P_DLY_TIME, 0.0f, 1, false) ;
    check(feq(v, 10.0f * 1.05946309436f), "curve from 0 starts at vmin") ;
    // RVB PRE (0..100, vmin 0): primer paso desde 0 = 1% del rango.
    v = FxNavigator::EditValue(FX_P_RVB_PRE, 0.0f, 1, false) ;
    check(feq(v, 1.0f * 1.05946309436f), "curve from 0 uses 1% of range") ;
    // Clamp superior de curva.
    v = FxNavigator::EditValue(FX_P_DLY_TIME, 1990.0f, 1, true) ;
    check(feq(v, 2000.0f), "curve clamps vmax") ;

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
