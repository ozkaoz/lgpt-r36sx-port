// sp404_peak_fused_bitexact_host_test - verifies P1 fused peak bit-exact vs legacy separate scan
#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#include <cstring>
#include <cmath>

// Legacy: separate scan after render/gain
static int legacy_peak(const int16_t* out, unsigned samples){
    int local_peak=0;
    for(unsigned i=0;i<samples;++i){
        int v=out[i];
        if(v<0) v=-v;
        if(v>local_peak) local_peak=v;
    }
    return local_peak;
}
// Fused with gain==1 (just scan)
static int fused_gain1(const int16_t* out, unsigned samples){
    int local=0;
    for(unsigned g=0; g<samples; ++g){
        int av=out[g];
        if(av<0) av=-av;
        if(av>local) local=av;
    }
    return local;
}
// Fused with gain!=1 (scale + peak in one loop)
static int fused_gain_scaled(const int16_t* in, unsigned samples, float gain, int16_t* out){
    int local=0;
    for(unsigned g=0; g<samples; ++g){
        int16_t v=(int16_t)((float)in[g]*gain);
        out[g]=v;
        int av=v;
        if(av<0) av=-av;
        if(av>local) local=av;
    }
    return local;
}

static bool test_one(unsigned frames, float gain){
    unsigned samples=frames*2u;
    std::vector<int16_t> src(samples), rendered(samples), legacy_out(samples), fused_out(samples);
    std::mt19937 rng(0x9ABC+frames+(int)(gain*100));
    for(unsigned i=0;i<samples;++i){
        int r=rng()%65536-32768;
        // inject edge cases
        if(i%50==0) r=32767;
        else if(i%50==1) r=-32768;
        else if(i%50==2) r=0;
        src[i]=(int16_t)r;
        rendered[i]=src[i]; // simulate ASRC output equals src for test
    }
    // Legacy path: gain then peak
    for(unsigned i=0;i<samples;++i){
        if(gain!=1.0f) legacy_out[i]=(int16_t)((float)rendered[i]*gain);
        else legacy_out[i]=rendered[i];
    }
    int leg_peak=legacy_peak(legacy_out.data(), samples);
    int leg_is_silence=(leg_peak==0)?1:0;

    // Fused path
    int fused_peak;
    if(gain!=1.0f){
        fused_peak=fused_gain_scaled(rendered.data(), samples, gain, fused_out.data());
        if(memcmp(legacy_out.data(), fused_out.data(), samples*sizeof(int16_t))!=0){
            fprintf(stderr,"FAIL gain %.2f frames %u output mismatch\n",gain,frames);
            return false;
        }
    } else {
        memcpy(fused_out.data(), rendered.data(), samples*sizeof(int16_t));
        fused_peak=fused_gain1(fused_out.data(), samples);
    }
    if(leg_peak!=fused_peak){
        fprintf(stderr,"FAIL peak gain %.2f frames %u leg %d fused %d\n",gain,frames,leg_peak,fused_peak);
        // debug first differing sample
        for(unsigned i=0;i<samples;++i){
            int av = legacy_out[i]<0?-legacy_out[i]:legacy_out[i];
            if(av==leg_peak) {fprintf(stderr," leg peak sample %u val %d\n",i,legacy_out[i]); break;}
        }
        return false;
    }
    int fused_silence=(fused_peak==0)?1:0;
    if(leg_is_silence!=fused_silence){
        fprintf(stderr,"FAIL silence flag gain %.2f frames %u\n",gain,frames);
        return false;
    }
    return true;
}

static bool test_int16_min(){
    // Specific test for -32768 -> 32768
    int16_t data[2]={-32768, -32768};
    int p1=legacy_peak(data,2);
    int p2=fused_gain1(data,2);
    if(p1!=32768 || p2!=32768){
        fprintf(stderr,"FAIL INT16_MIN peak leg %d fused %d expected 32768\n",p1,p2);
        return false;
    }
    // with gain scaling
    int16_t out[2];
    int p3=fused_gain_scaled(data,2,1.0f,out);
    if(p3!=32768){fprintf(stderr,"FAIL INT16_MIN scaled\n");return false;}
    // with gain 0.5, -32768*0.5 = -16384 -> peak 16384
    int16_t out2[2];
    int p4=fused_gain_scaled(data,2,0.5f,out2);
    int expected = (int16_t)((float)-32768*0.5f); // -16384
    int expPeak = expected<0?-expected:expected;
    if(p4!=expPeak){fprintf(stderr,"FAIL gain half peak %d vs %d\n",p4,expPeak);return false;}
    return true;
}

int main(){
    if(!test_int16_min()) return 1;
    unsigned frames_list[]={1,2,64,128,480,512,1024};
    float gains[]={1.0f, 0.85f, 0.5f, 2.0f, 0.0f};
    for(float g: gains){
        for(unsigned f: frames_list){
            if(!test_one(f,g)) return 1;
        }
        // random frames
        for(int i=0;i<20;++i){
            unsigned f=1+(rand()%1024);
            if(!test_one(f,g)) return 1;
        }
    }
    // Silence test
    {
        std::vector<int16_t> zero(960,0);
        int p1=legacy_peak(zero.data(), zero.size());
        int p2=fused_gain1(zero.data(), zero.size());
        if(p1!=0||p2!=0){fprintf(stderr,"FAIL silence peak\n");return 1;}
    }
    printf("SP404_PEAK_FUSED_BITEXACT_OK\n");
    printf("Tested gains 1.0 0.85 0.5 2.0 0.0 with frames 1..1024, INT16_MIN handling bit-identical\n");
    return 0;
}
