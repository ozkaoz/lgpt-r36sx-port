/*
 * chopper_operation_render_host_test.cpp
 * Checks operation progress sequences and completion handling
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
    const char* f="/home/dafunknoise/lgpt-repo/source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp";
    // Preview 5,65,90
    CHECK(contains(f, "showOperationProgress(\"Operacion Preview\", 5)"));
    CHECK(contains(f, "showOperationProgress(\"Operacion Preview\", 65)"));
    CHECK(contains(f, "showOperationProgress(\"Operacion Preview\", 90)"));
    // Apply 0,15,55,72,80,90,100
    CHECK(contains(f, "showOperationProgress(label, 0)"));
    CHECK(contains(f, "showOperationProgress(label, 15)"));
    CHECK(contains(f, "showOperationProgress(label, 55)"));
    CHECK(contains(f, "showOperationProgress(label, 72)"));
    CHECK(contains(f, "showOperationProgress(label, 80)"));
    CHECK(contains(f, "showOperationProgress(label, 90)"));
    // Undo/Redo 10,70,100
    CHECK(contains(f, "showOperationProgress(redo ? \"Operacion Redo\" : \"Operacion Undo\", 10)"));
    CHECK(contains(f, "showOperationProgress(redo ? \"Operacion Redo\" : \"Operacion Undo\", 70)"));
    CHECK(contains(f, "showOperationProgress(redo ? \"Operacion Redo\" : \"Operacion Undo\", 100)"));
    // Completion hints
    CHECK(contains(f, "A close  L1+X undo  R1+X redo"));
    CHECK(contains(f, "Processing sample, please wait"));
    // Sleep 90 preserved
    CHECK(contains(f, "Sleep(90)"));
    // Direct operation overlay
    CHECK(contains(f, "g_chopperOperationPercent >= 100"));
    printf("All %d checks PASS - operation render sequences OK\n", checks);
    return 0;
}
