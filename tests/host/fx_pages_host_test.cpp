// F3-4a: FxPages pure layer golden host test.
// Verifies the parameter table, row mapping, curve editing, VU scale and
// return conversions against hand-computed oracles, plus purity guards
// (no GUI/audio/Player/SamplePool dependencies).
#include "Application/Mixer/FxPages.h"
#include "Application/Mixer/FxNavigator.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int failures = 0 ;
static int checks = 0 ;

static void check(bool cond, const char *what) {
    checks++ ;
    if (!cond) {
        failures++ ;
        printf("FAIL: %s\n", what) ;
    }
}

static int countPage(FxPage page) {
    int count = 0 ;
    for (int i = 0 ; i < FX_PARAM_COUNT ; i++) {
        if (kFxParams_[i].page == page) count++ ;
    }
    return count ;
}

int main() {
    // ---- Table shape: 57 params (36 golden + 21 EQ EXT), pages match ----
    check(FX_PARAM_COUNT == 57, "FX_PARAM_COUNT == 57") ;
    check(countPage(FX_PAGE_DELAY) == 7, "DELAY page has 7 rows") ;
    check(countPage(FX_PAGE_REVERB) == 7, "REVERB page has 7 rows") ;
    check(countPage(FX_PAGE_EQ) == 13, "EQ page has 13 rows") ;
    check(countPage(FX_PAGE_EQ_EXT) == 21, "EQ_EXT page has 21 rows") ;
    check(countPage(FX_PAGE_COMP) == 9, "COMP page has 9 rows") ;
    check(countPage(FX_PAGE_MIX) == 0, "MIX page has no param rows") ;
    check(kFxParams_[FX_P_DLY_TIME].page == FX_PAGE_DELAY, "DLY TIME on DELAY") ;
    check(kFxParams_[FX_P_CMP_SC].page == FX_PAGE_COMP, "CMP SC on COMP") ;
    check(kFxParams_[FX_P_EQX_BYP].page == FX_PAGE_EQ_EXT, "EQX BYP on EQ_EXT") ;
    check(kFxParams_[FX_P_EQX_B7_TYP].page == FX_PAGE_EQ_EXT, "B7 TYP on EQ_EXT") ;
    check(strcmp(kFxParams_[FX_P_DLY_TIME].label, "DLY TIM") == 0, "DLY TIM label") ;
    check(strcmp(kFxParams_[FX_P_CMP_BYP].label, "CMP BYP") == 0, "CMP BYP label") ;
    check(strcmp(kFxParams_[FX_P_EQX_B4_TYP].label, "B4 TYP") == 0, "B4 TYP label") ;
    check(kFxParams_[FX_P_DLY_TIME].vmin == 10.0f && kFxParams_[FX_P_DLY_TIME].vmax == 2000.0f,
          "DLY TIM range 10..2000") ;
    check(kFxParams_[FX_P_CMP_THR].vmin == -60.0f && kFxParams_[FX_P_CMP_THR].vmax == 0.0f,
          "CMP THR range -60..0") ;
    check(kFxParams_[FX_P_EQX_B3_TYP].vmax == 6.0f, "B3 TYP max 6") ;
    check(kFxParams_[FX_P_EQX_B6_FRQ].vdef == 16000.0f, "B6 FRQ default 16000") ;

    // ---- Bypass rows first ----
    check(fxBypassId(FX_PAGE_DELAY) == FX_P_DLY_BYP, "DELAY bypass id") ;
    check(fxBypassId(FX_PAGE_REVERB) == FX_P_RVB_BYP, "REVERB bypass id") ;
    check(fxBypassId(FX_PAGE_EQ) == FX_P_EQ_BYP, "EQ bypass id") ;
    check(fxBypassId(FX_PAGE_EQ_EXT) == FX_P_EQX_BYP, "EQ_EXT bypass id") ;
    check(fxBypassId(FX_PAGE_COMP) == FX_P_CMP_BYP, "COMP bypass id") ;
    check(fxBypassId(FX_PAGE_MIX) == -1, "MIX has no bypass") ;

    // ---- Row mapping round trips on every page ----
    for (int page = FX_PAGE_DELAY ; page <= FX_PAGE_COMP ; page++) {
        int count = fxCountOnPage((FxPage)page) ;
        check(count > 0, "page has rows") ;
        for (int row = 0 ; row < count ; row++) {
            int id = fxIdForRow((FxPage)page, row) ;
            check(id >= 0 && kFxParams_[id].page == page, "row maps to id on page") ;
            check(fxRowForId(id) == row, "id maps back to same row") ;
            check(fxIdOnPage(id, (FxPage)page), "id is on its page") ;
        }
        check(fxIdForRow((FxPage)page, count) == -1, "row past end is -1") ;
        check(fxIdForRow((FxPage)page, -1) == -1, "negative row is -1") ;
        check(fxIdForRow((FxPage)page, 0) == fxBypassId((FxPage)page), "row 0 is bypass") ;
    }
    check(fxRowForId(FX_P_DLY_BYP) == 0, "DLY BYP row 0") ;
    check(fxRowForId(FX_P_DLY_TIME) == 1, "DLY TIME row 1") ;
    check(fxRowForId(FX_P_EQ_LOW_EN) == 1, "EQ LOW EN row 1") ;
    check(fxRowForId(FX_P_CMP_SC) == fxCountOnPage(FX_PAGE_COMP) - 1, "CMP SC last row") ;
    check(!fxIdOnPage(FX_P_DLY_TIME, FX_PAGE_COMP), "DLY TIME not on COMP") ;

    // ---- Curve params ----
    check(fxUsesCurve(FX_P_EQ_LOW_FRQ), "EQ LOW FRQ curve") ;
    check(fxUsesCurve(FX_P_DLY_TIME), "DLY TIME curve") ;
    check(fxUsesCurve(FX_P_RVB_DEC), "RVB DEC curve") ;
    check(fxUsesCurve(FX_P_CMP_ATK), "CMP ATK curve") ;
    check(fxUsesCurve(FX_P_CMP_RAT), "CMP RAT curve") ;
    check(fxUsesCurve(FX_P_EQX_B3_FRQ), "EQX B3 FRQ curve") ;
    check(fxUsesCurve(FX_P_EQX_B7_FRQ), "EQX B7 FRQ curve") ;
    check(!fxUsesCurve(FX_P_DLY_FBK), "DLY FBK not curve") ;
    check(!fxUsesCurve(FX_P_EQ_LOW_GAI), "EQ gain not curve") ;
    check(!fxUsesCurve(FX_P_CMP_BYP), "CMP BYP not curve") ;
    check(!fxUsesCurve(FX_P_EQX_B3_TYP), "EQX TYP not curve") ;

    // ---- Discrete params (bacon-1.5 item 2) ----
    check(fxIsDiscreteParam(FX_P_EQX_B3_TYP), "B3 TYP discrete") ;
    check(fxIsDiscreteParam(FX_P_EQX_B7_TYP), "B7 TYP discrete") ;
    check(fxIsDiscreteParam(FX_P_CMP_BYP), "CMP BYP discrete (switch)") ;
    check(!fxIsDiscreteParam(FX_P_EQX_B3_FRQ), "B3 FRQ not discrete") ;
    check(FxNavigator::EditValue(FX_P_EQX_B3_TYP, 1.0f, 1, true) == 2.0f, "TYP coarse steps by 1") ;
    check(FxNavigator::EditValue(FX_P_EQX_B3_TYP, 6.0f, 1, false) == 6.0f, "TYP clamps at 6") ;
    check(FxNavigator::EditValue(FX_P_EQX_B3_TYP, 0.0f, -1, false) == 0.0f, "TYP clamps at 0") ;

    // ---- Curve edit math (golden from Bacon 1.2.1) ----
    const FxParamSpec &dly = kFxParams_[FX_P_DLY_TIME] ;
    // v=0 default below vmin=10: first up edit snaps to vmin then semitone.
    float v = fxEditCurveValue(dly, 0.0f, 1, false) ;
    check(fabsf(v - (10.0f * 1.05946309436f)) < 0.001f, "DLY 0->+1 fine snaps floor*st") ;
    v = fxEditCurveValue(dly, 0.0f, 1, true) ;
    check(fabsf(v - 20.0f) < 0.001f, "DLY 0->+1 coarse snaps floor*2") ;
    v = fxEditCurveValue(dly, 10.0f, -1, false) ;
    check(fabsf(v - 10.0f) < 0.001f, "DLY down fine from floor clamps to vmin") ;
    v = fxEditCurveValue(dly, 100.0f, 12, false) ;
    check(fabsf(v - 200.0f) < 0.001f, "12 fine steps = octave") ;
    v = fxEditCurveValue(dly, 1000.0f, 2, true) ;
    check(fabsf(v - 2000.0f) < 0.001f, "2 coarse steps clamp at vmax") ;
    v = fxEditCurveValue(dly, 3000.0f, -1, false) ;
    check(fabsf(v - (2000.0f / 1.05946309436f)) < 0.001f, "above vmax snaps down then curve") ;
    const FxParamSpec &rvb = kFxParams_[FX_P_RVB_PRE] ;
    // vmin==0: first up edit starts from 1% of range (100*0.01=1) then *st.
    v = fxEditCurveValue(rvb, 0.0f, 1, false) ;
    check(fabsf(v - (1.0f * 1.05946309436f)) < 0.001f, "RVB PRE 0->+1 starts 1%*st") ;

    // ---- mixVULevel golden ----
    check(mixVULevel(0.0f) == 0.0f, "VU 0 -> 0") ;
    check(fabsf(mixVULevel(1.0f) - 1.0f) < 0.0001f, "VU 1.0 -> 1.0 (clamped)") ;
    // peak such that 20*log10(p)+12 = 0 -> p = 10^-0.6 = 0.25118..., level=(36)/39.
    {
        float p = powf(10.0f, -0.6f) ;
        float l = mixVULevel(p) ;
        check(fabsf(l - 36.0f / 39.0f) < 0.0001f, "VU 0 dB -> 36/39") ;
    }
    // peak = 10^-1.2 -> db=-12, level=(24)/39.
    {
        float p = powf(10.0f, -1.2f) ;
        float l = mixVULevel(p) ;
        check(fabsf(l - 24.0f / 39.0f) < 0.0001f, "VU -12 dB -> 24/39") ;
    }
    check(mixVULevel(1e-9f) == 0.0f, "VU tiny -> 0") ;
    check(mixVULevel(10.0f) == 1.0f, "VU over 1 clamps to 1") ;
    check(mixVULevel(-1.0f) == 0.0f, "VU negative -> 0") ;

    // ---- Return percent conversions (Q15 fixed 0..1 <-> 0..100) ----
    check(fxReturnPercent(fl2fp(0.0f)) == 0, "ret 0 -> 0%") ;
    check(fxReturnPercent(fl2fp(0.5f)) == 50, "ret 0.5 -> 50%") ;
    check(fxReturnPercent(fl2fp(1.0f)) == 100, "ret 1.0 -> 100%") ;
    check(fxReturnFromPercent(0) == fl2fp(0.0f), "0% -> ret 0") ;
    check(fxReturnFromPercent(50) == fl2fp(0.5f), "50% -> ret 0.5") ;
    check(fxReturnFromPercent(100) == fl2fp(1.0f), "100% -> ret 1.0") ;
    check(fxReturnPercent(fxReturnFromPercent(37)) == 37, "round trip 37%") ;
    check(fxReturnPercent(fxReturnFromPercent(0)) == 0, "round trip 0%") ;
    check(fxReturnPercent(fxReturnFromPercent(100)) == 100, "round trip 100%") ;
    check(fxReturnFromPercent(-5) == fl2fp(0.0f), "percent clamps up") ;
    check(fxReturnFromPercent(150) == fl2fp(1.0f), "percent clamps down") ;
    {
        // mid-scale round trip across the int range
        int bad = 0 ;
        for (int p = 0 ; p <= 100 ; p++) {
            if (fxReturnPercent(fxReturnFromPercent(p)) != p) bad++ ;
        }
        check(bad == 0, "all 0..100 round trip") ;
    }

    // ---- Purity guards: no app/GUI/audio globals in the layer ----
    const char *forbidden[] = {
        "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
        "SoundSource", "DrawString", "GUITextProperties", "AppWindow",
        "FxEngine::GetInstance", "SetDelayReturn", "GetChannelPeakL"
    } ;
    // sanity: these are the actual dependency-free tokens; verify the header
    // text does not reference them
    FILE *fh = fopen("source/sources/Application/Mixer/FxPages.h", "r") ;
    check(fh != NULL, "FxPages.h readable") ;
    if (fh) {
        // read whole file
        fseek(fh, 0, SEEK_END) ;
        long sz = ftell(fh) ;
        fseek(fh, 0, SEEK_SET) ;
        char *buf = new char[sz + 1] ;
        size_t rd = fread(buf, 1, sz, fh) ;
        buf[rd] = 0 ;
        fclose(fh) ;
        for (size_t i = 0 ; i < sizeof(forbidden) / sizeof(forbidden[0]) ; i++) {
            char needle[128] ;
            sprintf(needle, "%s", forbidden[i]) ;
            check(strstr(buf, needle) == NULL, forbidden[i]) ;
        }
        delete[] buf ;
    }

    if (failures == 0) {
        printf("ALL OK (%d checks)\n", checks) ;
        return 0 ;
    }
    printf("%d/%d checks FAILED\n", failures, checks) ;
    return 1 ;
}
