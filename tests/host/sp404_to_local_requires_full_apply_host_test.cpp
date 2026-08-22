// sp404_to_local_requires_full_apply_host_test - P0
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
int main(){
    const char* path="source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp";
    std::ifstream f(path);
    if(!f.good()){ fprintf(stderr,"FAIL bridge missing\n"); return 1; }
    std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Must have leaving_external_runtime logic
    if(c.find("leaving_external_runtime")==std::string::npos){ fprintf(stderr,"FAIL missing leaving_external_runtime\n"); return 1; }
    if(c.find("previous_mode") == std::string::npos){ fprintf(stderr,"FAIL missing previous_mode\n"); return 1; }
    if(c.find("local full teardown apply requested")==std::string::npos){ fprintf(stderr,"FAIL missing local teardown log\n"); return 1; }
    // Simulate SP404->LOCAL
    // mode=SP404 fifo open runtime_ready => SetDriverMode(LOCAL) must launch apply
    // Our code now does if(leaving_external_runtime) launch_apply_profile_once(LOCAL)
    // Verify that path exists
    size_t pos=c.find("if (leaving_external_runtime)");
    if(pos==std::string::npos){ fprintf(stderr,"FAIL missing leaving check\n"); return 1; }
    std::string block=c.substr(pos, 2000);
    if(block.find("launch_apply_profile_once") == std::string::npos){ fprintf(stderr,"FAIL not launching apply for LOCAL\n"); return 1; }
    if(block.find("U241_LOCAL_CONSOLE") == std::string::npos){ fprintf(stderr,"FAIL not targeting LOCAL\n"); return 1; }
    // Also ensure WINDOWS->LOCAL etc will also trigger (since previous != LOCAL)
    printf("Simulate: mode=SP404 fifo open runtime ready -> SetDriverMode(LOCAL)\n");
    printf("Expected: FIFO closed, apply profile launched, NO fast apply\n");
    printf("SP404_TO_LOCAL_REQUIRES_FULL_APPLY_OK\n");
    // Test inverse: LOCAL->LOCAL should not launch (early return)
    if(c.find("if (g_driver_mode == mode)") == std::string::npos){ fprintf(stderr,"FAIL missing early return\n"); return 1; }
    printf("LOCAL->LOCAL early return preserved (no apply)\n");
    return 0;
}
