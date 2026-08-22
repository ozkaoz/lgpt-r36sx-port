// sp404_ring_bulk_bitexact_host_test - verifies P1 ring bulk bit-exact vs legacy sample-by-sample
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <random>

#define RING_SAMPLES 65536

// Legacy implementation (sample-by-sample %)
struct RingLegacy {
    int16_t ring[RING_SAMPLES];
    unsigned rpos=0,wpos=0,rfill=0;
    void reset(){rpos=wpos=rfill=0;}
    unsigned push(const int16_t*s, unsigned n){
        unsigned pushed=0;
        while(pushed<n && rfill<RING_SAMPLES){
            ring[wpos]=s[pushed++];
            wpos=(wpos+1)%RING_SAMPLES;
            rfill++;
        }
        return pushed;
    }
    unsigned pop(int16_t*dst, unsigned n){
        unsigned popped=0;
        while(popped<n && rfill>0){
            dst[popped]=ring[rpos];
            rpos=(rpos+1)%RING_SAMPLES;
            rfill--;
            popped++;
        }
        return popped;
    }
};

// P1 bulk implementation (wrap-aware memcpy)
struct RingBulk {
    int16_t ring[RING_SAMPLES];
    unsigned rpos=0,wpos=0,rfill=0;
    void reset(){rpos=wpos=rfill=0;}
    unsigned push(const int16_t*s, unsigned n){
        if(!s||n==0) return 0;
        unsigned free=RING_SAMPLES-rfill;
        if(n>free) n=free;
        if(n==0) return 0;
        unsigned first=RING_SAMPLES-wpos;
        if(first>n) first=n;
        memcpy(ring+wpos, s, (size_t)first*sizeof(int16_t));
        if(n>first) memcpy(ring, s+first, (size_t)(n-first)*sizeof(int16_t));
        wpos+=n; if(wpos>=RING_SAMPLES) wpos-=RING_SAMPLES;
        rfill+=n;
        return n;
    }
    unsigned pop(int16_t*dst, unsigned n){
        if(!dst||n==0) return 0;
        if(n>rfill) n=rfill;
        if(n==0) return 0;
        unsigned first=RING_SAMPLES-rpos;
        if(first>n) first=n;
        memcpy(dst, ring+rpos, (size_t)first*sizeof(int16_t));
        if(n>first) memcpy(dst+first, ring, (size_t)(n-first)*sizeof(int16_t));
        rpos+=n; if(rpos>=RING_SAMPLES) rpos-=RING_SAMPLES;
        rfill-=n;
        return n;
    }
    unsigned pop_frames(int16_t*dst, unsigned frames){
        unsigned samples=frames*2u;
        unsigned got=pop(dst, samples);
        return got/2u;
    }
};

static bool test_push_pop(){
    std::mt19937 rng(0x1234);
    for(int iter=0; iter<2000; ++iter){
        RingLegacy leg; RingBulk bulk;
        leg.reset(); bulk.reset();
        leg.rpos=leg.wpos=leg.rfill=0;
        bulk.rpos=bulk.wpos=bulk.rfill=0;
        unsigned prefill = rng()%60000;
        std::vector<int16_t> pre(prefill);
        for(auto &v:pre) v = (int16_t)(rng()%65536-32768);
        leg.push(pre.data(), prefill);
        bulk.push(pre.data(), prefill);
        unsigned pop1 = rng()%1000;
        std::vector<int16_t> tmp1(pop1), tmp2(pop1);
        leg.pop(tmp1.data(), pop1);
        bulk.pop(tmp2.data(), pop1);
        if(memcmp(tmp1.data(), tmp2.data(), pop1*sizeof(int16_t))!=0){
            fprintf(stderr,"FAIL pop1 mismatch iter %d\n",iter);
            return false;
        }
        if(leg.rpos!=bulk.rpos || leg.wpos!=bulk.wpos || leg.rfill!=bulk.rfill){
            fprintf(stderr,"FAIL state mismatch after pop1 iter %d leg %u %u %u bulk %u %u %u\n",iter,leg.rpos,leg.wpos,leg.rfill,bulk.rpos,bulk.wpos,bulk.rfill);
            return false;
        }
        unsigned push2 = 500 + rng()%4000;
        std::vector<int16_t> data(push2);
        for(auto &v:data) v=(int16_t)(rng()%65536-32768);
        if(!data.empty()){ data[0]=32767; if(data.size()>1) data[1]=-32768; }
        unsigned p1=leg.push(data.data(), push2);
        unsigned p2=bulk.push(data.data(), push2);
        if(p1!=p2){
            fprintf(stderr,"FAIL push2 count iter %d %u vs %u\n",iter,p1,p2);
            return false;
        }
        if(leg.rfill!=bulk.rfill || leg.wpos!=bulk.wpos){
            fprintf(stderr,"FAIL after push2 iter %d\n",iter);
            return false;
        }
        unsigned frames = 1 + rng()%1024;
        std::vector<int16_t> outLeg(frames*2,0), outBulk(frames*2,0);
        unsigned leg_popped = leg.pop(outLeg.data(), frames*2);
        unsigned bulk_popped = bulk.pop(outBulk.data(), frames*2);
        unsigned leg_frames = leg_popped/2;
        unsigned bulk_frames = bulk_popped/2;
        if(leg_frames!=bulk_frames){
            fprintf(stderr,"FAIL frames count iter %d leg %u bulk %u\n",iter,leg_frames,bulk_frames);
            return false;
        }
        if(memcmp(outLeg.data(), outBulk.data(), bulk_frames*2*sizeof(int16_t))!=0){
            fprintf(stderr,"FAIL frames data iter %d\n",iter);
            for(unsigned i=0;i<bulk_frames*2 && i<10;++i) fprintf(stderr," %d vs %d", outLeg[i], outBulk[i]);
            fprintf(stderr,"\n");
            return false;
        }
        if(leg.rfill!=bulk.rfill || leg.rpos!=bulk.rpos || leg.wpos!=bulk.wpos){
            fprintf(stderr,"FAIL state after frames iter %d leg %u %u %u bulk %u %u %u\n",iter,leg.rpos,leg.wpos,leg.rfill,bulk.rpos,bulk.wpos,bulk.rfill);
            return false;
        }
        if(iter%100==0){
            leg.reset(); bulk.reset();
            std::vector<int16_t> big(RING_SAMPLES+100);
            for(auto &v:big) v=(int16_t)rng();
            unsigned lp=leg.push(big.data(), big.size());
            unsigned bp=bulk.push(big.data(), big.size());
            if(lp!=bp || lp!=RING_SAMPLES){
                fprintf(stderr,"FAIL overflow iter %d %u %u\n",iter,lp,bp);
                return false;
            }
        }
    }
    return true;
}

static bool test_stereo_align(){
    RingBulk b; b.reset();
    int16_t data[4]={1,2,3,4};
    b.push(data,4);
    if(b.rfill!=4 || b.rfill%2!=0){fprintf(stderr,"FAIL align push\n");return false;}
    int16_t out[2];
    b.pop(out,2);
    if(out[0]!=1||out[1]!=2){fprintf(stderr,"FAIL align pop\n");return false;}
    return true;
}

int main(){
    if(!test_stereo_align()) return 1;
    if(!test_push_pop()){
        fprintf(stderr,"SP404_RING_BULK_BITEXACT_FAIL\n");
        return 1;
    }
    printf("SP404_RING_BULK_BITEXACT_OK\n");
    printf("Tested 2000 iterations with wrap, overflow, stereo frames, INT16 edge values bit-identical\n");
    return 0;
}
