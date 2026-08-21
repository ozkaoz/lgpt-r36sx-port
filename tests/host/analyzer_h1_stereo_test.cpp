#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Utils/fixed.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks=0, failures=0;
static void check(bool c,const char*w){ checks++; if(!c){failures++; printf("FAIL: %s\n",w);} }

static void feedStereo(SpectrumAnalyzer &sp, double hz, int ampL, int ampR, double phaseR) {
    const int kF = SpectrumAnalyzer::kRingFrames;
    fixed buf[2*kF];
    for(int i=0;i<kF;i++){
        double t = (double)i/48000.0;
        int cL = (int)(ampL * sin(2*M_PI*hz*t));
        int cR = (int)(ampR * sin(2*M_PI*hz*t + phaseR));
        fixed sL = i2fp(cL);
        fixed sR = i2fp(cR);
        buf[i*2]=sL; buf[i*2+1]=sR;
    }
    // Use FeedMix for master path (stereo)
    sp.SetArmed(true);
    sp.SetInstrumentTarget(0);
    sp.FeedMix(buf,kF);
}

static int logBarIndex(double hz){
    return (int)(log(hz/20.0)/log(20000.0/20.0)*(SpectrumAnalyzer::kLogBins-1)+0.5);
}

int main(){
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    // H1: antiphase 8kHz should still be visible
    printf("H1 antiphase test\n");
    // In-phase L/R
    feedStereo(sp,8000,10000,10000,0);
    sp.Compute();
    float vInPhase = fp2fl(sp.Bins()[logBarIndex(8000)]);
    printf("in-phase 8k bar %d val %.4f\n",logBarIndex(8000),vInPhase);
    check(vInPhase > 0.1f,"in-phase 8k visible");

    // Antiphase L=+sin R=-sin
    feedStereo(sp,8000,10000,10000,3.14159265);
    sp.Compute();
    float vAnti = fp2fl(sp.Bins()[logBarIndex(8000)]);
    printf("antiphase 8k bar val %.4f (in-phase %.4f)\n",vAnti,vInPhase);
    // With mono sum, antiphase cancels -> vAnti ~0, should be comparable to in-phase
    check(vAnti > 0.1f,"antiphase 8k still visible (no mono cancel)");
    check(fabsf(vAnti - vInPhase) < 0.2f,"antiphase comparable to in-phase");

    // L only
    feedStereo(sp,8000,10000,0,0);
    sp.Compute();
    float vL = fp2fl(sp.Bins()[logBarIndex(8000)]);
    printf("L only 8k val %.4f\n",vL);
    check(vL > 0.05f,"L only visible");

    // R only
    feedStereo(sp,8000,0,10000,0);
    sp.Compute();
    float vR = fp2fl(sp.Bins()[logBarIndex(8000)]);
    printf("R only 8k val %.4f\n",vR);
    check(vR > 0.05f,"R only visible");

    // Partial phase 90deg
    feedStereo(sp,8000,10000,10000,1.570796);
    sp.Compute();
    float v90 = fp2fl(sp.Bins()[logBarIndex(8000)]);
    printf("90deg 8k val %.4f\n",v90);
    check(v90 > 0.05f,"90deg visible");

    printf("H1: %d checks %d failures\n",checks,failures);
    return failures?1:0;
}
