#include "Application/Utils/PerfDiag.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
// Host test markers: PERF_DIAG_SD_WRITE_FAIL AUTO_WRITE_FAIL AUTO_WRITE_OK PERF_DIAG_BOOT_OK
namespace PerfDiag {
unsigned long g_frameCount = 0;
unsigned long g_flushCount = 0;
unsigned long g_videoRefreshCount = 0;
unsigned long g_eqDrawCount = 0;
unsigned long g_mixerDrawCount = 0;
unsigned long g_analyzerComputeCount = 0;
unsigned long g_analyzerComputedTrue = 0;
unsigned long g_inputPollCount = 0;
unsigned long g_inputEventCount = 0;
char g_currentTag[64] = "none";
char g_currentView[32] = "unknown";
static char g_currentDriver[32] = "LOCAL";
static bool g_initialized = false;
struct WindowState {
    const char* driver;
    const char* view;
    bool done;
    bool measuring;
    unsigned long stableStartMs;
    unsigned long measureStartMs;
    unsigned long lgptStartTicks;
    unsigned long sp404StartTicks;
    unsigned long ctxtStart;
    unsigned long cpuUserStart;
    unsigned long cpuSystemStart;
    unsigned long cpuIdleStart;
    char irqStart[8192];
    char softStart[4096];
    char lgptStatStart[512];
    char sp404StatStart[512];
};
static WindowState g_windows[6] = {
    {"LOCAL","MAIN",false,false,0,0,0,0,0,0,0,0,{0},{0},{0},{0}},
    {"LOCAL","MIXER",false,false,0,0,0,0,0,0,0,0,{0},{0},{0},{0}},
    {"LOCAL","EQ8",false,false,0,0,0,0,0,0,0,0,{0},{0},{0},{0}},
    {"SP404","MAIN",false,false,0,0,0,0,0,0,0,0,{0},{0},{0},{0}},
    {"SP404","MIXER",false,false,0,0,0,0,0,0,0,0,{0},{0},{0},{0}},
    {"SP404","EQ8",false,false,0,0,0,0,0,0,0,0,{0},{0},{0},{0}}
};
static WindowState* g_currentMeasuring = 0;
static unsigned long g_lastDriverViewChangeMs = 0;
static char g_lastDriver[32] = "";
static char g_lastView[32] = "";
static const unsigned long kMaxBytesPerBoot = 256 * 1024;
static unsigned long g_totalWritten = 0;
static void ensure_dir(const char* path){
    mkdir("/tmp/r36sx_lgpt_usb",0777);
    mkdir("/tmp/r36sx_lgpt_logs",0777);
    char tmp[256]; strncpy(tmp,path,sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
    char* p=strrchr(tmp,'/'); if(p){ *p=0; mkdir(tmp,0777); }
}
static void write_boot_sentinel(){
    ensure_dir("/tmp/r36sx_lgpt_logs/PERF_DIAG_BOOT.log");
    const char* out="/tmp/r36sx_lgpt_logs/PERF_DIAG_BOOT.log";
    // Truncate per boot, bounded
    FILE* f=fopen(out,"w");
    if(f){ fprintf(f,"PERF_DIAG_BOOT_OK pid=%d build=a0e89c7f\n",(int)getpid()); fclose(f); g_totalWritten=0; }
}
void Init(){
    if(g_initialized) return;
    g_initialized=true;
    ensure_dir("/tmp/r36sx_lgpt_logs/PERF_DIAG_BOOT.log");
    write_boot_sentinel();
    FILE* f=fopen("/tmp/r36sx_lgpt_logs/perf_diag.log","w");
    if(f){ fprintf(f,"PERF_DIAG_INIT tag=%s view=%s driver=%s\n",g_currentTag,g_currentView,g_currentDriver); fclose(f); }
    FILE* t=fopen("/tmp/r36sx_lgpt_usb/perf_diag.log","w");
    if(t){ fprintf(t,"PERF_DIAG_INIT tag=%s view=%s driver=%s\n",g_currentTag,g_currentView,g_currentDriver); fclose(t); }
}
void CountFrame(){ ++g_frameCount; }
void CountFlush(){ ++g_flushCount; }
void CountVideoRefresh(){ ++g_videoRefreshCount; }
void CountEqDraw(){ ++g_eqDrawCount; }
void CountMixerDraw(){ ++g_mixerDrawCount; }
void CountAnalyzer(bool didCompute){ ++g_analyzerComputeCount; if(didCompute) ++g_analyzerComputedTrue; }
void CountInputPoll(){ ++g_inputPollCount; }
void CountInputEvent(){ ++g_inputEventCount; }
void SetTag(const char* tag){ if(tag){ strncpy(g_currentTag,tag,63); g_currentTag[63]=0; } }
void SetView(const char* v){
    if(!v) return;
    const char* nv=v;
    if(strcmp(v,"Song")==0) nv="MAIN";
    else if(strcmp(v,"Other")==0) nv="MAIN";
    strncpy(g_currentView,nv,31); g_currentView[31]=0;
}
void SetDriver(const char* driver){
    if(!driver) return;
    const char* nd="LOCAL";
    if(strstr(driver,"SP404")||strstr(driver,"USB_OUT")||strstr(driver,"SP404_IN")) nd="SP404";
    else if(strstr(driver,"LOCAL")) nd="LOCAL";
    else if(strstr(driver,"WINDOWS")) nd="WINDOWS";
    else if(strstr(driver,"ANDROID")) nd="ANDROID";
    else nd="LOCAL";
    strncpy(g_currentDriver,nd,31); g_currentDriver[31]=0;
}
void SetDriverMode(int mode){
    const char* nd="LOCAL";
    // U241 enum: 0 LOCAL_CONSOLE, 1 WINDOWS, 2 ANDROID, 3 USB_OUT (Sampler), 4 MIDI, 5 SP404_IN
    if(mode==3 || mode==5) nd="SP404";
    else if(mode==0) nd="LOCAL";
    else if(mode==1) nd="WINDOWS";
    else if(mode==2) nd="ANDROID";
    else if(mode==4) nd="MIDI";
    else nd="LOCAL";
    strncpy(g_currentDriver,nd,31); g_currentDriver[31]=0;
}
void Dump(const char* reason){
    // RAM only, bounded
    if(g_totalWritten > kMaxBytesPerBoot) return;
    ensure_dir("/tmp/r36sx_lgpt_logs/PERF_DIAG.log");
    const char* out="/tmp/r36sx_lgpt_logs/PERF_DIAG.log";
    FILE* f=fopen(out,"a");
    if(f){
        long before=ftell(f);
        fprintf(f,"PERF_DIAG tag=%s view=%s driver=%s reason=%s frame=%lu flush=%lu video=%lu eqDraw=%lu mixerDraw=%lu analyzer=%lu/%lu inputPoll=%lu inputEvent=%lu\n",
            g_currentTag,g_currentView,g_currentDriver,reason?reason:"periodic",
            g_frameCount,g_flushCount,g_videoRefreshCount,
            g_eqDrawCount,g_mixerDrawCount,
            g_analyzerComputeCount,g_analyzerComputedTrue,
            g_inputPollCount,g_inputEventCount);
        long after=ftell(f);
        if(after>before) g_totalWritten += (after-before);
        fclose(f);
    }
    // also mirror to /tmp/r36sx_lgpt_usb for backward compat, RAM
    FILE* uf=fopen("/tmp/r36sx_lgpt_usb/perf_diag.log","a");
    if(uf){
        fprintf(uf,"PERF_DIAG tag=%s view=%s driver=%s reason=%s frame=%lu flush=%lu video=%lu eqDraw=%lu mixerDraw=%lu analyzer=%lu/%lu inputPoll=%lu inputEvent=%lu\n",
            g_currentTag,g_currentView,g_currentDriver,reason?reason:"periodic",
            g_frameCount,g_flushCount,g_videoRefreshCount,
            g_eqDrawCount,g_mixerDrawCount,
            g_analyzerComputeCount,g_analyzerComputedTrue,
            g_inputPollCount,g_inputEventCount);
        fclose(uf);
    }
}
void Reset(const char* tag,const char* view){
    g_frameCount=0; g_flushCount=0; g_videoRefreshCount=0;
    g_eqDrawCount=0; g_mixerDrawCount=0;
    g_analyzerComputeCount=0; g_analyzerComputedTrue=0;
    g_inputPollCount=0; g_inputEventCount=0;
    if(tag) SetTag(tag); if(view) SetView(view);
}
static unsigned long read_ctxt(){
    FILE* f=fopen("/proc/stat","r"); if(!f) return 0;
    char line[512];
    while(fgets(line,sizeof(line),f)){
        if(strncmp(line,"ctxt",4)==0){ unsigned long v=0; sscanf(line,"ctxt %lu",&v); fclose(f); return v; }
    }
    fclose(f); return 0;
}
static void read_cpu(unsigned long *user,unsigned long *sys,unsigned long *idle){
    FILE* f=fopen("/proc/stat","r"); if(!f) return;
    char line[512];
    if(fgets(line,sizeof(line),f)){
        unsigned long u=0,n=0,s=0,i=0; sscanf(line,"cpu %lu %lu %lu %lu",&u,&n,&s,&i);
        if(user) *user=u+n; if(sys) *sys=s; if(idle) *idle=i;
    }
    fclose(f);
}
static unsigned long read_pid_ticks(pid_t pid){
    char path[64]; snprintf(path,sizeof(path),"/proc/%d/stat",(int)pid);
    FILE* f=fopen(path,"r"); if(!f) return 0;
    char buf[1024]; if(!fgets(buf,sizeof(buf),f)){ fclose(f); return 0; } fclose(f);
    char* p=strrchr(buf,')'); if(!p) return 0; p++;
    unsigned long ut=0,st=0; int n=sscanf(p," %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",&ut,&st);
    if(n==2) return ut+st; return 0;
}
static pid_t find_pid(const char* name){
    DIR* d=opendir("/proc"); if(!d) return 0;
    struct dirent* de;
    while((de=readdir(d))){
        if(de->d_name[0]<'0'||de->d_name[0]>'9') continue;
        char path[128]; snprintf(path,sizeof(path),"/proc/%s/comm",de->d_name);
        FILE* f=fopen(path,"r"); if(!f) continue;
        char comm[64]={0}; if(fgets(comm,sizeof(comm),f)){
            comm[strcspn(comm,"\r\n")]=0;
            if(strstr(comm,name)){ pid_t pid=atoi(de->d_name); fclose(f); closedir(d); return pid; }
        }
        fclose(f);
    }
    closedir(d); return 0;
}
static void capture_proc_snapshot(WindowState* w){
    w->ctxtStart=read_ctxt();
    read_cpu(&w->cpuUserStart,&w->cpuSystemStart,&w->cpuIdleStart);
    FILE* f=fopen("/proc/interrupts","r"); if(f){ size_t n=fread(w->irqStart,1,sizeof(w->irqStart)-1,f); w->irqStart[n]=0; fclose(f);} else w->irqStart[0]=0;
    f=fopen("/proc/softirqs","r"); if(f){ size_t n=fread(w->softStart,1,sizeof(w->softStart)-1,f); w->softStart[n]=0; fclose(f);} else w->softStart[0]=0;
    pid_t lgpt=find_pid("picoarch"); if(lgpt==0) lgpt=find_pid("lgpt");
    pid_t sp=0; FILE* pf=fopen("/tmp/r36sx_lgpt_usb/sp404_daemon_pid","r");
    if(pf){ char buf[32]={0}; if(fgets(buf,sizeof(buf),pf)) sp=atoi(buf); fclose(pf); }
    if(sp==0){ pf=fopen("/tmp/r36sx_lgpt_usb/daemon_pid","r"); if(pf){ char buf[32]={0}; if(fgets(buf,sizeof(buf),pf)) sp=atoi(buf); fclose(pf);} }
    w->lgptStartTicks=lgpt?read_pid_ticks(lgpt):0;
    w->sp404StartTicks=sp?read_pid_ticks(sp):0;
    if(lgpt){ char path[64]; snprintf(path,sizeof(path),"/proc/%d/stat",(int)lgpt); FILE* sf=fopen(path,"r"); if(sf){ fgets(w->lgptStatStart,sizeof(w->lgptStatStart),sf); fclose(sf);} }
    if(sp){ char path[64]; snprintf(path,sizeof(path),"/proc/%d/stat",(int)sp); FILE* sf=fopen(path,"r"); if(sf){ fgets(w->sp404StatStart,sizeof(w->sp404StatStart),sf); fclose(sf);} }
}
static void write_window_log(WindowState* w, unsigned long durationMs){
    if(g_totalWritten > kMaxBytesPerBoot) return;
    ensure_dir("/tmp/r36sx_lgpt_logs/PERF_AUTO_LOCAL_MAIN.log");
    char path[128]; snprintf(path,sizeof(path),"/tmp/r36sx_lgpt_logs/PERF_AUTO_%s_%s.log",w->driver,w->view);
    unsigned long ctxtEnd=read_ctxt();
    unsigned long cpuU2=0,cpuS2=0,cpuI2=0; read_cpu(&cpuU2,&cpuS2,&cpuI2);
    char irqEnd[8192]={0},softEnd[4096]={0};
    FILE* f=fopen("/proc/interrupts","r"); if(f){ size_t n=fread(irqEnd,1,sizeof(irqEnd)-1,f); irqEnd[n]=0; fclose(f); }
    f=fopen("/proc/softirqs","r"); if(f){ size_t n=fread(softEnd,1,sizeof(softEnd)-1,f); softEnd[n]=0; fclose(f); }
    pid_t lgpt=find_pid("picoarch"); if(lgpt==0) lgpt=find_pid("lgpt");
    pid_t sp=0; FILE* pf=fopen("/tmp/r36sx_lgpt_usb/sp404_daemon_pid","r");
    if(pf){ char buf[32]={0}; if(fgets(buf,sizeof(buf),pf)) sp=atoi(buf); fclose(pf); }
    if(sp==0){ pf=fopen("/tmp/r36sx_lgpt_usb/daemon_pid","r"); if(pf){ char buf[32]={0}; if(fgets(buf,sizeof(buf),pf)) sp=atoi(buf); fclose(pf);} }
    unsigned long lgptEnd=lgpt?read_pid_ticks(lgpt):0;
    unsigned long spEnd=sp?read_pid_ticks(sp):0;
    double frameHz=(double)g_frameCount*1000.0/(double)durationMs;
    double flushHz=(double)g_flushCount*1000.0/(double)durationMs;
    double videoHz=(double)g_videoRefreshCount*1000.0/(double)durationMs;
    double eqHz=(double)g_eqDrawCount*1000.0/(double)durationMs;
    double mixerHz=(double)g_mixerDrawCount*1000.0/(double)durationMs;
    double analyzerHz=(double)g_analyzerComputeCount*1000.0/(double)durationMs;
    FILE* out=fopen(path,"w");
    if(!out){
        FILE* ef=fopen("/tmp/r36sx_lgpt_logs/PERF_DIAG_SD_WRITE_FAIL.log","a");
        if(ef){ fprintf(ef,"AUTO_WRITE_FAIL file=%s errno=%d\n",path,errno); fclose(ef); }
        return;
    }
    fprintf(out,"BUILD=a0e89c7f\n");
    fprintf(out,"DRIVER=%s\n",w->driver);
    fprintf(out,"VIEW=%s\n",w->view);
    fprintf(out,"DURATION_MS=%lu\n",durationMs);
    fprintf(out,"FRAME_COUNT=%lu\n",g_frameCount);
    fprintf(out,"FRAME_HZ=%.2f\n",frameHz);
    fprintf(out,"FLUSH_COUNT=%lu\n",g_flushCount);
    fprintf(out,"FLUSH_HZ=%.2f\n",flushHz);
    fprintf(out,"VIDEO_COUNT=%lu\n",g_videoRefreshCount);
    fprintf(out,"VIDEO_HZ=%.2f\n",videoHz);
    fprintf(out,"EQ_DRAW_COUNT=%lu\n",g_eqDrawCount);
    fprintf(out,"EQ_DRAW_HZ=%.2f\n",eqHz);
    fprintf(out,"MIXER_DRAW_COUNT=%lu\n",g_mixerDrawCount);
    fprintf(out,"MIXER_DRAW_HZ=%.2f\n",mixerHz);
    fprintf(out,"ANALYZER_COUNT=%lu\n",g_analyzerComputeCount);
    fprintf(out,"ANALYZER_TRUE=%lu\n",g_analyzerComputedTrue);
    fprintf(out,"ANALYZER_HZ=%.2f\n",analyzerHz);
    fprintf(out,"INPUT_POLLS=%lu\n",g_inputPollCount);
    fprintf(out,"INPUT_EVENTS=%lu\n",g_inputEventCount);
    fprintf(out,"LGPT_TICKS_DELTA=%lu\n",lgptEnd - w->lgptStartTicks);
    fprintf(out,"SP404_TICKS_DELTA=%lu\n",spEnd - w->sp404StartTicks);
    fprintf(out,"CPU_USER_DELTA=%lu\n",cpuU2 - w->cpuUserStart);
    fprintf(out,"CPU_SYSTEM_DELTA=%lu\n",cpuS2 - w->cpuSystemStart);
    fprintf(out,"CPU_IDLE_DELTA=%lu\n",cpuI2 - w->cpuIdleStart);
    fprintf(out,"CTXT_DELTA=%lu\n",ctxtEnd - w->ctxtStart);
    fprintf(out,"IRQ_START:\n%s\n",w->irqStart);
    fprintf(out,"IRQ_END:\n%s\n",irqEnd);
    fprintf(out,"SOFT_START:\n%s\n",w->softStart);
    fprintf(out,"SOFT_END:\n%s\n",softEnd);
    fprintf(out,"LGPT_STAT_START:%s\n",w->lgptStatStart);
    fprintf(out,"SP404_STAT_START:%s\n",w->sp404StatStart);
    fflush(out); fclose(out);
    struct stat st; if(stat(path,&st)==0){
        g_totalWritten += st.st_size;
        FILE* b=fopen("/tmp/r36sx_lgpt_logs/PERF_DIAG_BOOT.log","a");
        if(b){ fprintf(b,"AUTO_WRITE_OK file=PERF_AUTO_%s_%s.log bytes=%ld\n",w->driver,w->view,(long)st.st_size); fclose(b); }
        FILE* tb=fopen("/tmp/r36sx_lgpt_usb/PERF_DIAG_BOOT.log","a");
        if(tb){ fprintf(tb,"AUTO_WRITE_OK file=PERF_AUTO_%s_%s.log bytes=%ld\n",w->driver,w->view,(long)st.st_size); fclose(tb); }
    }
    // also mirror to /tmp/r36sx_lgpt_usb for compat
    char alt[128]; snprintf(alt,sizeof(alt),"/tmp/r36sx_lgpt_usb/PERF_AUTO_%s_%s.log",w->driver,w->view);
    FILE* src=fopen(path,"r"); FILE* dst=fopen(alt,"w");
    if(src && dst){ char buf[1024]; size_t n; while((n=fread(buf,1,sizeof(buf),src))>0) fwrite(buf,1,n,dst); }
    if(src) fclose(src); if(dst) fclose(dst);
}
void PeriodicCheck(unsigned long nowMs){
    if(!g_initialized) Init();
    static unsigned long lastTriggerCheckMs=0;
    if(nowMs - lastTriggerCheckMs >= 1000){
        lastTriggerCheckMs=nowMs;
        FILE* tf=fopen("/tmp/r36sx_lgpt_usb/perf_diag_trigger","r");
        if(tf){
            char buf[128]={0};
            if(fgets(buf,sizeof(buf),tf)){
                if(strncmp(buf,"START",5)==0){
                    char tag[64]={0}, view[32]={0};
                    sscanf(buf,"START %63s %31s",tag,view);
                    Reset(tag[0]?tag:"manual", view[0]?view:"unknown");
                    Dump("trigger_start");
                } else if(strncmp(buf,"STOP",4)==0 || strncmp(buf,"DUMP",4)==0){
                    Dump("trigger_stop");
                }
            }
            fclose(tf); remove("/tmp/r36sx_lgpt_usb/perf_diag_trigger");
        }
    }
    static unsigned long lastWindowCheckMs=0;
    if(nowMs - lastWindowCheckMs < 1000) {
        if(g_currentMeasuring){
            // Cancel if driver/view changed during measuring
            if(strcmp(g_currentDriver, g_currentMeasuring->driver)!=0 || strcmp(g_currentView, g_currentMeasuring->view)!=0){
                g_currentMeasuring->measuring=false;
                // not done, allow retry after stable
                g_currentMeasuring=0;
                g_frameCount=0; g_flushCount=0; g_videoRefreshCount=0; g_eqDrawCount=0; g_mixerDrawCount=0; g_analyzerComputeCount=0; g_analyzerComputedTrue=0; g_inputPollCount=0; g_inputEventCount=0;
                g_lastDriverViewChangeMs=nowMs;
                strncpy(g_lastDriver,g_currentDriver,31); g_lastDriver[31]=0;
                strncpy(g_lastView,g_currentView,31); g_lastView[31]=0;
                return;
            }
            unsigned long elapsed=nowMs - g_currentMeasuring->measureStartMs;
            if(elapsed >= 30000){
                write_window_log(g_currentMeasuring, elapsed);
                g_currentMeasuring->done=true;
                g_currentMeasuring->measuring=false;
                g_currentMeasuring=0;
                g_frameCount=0; g_flushCount=0; g_videoRefreshCount=0; g_eqDrawCount=0; g_mixerDrawCount=0; g_analyzerComputeCount=0; g_analyzerComputedTrue=0; g_inputPollCount=0; g_inputEventCount=0;
            }
        }
        return;
    }
    lastWindowCheckMs=nowMs;
    if(g_currentMeasuring){
        // Cancel if driver/view changed
        if(strcmp(g_currentDriver, g_currentMeasuring->driver)!=0 || strcmp(g_currentView, g_currentMeasuring->view)!=0){
            g_currentMeasuring->measuring=false;
            g_currentMeasuring=0;
            g_frameCount=0; g_flushCount=0; g_videoRefreshCount=0; g_eqDrawCount=0; g_mixerDrawCount=0; g_analyzerComputeCount=0; g_analyzerComputedTrue=0; g_inputPollCount=0; g_inputEventCount=0;
            g_lastDriverViewChangeMs=nowMs;
            strncpy(g_lastDriver,g_currentDriver,31); g_lastDriver[31]=0;
            strncpy(g_lastView,g_currentView,31); g_lastView[31]=0;
            return;
        }
        unsigned long elapsed=nowMs - g_currentMeasuring->measureStartMs;
        if(elapsed >= 30000){
            write_window_log(g_currentMeasuring, elapsed);
            g_currentMeasuring->done=true;
            g_currentMeasuring->measuring=false;
            g_currentMeasuring=0;
            g_frameCount=0; g_flushCount=0; g_videoRefreshCount=0; g_eqDrawCount=0; g_mixerDrawCount=0; g_analyzerComputeCount=0; g_analyzerComputedTrue=0; g_inputPollCount=0; g_inputEventCount=0;
        } else {
            return;
        }
    }
    const char* curDriver=g_currentDriver;
    const char* curView=g_currentView;
    if(strcmp(curView,"unknown")==0) return;
    if(strcmp(curDriver,g_lastDriver)!=0 || strcmp(curView,g_lastView)!=0){
        strncpy(g_lastDriver,curDriver,31); g_lastDriver[31]=0;
        strncpy(g_lastView,curView,31); g_lastView[31]=0;
        g_lastDriverViewChangeMs=nowMs;
        for(int i=0;i<6;i++){ if(!g_windows[i].done && !g_windows[i].measuring) g_windows[i].stableStartMs=0; }
        return;
    }
    if(nowMs - g_lastDriverViewChangeMs < 2000) return;
    WindowState* target=0;
    for(int i=0;i<6;i++){
        if(strcmp(g_windows[i].driver,curDriver)==0 && strcmp(g_windows[i].view,curView)==0){
            if(!g_windows[i].done && !g_windows[i].measuring){ target=&g_windows[i]; break; }
        }
    }
    if(!target) return;
    if(g_currentMeasuring) return;
    target->measuring=true;
    target->measureStartMs=nowMs;
    g_frameCount=0; g_flushCount=0; g_videoRefreshCount=0; g_eqDrawCount=0; g_mixerDrawCount=0; g_analyzerComputeCount=0; g_analyzerComputedTrue=0; g_inputPollCount=0; g_inputEventCount=0;
    capture_proc_snapshot(target);
    g_currentMeasuring=target;
    FILE* f=fopen("/tmp/r36sx_lgpt_logs/perf_diag.log","a");
    if(f){ fprintf(f,"AUTO_START driver=%s view=%s\n",target->driver,target->view); fclose(f); }
}
}
