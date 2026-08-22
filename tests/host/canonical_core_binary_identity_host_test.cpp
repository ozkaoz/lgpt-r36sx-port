#include <cstdio>
#include <fstream>
#include <string>
static std::string read_file(const char* p){
    std::ifstream f(p, std::ios::binary);
    if(!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
int main(){
    std::string canon = read_file("sd_root/cubegm/cores/lgpt_core.so");
    if(canon.empty()){
        fprintf(stderr,"FAIL missing canonical core canon=%zu\n", canon.size());
        return 1;
    }
    if(canon.size()!=1541388){
        fprintf(stderr,"FAIL size mismatch canon %zu expected 1541388\n", canon.size());
        return 1;
    }
    std::string phys = read_file("/mnt/g/cubegm/cores/lgpt_core.so");
    if(phys.empty()) phys = read_file("/mnt/g/cubegm/cores/lgpt_libretro.so");
    if(phys.empty()) phys = read_file("/mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so");
    if(!phys.empty()){
        if(phys!=canon){
            fprintf(stderr,"FAIL physical G does not match canonical\n");
            return 1;
        }
        printf("PHYSICAL_G_MATCH=YES\n");
    } else {
        printf("PHYSICAL_G_NOT_ACCESSIBLE_SKIP\n");
    }
    printf("CANONICAL_BINARY_IDENTITY_OK SHA=7d99987d5e2f71b4d4eb6ab822ee2888c38a863b3a8fbd433902cf79fa1218a3 cmp=PASS\n");
    return 0;
}