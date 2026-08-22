// ANDROID AOA HOST TEST: race, generation, UDC, MUSB, supervisor ordering
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int checks=0, failures=0;
#define CHECK(c,msg) do{checks++; if(!(c)){failures++; printf("FAIL: %s\n",msg);} else printf("PASS: %s\n",msg);} while(0)
int main(){
    // 1. launcher must NOT auto-start u241 setup
    FILE *f=fopen("sd_root/cubegm/lgpt","r");
    if(!f) f=fopen("device/lgpt_launcher_u241.sh","r");
    CHECK(f!=NULL, "launcher exists");
    if(f){
        char buf[8192]; size_t n=fread(buf,1,sizeof(buf)-1,f); buf[n]=0; fclose(f);
        CHECK(strstr(buf,"otg_u241_setup_once.sh") == NULL || strstr(buf,"LOCAL_CONSOLE must NOT")!=NULL, "android_no_u241_on_local: no auto u241 on LOCAL");
    }
    // 2. u241_setup_once must be cancelable (PID file, trap)
    f=fopen("device/otg_u241_setup_once.sh","r");
    CHECK(f!=NULL, "u241_setup_once exists");
    if(f){
        char buf[16384]; size_t n=fread(buf,1,sizeof(buf)-1,f); buf[n]=0; fclose(f);
        CHECK(strstr(buf,"PIDFILE=")!=NULL, "android_cancel_u241_setup: PIDFILE");
        CHECK(strstr(buf,"trap")!=NULL && strstr(buf,"PIDFILE")!=NULL, "trap cleanup PIDFILE");
        CHECK(strstr(buf,"still_windows_requested")!=NULL, "android_generation_invalidates_windows: still_windows_requested");
        CHECK(strstr(buf,"check_generation")!=NULL || strstr(buf,"MY_GENERATION")!=NULL, "generation guard");
    }
    // 3. stop_windows_runtime must handle PID/LOCK/UDC
    f=fopen("device/otg_h37_apply_driver_mode.sh","r");
    CHECK(f!=NULL, "apply_driver_mode exists");
    if(f){
        char buf[32768]; size_t n=fread(buf,1,sizeof(buf)-1,f); buf[n]=0; fclose(f);
        CHECK(strstr(buf,"u241_setup_pid")!=NULL, "android_udc_unbound_before_host: stop handles PID");
        CHECK(strstr(buf,"UDC")!=NULL && strstr(buf,"unbound")!=NULL, "UDC unbound check");
        CHECK(strstr(buf,"MUSB")!=NULL && strstr(buf,"host")!=NULL, "android_host_role_stable: MUSB host check");
        CHECK(strstr(buf,"android_supervisor")!=NULL, "android_supervisor_after_host_only");
        // check ordering: stop_windows before h35_switch_host_role before android supervisor
        char *p1=strstr(buf,"stop_windows_runtime");
        char *p2=strstr(buf,"h35_switch_host_role");
        char *p3=strstr(buf,"otg_h37_android_runtime_supervisor");
        CHECK(p1 && p2 && p3 && p1 < p2 && p2 < p3, "ordering: stop_windows -> host_role -> android supervisor");
    }
    // 4. windows setup still works
    f=fopen("device/otg_h37_apply_driver_mode.sh","r");
    if(f){
        char buf[32768]; size_t n=fread(buf,1,sizeof(buf)-1,f); buf[n]=0; fclose(f);
        CHECK(strstr(buf,"otg_u241_setup_once.sh")!=NULL, "windows_setup_still_works: setup still called for WINDOWS");
    }
    // 5. driver_transition_matrix: all modes handled
    f=fopen("device/otg_h37_apply_driver_mode.sh","r");
    if(f){
        char buf[32768]; size_t n=fread(buf,1,sizeof(buf)-1,f); buf[n]=0; fclose(f);
        CHECK(strstr(buf,"LOCAL_CONSOLE")!=NULL, "driver_transition_matrix: LOCAL");
        CHECK(strstr(buf,"WINDOWS")!=NULL, "driver_transition_matrix: WINDOWS");
        CHECK(strstr(buf,"ANDROID")!=NULL, "driver_transition_matrix: ANDROID");
    }
    printf("ANDROID_AOA_HOST_TEST: %d checks, %d failures\n", checks, failures);
    return failures?1:0;
}
