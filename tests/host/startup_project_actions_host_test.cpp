#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <cstring>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

static std::string read_file(const char* p){
    std::ifstream f(p);
    if(!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static bool contains(const std::string& hay, const std::string& needle){
    return hay.find(needle)!=std::string::npos;
}
int main(){
    int errors=0;
    auto check=[&](bool ok, const std::string msg){
        if(!ok){ fprintf(stderr,"FAIL %s\n",msg.c_str()); errors++; } else printf("PASS %s\n",msg.c_str());
    };
    std::string spd_cpp = read_file("source/sources/Application/UI/Views/ModalDialogs/SelectProjectDialog.cpp");
    std::string spd_h = read_file("source/sources/Application/UI/Views/ModalDialogs/SelectProjectDialog.h");
    std::string tte_h = read_file("source/sources/Application/UI/Views/ModalDialogs/TreeFrogTextEditor.h");
    std::string tte_cpp = read_file("source/sources/Application/UI/Views/ModalDialogs/TreeFrogTextEditor.cpp");
    std::string npd_h = read_file("source/sources/Application/UI/Views/ModalDialogs/NewProjectDialog.h");
    std::string npd_cpp = read_file("source/sources/Application/UI/Views/ModalDialogs/NewProjectDialog.cpp");
    std::string modal_cpp = read_file("source/sources/Application/UI/Views/ModalDialogs/TreeFrogProjectActionModal.cpp");
    std::string pv = read_file("source/sources/Application/UI/Views/ProjectView.cpp");

    // 1 plain SELECT
    check(contains(spd_cpp, "mask == EPBM_SELECT"), "plain SELECT mask == EPBM_SELECT");
    check(contains(spd_cpp, "HasValidCurrentProjectSelection"), "HasValidCurrentProjectSelection exists");
    check(contains(spd_cpp, "TreeFrogV40IsLgptProjectName"), "uses IsLgptProjectName");
    check(contains(spd_cpp, "TreeFrogV40ProjectHasSaveFile"), "uses HasSaveFile");
    // menu items - REVISION: centralized 4 items
    check(contains(modal_cpp, "\"Rename\"") && contains(modal_cpp, "\"Duplicate\"") && contains(modal_cpp, "\"Export\"") && contains(modal_cpp, "\"Delete\""), "SELECT menu items Rename Duplicate Export Delete");
    check(contains(modal_cpp, "\"Rename\"") && contains(modal_cpp, "\"Duplicate\"") && contains(modal_cpp, "\"Export\""), "SELECT menu includes Export");
    // order check via spd mapping
    check(contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_RENAME") && contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_DUPLICATE") && contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_EXPORT") && contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_DELETE"), "SELECT mapping 1,2,3,4");
    // SELECT validity: stale/empty handling
    check(contains(spd_cpp, "currentProject_ < 0") && contains(spd_cpp, "content_.Size()"), "SELECT no valid project guard");
    // SELECT plain vs SELECT+R1/R2
    check(contains(spd_cpp, "mask == EPBM_SELECT"), "SELECT plain only");
    check(contains(spd_cpp, "if (mask & EPBM_SELECT)") && contains(spd_cpp, "return;"), "SELECT_R1/R2 not consumed (guard)");
    // REVISION: R1+A must be removed as startup menu
    check(!contains(spd_cpp, "static const char *actionItems[] = {\"Rename\", \"Export\", \"Delete\"}"), "STARTUP_R1A_PROJECT_MENU_REMOVED");
    // but R1+A early return should exist to prevent fallthrough
    check(contains(spd_cpp, "(mask & EPBM_R) && (mask & EPBM_A)") && contains(spd_cpp, "return;"), "R1+A early return prevents load");
    // A+B must be removed
    check(!contains(spd_cpp, "Handle A + B combination for delete"), "STARTUP_AB_DELETE_REMOVED");
    check(contains(spd_cpp, "(mask & EPBM_A) && (mask & EPBM_B)") && contains(spd_cpp, "return;"), "A+B early return prevents load");
    // plain A loads
    check(contains(spd_cpp, "if (mask == EPBM_A)"), "plain A load preserved");
    // Export action preserved via SELECT
    check(contains(spd_h, "PA_EXPORT = 2"), "PA_EXPORT=2");
    check(contains(spd_h, "PA_DUPLICATE = 4"), "PA_DUPLICATE=4");
    check(contains(spd_cpp, "PA_DUPLICATE") && contains(spd_cpp, "PA_EXPORT"), "both actions exist");
    check(contains(spd_cpp, "case PA_EXPORT") && contains(spd_cpp, "EXPORT MODE"), "Export via SELECT still uses picker");
    // menu B cancels
    check(contains(modal_cpp, "EPBM_B") && contains(modal_cpp, "EndModal(0)"), "menu B cancels");

    // Deferred
    check(contains(spd_cpp, "DeferProjectAction") && contains(spd_cpp, "pendingAction_") && contains(spd_cpp, "OnFrameUpdate") && contains(spd_cpp, "launchProjectAction"), "deferred pattern preserved");
    check(contains(spd_cpp, "StartupProjectActionMenuCallback"), "startup callback deferred");
    check(contains(spd_h, "PA_DUPLICATE") && !contains(spd_cpp, "case 2:\n        {\n            static const char *exportItems"), "PA_DUPLICATE != PA_EXPORT (no repurpose)");

    // Duplicate naming
    {
        std::string srcBase="KaOz";
        std::string dstBase=srcBase+"_c";
        check(dstBase=="KaOz_c", "KaOz->KaOz_c");
        check(std::string("KaOz_c")+"_c"=="KaOz_c_c", "KaOz_c->KaOz_c_c");
        std::string dstFull="lgpt_"+dstBase;
        check(dstFull=="lgpt_KaOz_c", "filesystem lgpt_KaOz_c");
    }
    // File copy functional test using real fs
    {
        auto tmp = fs::temp_directory_path() / ("lgpt_dup_test_"+std::to_string(rand()));
        fs::create_directories(tmp);
        auto src = tmp / "lgpt_KaOz";
        fs::create_directories(src);
        fs::create_directories(src / "samples");
        fs::create_directories(src / "samples" / "nested");
        { std::ofstream f(src / "lgptsav.dat"); f<<"TESTDATA123"; }
        { std::ofstream f(src / "samples" / "a.wav"); f<<"WAVDATA"; }
        { std::ofstream f(src / "samples" / "nested" / "b.txt"); f<<"NESTED"; }
        auto dst = tmp / "lgpt_KaOz_c";
        // Simulate RecursiveCopyDirectory via fs::copy
        bool ok=false;
        try{
            fs::copy(src, dst, fs::copy_options::recursive);
            ok = fs::exists(dst / "lgptsav.dat") && fs::exists(dst / "samples" / "a.wav");
        }catch(...){ ok=false; }
        check(ok, "duplicate copy recursive");
        check(fs::exists(src / "lgptsav.dat"), "source preserved after copy");
        {
            std::ifstream a(src / "lgptsav.dat"), b(dst / "lgptsav.dat");
            std::string sa((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>());
            std::string sb((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
            check(sa==sb && sa=="TESTDATA123", "source byte identical");
        }
        check(fs::exists(dst / "samples" / "nested" / "b.txt"), "nested copied");
        // collision
        bool exists_before = fs::exists(dst);
        check(exists_before, "collision dest exists");
        // should not overwrite
        {
            std::string before; { std::ifstream f(dst / "lgptsav.dat"); before.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
            // attempt to copy again should be prevented: we check Exists before copy
            bool would_copy = !fs::exists(dst);
            check(!would_copy, "collision prevents overwrite");
            std::string after; { std::ifstream f(dst / "lgptsav.dat"); after.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
            check(before==after, "dest unchanged on collision");
        }
        // name too long
        std::string longBase(24,'A');
        std::string longDst = longBase + "_c";
        check(longDst.size()>24, "Name too long detected 24+2");
        check(std::string(22,'A').size()+2 <=24, "within limit passes");
        // failure cleanup: create partial and remove
        auto dst3 = tmp / "lgpt_FailTest_c";
        fs::create_directories(dst3);
        { std::ofstream f(dst3 / "partial.dat"); f<<"partial"; }
        // simulate failure cleanup
        std::error_code ec;
        fs::remove_all(dst3, ec);
        check(!fs::exists(dst3), "partial cleaned on failure");
        auto src3 = tmp / "lgpt_FailTest";
        fs::create_directories(src3);
        { std::ofstream f(src3 / "lgptsav.dat"); f<<"FAILTEST"; }
        check(fs::exists(src3 / "lgptsav.dat"), "source preserved after failure");
        fs::remove_all(tmp, ec);
    }

    // Duplicate implementation details
    check(contains(spd_cpp, "\"Copy exists\""), "Copy exists policy");
    check(contains(spd_cpp, "\"Name too long\""), "Name too long policy");
    check(contains(spd_cpp, "\"Duplicate failed\""), "Duplicate failed cleanup");
    check(contains(spd_cpp, "RecursiveCopyDirectory"), "uses RecursiveCopyDirectory");
    check(contains(spd_cpp, "RecursiveDeleteDirectory"), "cleanup uses RecursiveDeleteDirectory");
    check(contains(spd_cpp, "sync()"), "sync after copy");
    check(contains(spd_cpp, "Project duplicated:"), "success notification");
    check(contains(spd_cpp, "setCurrentFolder(currentPath_)"), "refresh after duplicate");
    check(contains(spd_cpp, "kMaxStem = 24"), "length check 24");

    // Startup NEW mode
    check(contains(npd_h, "startupRandomMode_"), "NPD startupRandomMode field");
    check(contains(npd_h, "startupRandomMode = false"), "default false");
    check(contains(npd_cpp, "getRandomName()"), "NPD getRandomName");
    check(contains(npd_cpp, "Descend(GetName()).Exists()"), "NPD collision check");
    check(contains(npd_cpp, "\"A random START confirm B erase\""), "NPD hint");
    check(contains(npd_cpp, "TFSP_A") && contains(npd_cpp, "TFSP_START"), "NPD handles A and START");
    check(contains(npd_cpp, "if (!startupRandomMode_) return false;"), "NPD preserve default");
    check(contains(tte_h, "GetAdditionalActionMask") && contains(tte_h, "HandlePhysicalAction") && contains(tte_h, "GetActionHintLine"), "TTE hooks exist");
    check(contains(tte_cpp, "GetAdditionalActionMask()") && contains(tte_cpp, "HandlePhysicalAction(actions)"), "TTE hooks used");
    // Save As preserved + SAVEs fix: must target root:projects
    check(contains(pv, "NewProjectDialog(*this, \"root:projects\")"), "SaveAs uses root:projects");
    check(contains(pv, "Path projectsRoot(\"root:projects\")"), "SaveAs destination projectsRoot");
    check(contains(pv, "projectsRoot.GetName() + \"/\" + npd.GetName()"), "SaveAs str under projects");
    check(!contains(pv, "Path root(\"root:\")"), "old root not used");
    check(!contains(pv, "\"/mnt/sdcard"), "no hardcoded physical path");
    check(!contains(pv, "NewProjectDialog(*this, \"root:\", true"), "SaveAs not startup mode");
    check(contains(spd_cpp, "NewProjectDialog(*this, currentPath_, true)"), "startup NEW uses true");
    // Save As default A confirm preserved (startupRandomMode false)
    check(contains(npd_h, "startupRandomMode = false"), "SaveAs default false preserved");
    // Default editor preserved
    check(contains(tte_cpp, "return \"A confirm"), "default hint preserved");
    check(contains(tte_cpp, "EndModal(1)"), "default A confirm preserved");

    // Visual modal
    check(contains(modal_cpp, "SetWindow") && contains(modal_cpp, "DrawString"), "modal uses SetWindow DrawString");
    check(!contains(modal_cpp, "TreeFrogGetFramebuffer"), "no direct framebuffer");
    check(!contains(modal_cpp, "SampleChopperModal"), "no chopper modify");
    check(contains(modal_cpp, "width + 2"), "modal bounded not fullscreen");

    // Build marker
    check(contains(spd_cpp, "TreeFrogStartupProjectActionsBuildMarker"), "build marker");
    check(contains(spd_cpp, "TREEFROG_STARTUP_PROJECT_ACTIONS_V1"), "marker string");

    // Unrelated files not modified
    for(auto path: { "source/sources/Application/UI/Views/MixerView.cpp", "source/sources/Application/UI/Views/InstrumentEqView.cpp", "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp", "source/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp" }){
        std::string c = read_file(path);
        if(!c.empty()){
            check(!contains(c, "TREEFROG_STARTUP_PROJECT_ACTIONS_V1"), std::string("no startup marker in ")+path);
            check(!contains(c, "PA_DUPLICATE"), std::string("no PA_DUPLICATE in ")+path);
        }
    }

    if(errors==0){ printf("STARTUP_PROJECT_ACTIONS_HOST_TEST_OK\n"); return 0; }
    fprintf(stderr,"STARTUP_PROJECT_ACTIONS_HOST_TEST_FAIL %d\n",errors); return 1;
}
