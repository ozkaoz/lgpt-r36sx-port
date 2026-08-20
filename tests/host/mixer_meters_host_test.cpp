// F3-4b: MixerMeters pure layer golden host test.
// Verifies the VU smoothing (attack/release/stop-reset), the bar level
// derive and the half-cell bar geometry against hand-computed oracles,
// plus purity guards (no GUI/audio/Player/framebuffer dependencies).
#include "Application/Mixer/MixerMeters.h"
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

static float closeOr(float a, float b) {
    return fabsf(a - b) < 0.0001f ;
}

int main() {
    MixerMeters m ;
    float peakL[MixerMeters::kChannels] ;
    float peakR[MixerMeters::kChannels] ;
    for (int i = 0 ; i < MixerMeters::kChannels ; i++) {
        peakL[i] = 0.0f ;
        peakR[i] = 0.0f ;
    }

    // ---- Default state ----
    for (int i = 0 ; i < MixerMeters::kChannels ; i++) {
        check(m.LevelL(i) == 0.0f, "fresh levelL 0") ;
        check(m.LevelR(i) == 0.0f, "fresh levelR 0") ;
    }

    // ---- Attack is instant ----
    peakL[0] = 0.8f ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(0), 0.8f), "attack L instant") ;
    check(closeOr(m.LevelR(0), 0.0f), "no R without peak") ;
    // attack straight to 1.0
    peakL[0] = 1.0f ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(0), 1.0f), "attack to 1.0") ;

    // ---- Release is a per-frame *0.6 fall, floored at 0.001 ----
    peakL[0] = 0.0f ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(0), 0.6f), "release 1: *0.6") ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(0), 0.36f), "release 2: *0.6 again") ;
    // floor: 0.001 -> exactly 0 next frame
    for (int i = 0 ; i < 20 ; i++) m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(m.LevelL(0) == 0.0f, "release floors to exactly 0") ;
    // 0.6^12 empties a full bar
    peakL[0] = 1.0f ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    peakL[0] = 0.0f ;
    for (int i = 0 ; i < 12 ; i++) m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(0), powf(0.6f, 12.0f)), "release after 12 frames = 0.6^12") ;

    // ---- Stereo independence ----
    peakL[1] = 0.5f ;
    peakR[1] = 0.9f ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(1), 0.5f), "channel1 L independent") ;
    check(closeOr(m.LevelR(1), 0.9f), "channel1 R independent") ;

    // ---- Stop-reset: stopped transport samples 0, decay to 0 ----
    peakL[2] = 0.7f ;
    peakR[2] = 0.7f ;
    m.SmoothFrame(true, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(2), 0.7f), "pre-stop level") ;
    m.SmoothFrame(false, MixerMeters::kChannels, peakL, peakR) ;
    check(closeOr(m.LevelL(2), 0.42f), "stop decays *0.6") ;
    for (int i = 0 ; i < 20 ; i++) m.SmoothFrame(false, MixerMeters::kChannels, peakL, peakR) ;
    check(m.LevelL(2) == 0.0f && m.LevelR(2) == 0.0f, "stop resets to 0") ;

    // ---- BarLevel (BACON_1.5_VU_TOP0DB, U2.59): the level is the dB
    // position of the true linear peak over -24..0 dBFS
    // ((20*log10(peak)+24)/24, clamped 0..1 -- the scanned peaks already
    // include their fader (post-volume channel scan, pre-scan master
    // damp), so the old *volume/100 double-applied it (a volume-20 track
    // read ~87% + red).  The volume param stays for API stability but is
    // ignored.
    check(m.BarLevel(0.0f, 100) == 0.0f, "BarLevel 0 peak -> 0") ;
    // 0 dBFS = the top of the bar (the full bar), the same 0 dB reference
    // other consoles/DAWs use; only a pre-clip sum over 0 dBFS fills the
    // red top cell (the clip lamp).
    check(closeOr(m.BarLevel(1.0f, 100), 1.0f), "BarLevel 0dBFS -> full bar") ;
    // -6.02 dB -> (-6.02+24)/24 = 0.7492
    check(closeOr(m.BarLevel(0.5f, 50), 0.7492f), "BarLevel 0.5 -> 0.749 (dB)") ;
    // -13.98 dB (volume-20 track on a full-scale instrument) -> 10.02/24 = 0.4175
    check(closeOr(m.BarLevel(0.2f, 100), 0.4175f), "BarLevel vol-20 track reads 42%") ;
    check(m.BarLevel(10.0f, 100) == 1.0f, "BarLevel over 0 dBFS clamps to 1 (clip)") ;
    check(closeOr(m.BarLevel(1.0f, 0), 1.0f), "BarLevel ignores the volume param") ;
    check(m.BarLevel(-1.0f, 100) == 0.0f, "BarLevel negative peak -> 0") ;

    // ---- GeometryFor golden: height 12 cells ----
    {
        MixerMeters::Geometry g = MixerMeters::GeometryFor(12, 0.5f, 0.25f) ;
        check(g.totalPx == 96, "G totalPx 96") ;
        check(g.totalLevels == 32, "G totalLevels 32") ;
        // BACON_1.5_VU_TOP0DB (U2.59): the red band is the TOP CELL ONLY
        // (0 dBFS = top of the bar, the clip lamp): 32 - (int)(1.0*32+0.5)
        // = 32 - 32 = 0 -> forced to the min 1 level (the top 3-px cell).
        check(g.redBandLevels == 1, "G redBandLevels 1 (0 dBFS top cell)") ;
        check(g.redBandPx == 3, "G redBandPx 3") ;
        // round(0.5*32)=16 levels -> 16
        check(g.filledLLevels == 16, "G filledLLevels 16") ;
        // round(0.25*32)=8
        check(g.filledRLevels == 8, "G filledRLevels 8") ;
    }
    // full bar clamps at totalPx
    {
        MixerMeters::Geometry g = MixerMeters::GeometryFor(12, 1.0f, 1.0f) ;
        check(g.filledLLevels == 32, "G full L = 32 levels") ;
        check(g.filledRLevels == 32, "G full R = 32 levels") ;
    }
    // zero level
    {
        MixerMeters::Geometry g = MixerMeters::GeometryFor(12, 0.0f, 0.0f) ;
        check(g.filledLLevels == 0 && g.filledRLevels == 0, "G zero fill") ;
    }
    // tiny bar (height 1): 8 px -> 2 levels, red band min 1
    {
        MixerMeters::Geometry g = MixerMeters::GeometryFor(1, 1.0f, 1.0f) ;
        check(g.totalPx == 8 && g.totalLevels == 2, "G height1: 8px/2 levels") ;
        // 2 - round(1.0*2)=2-2=0 -> min 1 (0 dBFS top cell)
        check(g.redBandLevels == 1, "G height1 redBand min 1") ;
        check(g.redBandPx == 3, "G height1 redBandPx 3") ;
    }
    // degenerate height 0 -> zeroed geometry
    {
        MixerMeters::Geometry g = MixerMeters::GeometryFor(0, 0.5f, 0.5f) ;
        check(g.totalLevels == 0 && g.redBandPx == 0 && g.filledLLevels == 0, "G height0 zeroed") ;
    }

    // ---- RowStateFor golden on the 96px/32-level bar ----
    {
        // redBandPx 6, filledL 16 levels, filledR 8 levels
        MixerMeters::RowState r0 = MixerMeters::RowStateFor(0, 96, 6, 16, 8) ;
        check(r0.inBand && !r0.gapRow, "row0 in red band, no gap") ;
        MixerMeters::RowState r5 = MixerMeters::RowStateFor(5, 96, 6, 16, 8) ;
        check(r5.inBand && !r5.gapRow, "row5 still in band") ;
        MixerMeters::RowState r6 = MixerMeters::RowStateFor(6, 96, 6, 16, 8) ;
        check(!r6.inBand && r6.gapRow, "row6 first gap row outside band") ;
        MixerMeters::RowState r7 = MixerMeters::RowStateFor(7, 96, 6, 16, 8) ;
        check(!r7.gapRow, "row7 no gap") ;
        // levelIdx=(96-1-row)/3 ; filledL 16 -> levelIdx 15 is the last
        // filled level -> row >= 48 (95-48)/3=15; row 47 -> (95-47)/3=16
        MixerMeters::RowState r47 = MixerMeters::RowStateFor(47, 96, 6, 16, 8) ;
        check(!r47.fillL, "row47 levelIdx 16 -> not L fill") ;
        MixerMeters::RowState r48 = MixerMeters::RowStateFor(48, 96, 6, 16, 8) ;
        check(r48.fillL, "row48 levelIdx 15 -> L fill") ;
        MixerMeters::RowState r49 = MixerMeters::RowStateFor(49, 96, 6, 16, 8) ;
        check(r49.fillL, "row49 levelIdx 15 -> L fill") ;
        // filledR 8 -> fill until levelIdx 7 -> row >= 95-24+1=72 (levelIdx=(95-row)/3 <8 -> 95-row<=23 -> row>=72)
        MixerMeters::RowState r71 = MixerMeters::RowStateFor(71, 96, 6, 16, 8) ;
        check(!r71.fillR, "row71 levelIdx 8 -> not R fill") ;
        MixerMeters::RowState r72 = MixerMeters::RowStateFor(72, 96, 6, 16, 8) ;
        check(r72.fillR, "row72 levelIdx 7 -> R fill") ;
        // gap rows every 3rd row outside band: 6,9,12...
        MixerMeters::RowState r9 = MixerMeters::RowStateFor(9, 96, 6, 16, 8) ;
        check(r9.gapRow, "row9 gap") ;
        MixerMeters::RowState r10 = MixerMeters::RowStateFor(10, 96, 6, 16, 8) ;
        check(!r10.gapRow, "row10 no gap") ;
        MixerMeters::RowState r93 = MixerMeters::RowStateFor(93, 96, 6, 16, 8) ;
        check(r93.fillL && r93.fillR, "row93 bottom fill both") ;
    }

    // ---- Purity guards ----
    const char *forbidden[] = {
        "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
        "SoundSource", "DrawString", "GUITextProperties", "AppWindow",
        "GetChannelPeakL", "TreeFrogGetFramebuffer", "ColorDefinition",
        "ResolveColor565"
    } ;
    FILE *fh = fopen("source/sources/Application/Mixer/MixerMeters.h", "r") ;
    check(fh != NULL, "MixerMeters.h readable") ;
    if (fh) {
        fseek(fh, 0, SEEK_END) ;
        long sz = ftell(fh) ;
        fseek(fh, 0, SEEK_SET) ;
        char *buf = new char[sz + 1] ;
        size_t rd = fread(buf, 1, sz, fh) ;
        buf[rd] = 0 ;
        fclose(fh) ;
        for (size_t i = 0 ; i < sizeof(forbidden) / sizeof(forbidden[0]) ; i++) {
            check(strstr(buf, forbidden[i]) == NULL, forbidden[i]) ;
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
