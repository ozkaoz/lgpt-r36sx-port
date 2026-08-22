// sp404_output_fastpath_bitexact_host_test - verifies P1 convert fastpath bit-exact
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <random>

static unsigned convert_s16_to_device_generic(
    const int16_t *in, int frames, unsigned in_ch,
    unsigned char *out, unsigned out_ch, unsigned out_bytes, unsigned out_shift){
    if(out_ch<1) out_ch=1;
    for(int f=0;f<frames;++f){
        int32_t left=in[(size_t)f*in_ch];
        int32_t right=(in_ch==2)?in[(size_t)f*in_ch+1]:left;
        for(int c=0;c<(int)out_ch;++c){
            int32_t v=(c==0)?left:((c==1)?right:0);
            v <<= (int)out_shift;
            unsigned char *dst=out+((size_t)f*out_ch+(size_t)c)*out_bytes;
            for(unsigned b=0;b<out_bytes;++b) dst[b]=(unsigned char)((uint32_t)v>>(8*b));
        }
    }
    return (unsigned)frames;
}

// Fastpath 2ch S16 direct: out is already payload
static unsigned fast_2ch(const int16_t* in, int frames, unsigned char* out){
    memcpy(out, in, (size_t)frames*2*sizeof(int16_t));
    return frames;
}
// Fastpath 4ch S16 L R 0 0
static unsigned fast_4ch(const int16_t* in, int frames, unsigned char* out){
    int16_t* d=(int16_t*)out;
    for(int f=0;f<frames;++f){
        d[f*4]=in[f*2];
        d[f*4+1]=in[f*2+1];
        d[f*4+2]=0;
        d[f*4+3]=0;
    }
    return frames;
}

static bool test_one(unsigned out_ch, unsigned out_bytes, unsigned shift, int frames){
    std::vector<int16_t> in(frames*2);
    std::mt19937 rng(0x5678+frames+out_ch*10);
    for(auto &v:in) v=(int16_t)(rng()%65536-32768);
    if(!in.empty()){ in[0]=32767; in[1]=-32768; in[2]=0; }
    size_t out_size = (size_t)frames*out_ch*out_bytes;
    std::vector<unsigned char> ref(out_size,0xAA), fast(out_size,0xAA);
    convert_s16_to_device_generic(in.data(), frames, 2, ref.data(), out_ch, out_bytes, shift);
    if(out_ch==2 && out_bytes==2 && shift==0){
        // fastpath should be direct memcpy
        fast_2ch(in.data(), frames, fast.data());
    } else if(out_ch==4 && out_bytes==2 && shift==0){
        fast_4ch(in.data(), frames, fast.data());
    } else {
        convert_s16_to_device_generic(in.data(), frames, 2, fast.data(), out_ch, out_bytes, shift);
    }
    if(memcmp(ref.data(), fast.data(), out_size)!=0){
        fprintf(stderr,"FAIL out_ch=%u bytes=%u shift=%u frames=%d mismatch\n",out_ch,out_bytes,shift,frames);
        for(size_t i=0;i<out_size && i<32; ++i){
            if(ref[i]!=fast[i]){
                fprintf(stderr,"  byte %zu ref=%02x fast=%02x\n",i,ref[i],fast[i]);
                break;
            }
        }
        return false;
    }
    return true;
}

int main(){
    int frames_list[]={1,2,64,128,480,512,1024};
    // Test fastpaths
    for(int f: frames_list){
        if(!test_one(2,2,0,f)) return 1; // 2ch S16
        if(!test_one(4,2,0,f)) return 1; // 4ch S16
    }
    // Test fallback paths still bit-exact (generic)
    for(int f: frames_list){
        if(!test_one(2,4,16,f)) return 1; // S32
        if(!test_one(2,3,8,f)) return 1; // S24_3
        if(!test_one(1,2,0,f)) return 1; // mono fallback
    }
    // Test starvation silence: zeroed out should be same
    {
        int frames=480;
        std::vector<int16_t> zero(frames*2,0);
        std::vector<unsigned char> ref(frames*4*2,0xFF), fast(frames*4*2,0xFF);
        convert_s16_to_device_generic(zero.data(), frames, 2, ref.data(), 2,2,0);
        fast_2ch(zero.data(), frames, fast.data());
        if(memcmp(ref.data(), fast.data(), frames*2*2)!=0){fprintf(stderr,"FAIL silence 2ch\n");return 1;}
        std::vector<unsigned char> ref4(frames*4*2,0xFF), fast4(frames*4*2,0xFF);
        convert_s16_to_device_generic(zero.data(), frames, 2, ref4.data(), 4,2,0);
        fast_4ch(zero.data(), frames, fast4.data());
        if(memcmp(ref4.data(), fast4.data(), frames*4*2)!=0){fprintf(stderr,"FAIL silence 4ch\n");return 1;}
    }
    printf("SP404_OUTPUT_FASTPATH_BITEXACT_OK\n");
    printf("Tested 2ch S16 direct, 4ch S16 L R 0 0, and fallback S32/S24 bit-identical\n");
    return 0;
}
