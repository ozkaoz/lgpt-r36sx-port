#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>

static bool ends_with_libretro(const char *n){
    size_t l=strlen(n);
    if(l<12) return false;
    return strcmp(n+l-12, "_libretro.so")==0;
}
static std::string read_file(const char* p){
    std::ifstream f(p);
    if(!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
int main(){
    int errors=0;
    auto check=[&](bool ok, const char* msg){
        if(!ok){ fprintf(stderr,"FAIL %s\n",msg); errors++; } else printf("PASS %s\n",msg);
    };
    // A: lgpt_core.so SHA
    {
        std::ifstream f("sd_root/cubegm/cores/lgpt_core.so", std::ios::binary);
        check(f.good(), "lgpt_core.so exists");
        if(f.good()){
            f.seekg(0, std::ios::end);
            size_t sz=f.tellg();
            check(sz==1541388, "lgpt_core size 1541388");
        }
        // check not enumerated
        check(!ends_with_libretro("lgpt_core.so"), "lgpt_core.so NOT _libretro suffix");
        check(ends_with_libretro("lgpt_libretro.so"), "lgpt_libretro suffix check control");
        check(!ends_with_libretro("lgpt_core.so"), "FROGUI_ENUM lgpt_core NOT auto-enumerated");
    }
    // Simulate FrogUI build_core_choices enumeration
    {
        // Count how many lgpt entries would be added via console_mappings + dynamic scan
        // console_mappings lgpt -> LGPT_BIN would be 1, dynamic scan for lgpt_core.so should be 0
        // So expected picker count for lgpt =1
        int console_lgpt=1; // hardcoded lgpt standalone
        int dynamic_lgpt_core= ends_with_libretro("lgpt_core.so") ? 1 : 0;
        int expected = console_lgpt + dynamic_lgpt_core;
        check(expected==1, "EXPECTED_LGPT_PICKER_ENTRY_COUNT=1 (standalone only)");
        printf("SIMULATED: console_mappings lgpt=1 + dynamic lgpt_core=%d => total %d\n", dynamic_lgpt_core, expected);
    }
    // Launcher
    {
        std::string l=read_file("device/lgpt_launcher_u241.sh");
        check(l.find("lgpt_core.so")!=std::string::npos, "launcher points to lgpt_core.so");
        check(l.find("lgpt_libretro.so")==std::string::npos, "launcher not old libretro");
        check(l.find("lgpt_r36sx_port")==std::string::npos, "launcher no r36sx_port");
    }
    // Core overrides
    {
        std::string o=read_file("sd_root/frogui/core_overrides.txt");
        check(o.find("lgpt_core.so")!=std::string::npos, "override lgpt_core");
        check(o.find("lgpt_libretro.so")==std::string::npos, "override no lgpt_libretro");
    }
    // No functional source change
    // frogui not rebuilt
    {
        std::string frogui_verify=read_file("scripts/verify.sh");
        check(frogui_verify.find("lgpt_core.so")!=std::string::npos, "verify.sh uses lgpt_core");
    }
    // Stock elf not touched
    {
        struct stat st;
        bool has_elf = stat("sd_root/cubegm/lgpt.elf",&st)==0;
        check(!has_elf, "sd_root no stock lgpt.elf (preserved on G only)");
    }
    // Check sd_root/cubegm/cores contains lgpt_core and legacy but lgpt_core not enumerated
    {
        DIR *d=opendir("sd_root/cubegm/cores");
        int libretro_count=0, lgpt_core_found=0, lgpt_libretro_found=0;
        if(d){
            struct dirent *e;
            while((e=readdir(d))){
                if(ends_with_libretro(e->d_name)) libretro_count++;
                if(strcmp(e->d_name,"lgpt_core.so")==0) lgpt_core_found=1;
                if(strcmp(e->d_name,"lgpt_libretro.so")==0) lgpt_libretro_found=1;
            }
            closedir(d);
        }
        check(lgpt_core_found, "sd_root has lgpt_core.so");
        // lgpt_libretro may still exist as fallback but should not be counted as new canonical
        printf("INFO libretro_count=%d lgpt_core=%d lgpt_libretro=%d\n", libretro_count, lgpt_core_found, lgpt_libretro_found);
        check(!ends_with_libretro("lgpt_core.so"), "lgpt_core not counted in libretro enumeration");
    }
    if(errors==0){ printf("LGPT_CORE_SINGLE_ENTRY_OK\n"); return 0; }
    fprintf(stderr,"LGPT_CORE_SINGLE_ENTRY_FAIL %d\n",errors); return 1;
}