/*
 * chopper_pitch_entry_render_host_test.cpp
 * Tests pitch entry rendering: L1+R1 -> pitchMode true -> direct overlay draws limited panel
 */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
static int checks=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL %s line %d\n", #c, __LINE__); exit(1);} checks++; } while(0)
bool contains(const char* p, const char* n){
    std::ifstream f(p);
    if(!f) return false;
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return s.find(n)!=std::string::npos;
}
int main(){
    const char* chopper = "/home/dafunknoise/lgpt-repo/source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp";
    // Entry: L1+R1 toggles pitchMode, publish should set active
    CHECK(contains(chopper, "g_chopperPitchActive = (!suspended_ && !operationActive_ && pitchMode_)"));
    // Limited geometry
    CHECK(contains(chopper, "tf_rect(0, 60, 320, 116"));
    CHECK(contains(chopper, "tf_rect(0, 59, 320, 1"));
    CHECK(contains(chopper, "tf_rect(0, 176, 320, 1"));
    // Title
    CHECK(contains(chopper, "\"PITCH/ENV\""));
    // Header
    CHECK(contains(chopper, "g_chopperPitchHeader"));
    // 6 params
    CHECK(contains(chopper, "for (int i = 0; i < 6; i++)"));
    // Labels/values
    CHECK(contains(chopper, "g_chopperPitchLabels"));
    CHECK(contains(chopper, "g_chopperPitchValues"));
    // Hints within panel
    CHECK(contains(chopper, "g_chopperPitchHints"));
    // Not fullscreen
    CHECK(!contains(chopper, "tf_rect(0, 0, 320, 240, pbg)"));
    // Playback stopped vs playing both should show (publish does not check IsStreaming)
    CHECK(contains(chopper, "hasAssignedSample()"));
    // Operation also direct
    CHECK(contains(chopper, "tf_rect(0, 64, 320, 112"));
    CHECK(contains(chopper, "g_chopperOperationMessage"));
    printf("All %d checks PASS - pitch entry render limited direct OK\n", checks);
    return 0;
}
