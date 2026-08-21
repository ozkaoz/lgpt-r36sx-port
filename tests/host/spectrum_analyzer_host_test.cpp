// SpectrumAnalyzer host test - new spec (exclusive log intervals, Blackman 2/sum, no visGain, no x4)
#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Utils/fixed.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}
static int logBarIndex(double hz) {
    double r = log(hz / 20.0) / log(20000.0 / 20.0) * (SpectrumAnalyzer::kLogBins - 1);
    return (int)(r + 0.5);
}
static void feedSine(float hz, int ampCounts) {
    const int kF = SpectrumAnalyzer::kRingFrames;
    fixed buf[2*kF];
    for (int i=0;i<kF;i++) {
        float v = (float)ampCounts * sinf(2.0f*3.14159265f*hz*i/48000.0f);
        fixed s = ((fixed)(long long)v) << 15;
        buf[i*2]=s; buf[i*2+1]=s;
    }
    SpectrumAnalyzer::Get().SetArmed(true);
    SpectrumAnalyzer::Get().FeedMix(buf,kF);
}
static void feedDC(int ampCounts) {
    const int kF = SpectrumAnalyzer::kRingFrames;
    fixed buf[2*kF];
    fixed s = ((fixed)ampCounts) << 15;
    for(int i=0;i<kF;i++){ buf[i*2]=s; buf[i*2+1]=s; }
    SpectrumAnalyzer::Get().SetArmed(true);
    SpectrumAnalyzer::Get().FeedMix(buf,kF);
}
static void oneShot(float hz,int amp){ feedSine(hz,amp); SpectrumAnalyzer::Get().Compute(); }

int main(){
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    // 1. silence
    sp.SetArmed(true);
    static fixed silence[SpectrumAnalyzer::kRingFrames*2];
    memset(silence,0,sizeof(silence));
    sp.FeedMix(silence, SpectrumAnalyzer::kRingFrames);
    check(sp.Compute(),"compute silence");
    bool allZero=true;
    for(int i=0;i<sp.BinCount();i++) if(fp2fl(sp.Bins()[i])>0.001f) allZero=false;
    check(allZero,"silence all bins 0");
    // 2. 984.375 Hz exact bin (bin 336) 0 dBFS
    oneShot(984.375f,32767);
    {
        int bar = logBarIndex(984.375);
        // Find max bar
        int peakBar=0; float peakVal=0;
        for(int i=0;i<sp.BinCount();i++){ float v=fp2fl(sp.Bins()[i]); if(v>peakVal){peakVal=v; peakBar=i;} }
        printf("984Hz 0dB: peakBar=%d expect=%d val=%.4f\n",peakBar,bar,peakVal);
        check(abs(peakBar - bar) <= 1,"984Hz max visual in real bin log");
        check(peakVal > 0.95f && peakVal < 1.05f,"984Hz 0dB amplitude 0.95-1.0");
        // width above -6dB (0.5 * peak) max 3 pixels
        int width=0;
        for(int i=0;i<sp.BinCount();i++){ if(fp2fl(sp.Bins()[i]) > peakVal*0.5f) width++; }
        printf("984Hz width -6dB = %d\n",width);
        check(width <= 3,"984Hz width above -6dB max 3 pixels");
        // no plateau of 28 bars: check that bar+14 is low
        float plateau = fp2fl(sp.Bins()[bar+14 < sp.BinCount() ? bar+14 : bar]);
        check(plateau < 0.1f, "no 28-bar plateau");
        // far bars low
        check(fp2fl(sp.Bins()[0]) < 0.02f && fp2fl(sp.Bins()[logBarIndex(16000)]) < 0.02f,"984Hz far bars low");
        // PeakFrequency
        float pf = sp.PeakFrequency();
        printf("PeakFrequency %.2f\n",pf);
        check(fabsf(pf - 984.375f) < 3.0f,"PeakFrequency <3Hz");
    }
    // 3. same tone amp 0.25 (8192)
    oneShot(984.375f,8192);
    {
        float v = fp2fl(sp.Bins()[logBarIndex(984.375)]);
        printf("984Hz 0.25 amp bar=%.4f\n",v);
        check(v > 0.24f && v < 0.26f,"984Hz amp 0.25 reading 0.24-0.26");
    }
    // 4. sweep
    double freqs[]={30,40,60,80,100,200,440,1000,2500,5000,10000,16000,19000};
    for(int f=0;f<13;f++){
        double hz=freqs[f];
        oneShot((float)hz,32767);
        int expect = logBarIndex(hz);
        int peakBar=0; float peakVal=0;
        for(int i=0;i<sp.BinCount();i++){ float v=fp2fl(sp.Bins()[i]); if(v>peakVal){peakVal=v; peakBar=i;}}
        printf("sweep %.0f expect %d peak %d val %.3f pf %.1f\n",hz,expect,peakBar,peakVal,sp.PeakFrequency());
        check(abs(peakBar - expect) <= 1,"sweep max visual +-1 pixel");
        check(fabsf(sp.PeakFrequency() - (float)hz) < 3.0f,"sweep PeakFrequency <3Hz");
        // -6dBFS amp 0.5 should be 0.35-0.55? Actually -6dBFS is 0.5 amp -> 0.5*1.0 =0.5, but with Blackman maybe 0.35-0.55
        // We'll test amplitude 0.5 counts 16384
        feedSine((float)hz,16384);
        sp.Compute();
        float v6 = fp2fl(sp.Bins()[peakBar]);
        // For 0.5 amp, expect ~0.5
        check(v6 > 0.35f && v6 < 0.55f,"sweep -6dBFS 0.35-0.55");
    }
    // 5. DC
    feedDC(4300);
    sp.Compute();
    {
        float worst=0; for(int i=0;i<sp.BinCount();i++){ float v=fp2fl(sp.Bins()[i]); if(v>worst) worst=v; }
        printf("DC worst %.4f\n",worst);
        check(worst < 0.01f,"DC bins ~0");
    }
    // 6. 1ms pulse
    {
        const int kF = SpectrumAnalyzer::kRingFrames;
        fixed buf[2*kF];
        memset(buf,0,sizeof(buf));
        fixed s = ((fixed)32767)<<15;
        for(int i=8168;i<8216;i++){ buf[i*2]=s; buf[i*2+1]=s; }
        sp.SetArmed(true);
        sp.FeedMix(buf,kF);
        sp.Compute();
        float worst=0; for(int i=0;i<sp.BinCount();i++){ float v=fp2fl(sp.Bins()[i]); if(v>worst) worst=v; }
        printf("1ms pulse worst %.4f\n",worst);
        check(worst < 0.2f,"1ms pulse not full scale");
    }
    printf("spectrum_analyzer: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
