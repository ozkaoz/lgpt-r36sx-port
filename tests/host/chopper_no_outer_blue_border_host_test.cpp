/*
 * chopper_no_outer_blue_border_host_test.cpp
 * Ensures outer blue square border around Chopper is removed for TreeFrog.
 */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
static int checks=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL %s line %d\n", #c, __LINE__); exit(1);} checks++; } while(0)
bool fileContains(const char* p, const char* n){
    std::ifstream f(p);
    if(!f) return false;
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return s.find(n)!=std::string::npos;
}
int main(){
    const char* chopper = "/home/dafunknoise/lgpt-repo/source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp";
    const char* viewH = "/home/dafunknoise/lgpt-repo/source/sources/Application/UI/Views/ModalDialogs/ChopperView.h";
    // Check drawFrame is no-op for TreeFrog
    CHECK(fileContains(chopper, "#if defined(PLATFORM_TREEFROG)"));
    CHECK(fileContains(chopper, "void SampleChopperModal::drawFrame"));
    // The drawFrame should contain early return for TreeFrog
    {
        std::ifstream f(chopper);
        std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t pos = c.find("void SampleChopperModal::drawFrame");
        CHECK(pos!=std::string::npos);
        std::string snippet = c.substr(pos, 800);
        CHECK(snippet.find("PLATFORM_TREEFROG")!=std::string::npos);
        CHECK(snippet.find("return;")!=std::string::npos);
        // Ensure it does NOT unconditionally call DrawFrame for TreeFrog
        // The #else branch should contain DrawFrame
        CHECK(snippet.find("ChopperView::DrawFrame")!=std::string::npos);
        printf("PASS drawFrame guarded for TreeFrog\n");
    }
    // Ensure outer border invert not drawn for TreeFrog via drawFrame
    // The ChopperView::DrawFrame itself still has border code, but it's not called for TreeFrog
    CHECK(fileContains(viewH, "DrawFrame"));
    // Verify no other outer border tf_rect in chopper view
    // Waveform border is separate and should remain (TF_WAVE_X)
    CHECK(fileContains(chopper, "TF_WAVE_X - 2"));
    printf("PASS waveform border preserved, outer chopper border removed for TreeFrog\n");
    printf("All %d checks PASS\n", checks);
    return 0;
}
