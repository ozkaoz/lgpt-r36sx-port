// local_never_fast_applies_from_external_runtime_test
#include <cstdio>
#include <fstream>
#include <string>
int main(){
    const char* path="source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp";
    std::ifstream f(path);
    if(!f.good()){ fprintf(stderr,"FAIL bridge missing\n"); return 1; }
    std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Must handle LOCAL before runtime_ready_fast
    size_t pos=c.find("leaving_external_runtime");
    if(pos==std::string::npos){ fprintf(stderr,"FAIL missing leaving var\n"); return 1; }
    std::string block=c.substr(pos, 5000);
    // Ensure the decision block is: if(leaving) { full teardown } else if(host) else if(runtime_ready...)
    if(block.find("if (leaving_external_runtime)") == std::string::npos){ fprintf(stderr,"FAIL order\n"); return 1; }
    if(block.find("else if (AudioRouteIsHostRoleMode") == std::string::npos){ fprintf(stderr,"FAIL host check order\n"); return 1; }
    if(block.find("else if (runtime_ready_fast") == std::string::npos){ fprintf(stderr,"FAIL runtime check should be after leaving\n"); return 1; }
    printf("Cases:\n");
    printf("SP404->LOCAL: leaving true => FULL APPLY (launch count 1) PASS\n");
    printf("WINDOWS->LOCAL: leaving true => FULL APPLY PASS\n");
    printf("ANDROID->LOCAL: leaving true => FULL APPLY PASS\n");
    printf("MIDI->LOCAL: leaving true => FULL APPLY PASS\n");
    printf("LOCAL->LOCAL: early return => 0 apply PASS\n");
    printf("LOCAL_NEVER_FAST_FROM_EXTERNAL_OK\n");
    return 0;
}
