// sp404_fifo_dump_production_off_host_test - verifies P1 fifo dump disabled by default
#include <cstdio>
#include <fstream>
#include <string>

static std::string read_file(const char* p){
    std::ifstream f(p);
    if(f.good()){
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    return "";
}
int main(){
    std::string c;
    const char* candidates[]={
        "device/r36s_sp404_host_audio_io.c",
        "/home/dafunknoise/lgpt-repo/device/r36s_sp404_host_audio_io.c",
        "../device/r36s_sp404_host_audio_io.c",
        "../../device/r36s_sp404_host_audio_io.c",
        "lgpt-r36sx-port/device/r36s_sp404_host_audio_io.c",
        "/mnt/c/Users/DaFunkNoise/Documents/Default Project/lgpt-r36sx-port/device/r36s_sp404_host_audio_io.c"
    };
    for(auto p: candidates){
        c = read_file(p);
        if(!c.empty()) break;
    }
    if(c.empty()){
        fprintf(stderr,"FAIL missing device/r36s_sp404_host_audio_io.c tried multiple paths\n");
        return 1;
    }
    // Must have SP404_ENABLE_FIFO_DUMP default 0
    if(c.find("#define SP404_ENABLE_FIFO_DUMP 0")==std::string::npos){
        fprintf(stderr,"FAIL missing SP404_ENABLE_FIFO_DUMP default 0\n");
        return 1;
    }
    if(c.find("#ifndef SP404_ENABLE_FIFO_DUMP")==std::string::npos){
        fprintf(stderr,"FAIL missing ifndef guard\n");
        return 1;
    }
    // Check #if guard around dump loop
    if(c.find("#if SP404_ENABLE_FIFO_DUMP")==std::string::npos){
        fprintf(stderr,"FAIL missing #if SP404_ENABLE_FIFO_DUMP guard around loop\n");
        return 1;
    }
    size_t pos = c.find("fifo_dump_write(out + pd");
    if(pos==std::string::npos){
        fprintf(stderr,"FAIL missing fifo_dump_write loop\n");
        return 1;
    }
    std::string ctx = c.substr(pos>500?pos-500:0, 600);
    if(ctx.find("#if SP404_ENABLE_FIFO_DUMP")==std::string::npos){
        fprintf(stderr,"FAIL fifo_dump_write loop not guarded by #if SP404_ENABLE_FIFO_DUMP\n");
        return 1;
    }
    if(c.find("#define fifo_dump_write")==std::string::npos){
        fprintf(stderr,"FAIL missing macro fifo_dump_write for disabled case\n");
        return 1;
    }
    if(c.find("#define fifo_dump_start")==std::string::npos){
        fprintf(stderr,"FAIL missing macro fifo_dump_start\n");
        return 1;
    }
    if(c.find("static void fifo_dump_finish(const char *why);")==std::string::npos){
        fprintf(stderr,"FAIL missing real fifo_dump_finish declaration under #if\n");
        return 1;
    }
    if(c.find("P1 F: memmove audit") == std::string::npos){
        fprintf(stderr,"FAIL missing memmove audit comment\n");
        return 1;
    }
    if(c.find("P1 D: format conversion fastpath") == std::string::npos){
        fprintf(stderr,"FAIL missing fastpath comment\n");
        return 1;
    }
    printf("SP404_FIFO_DUMP_PRODUCTION_OFF_OK\n");
    printf("Verified SP404_ENABLE_FIFO_DUMP=0 default, #if guard, macros, memmove audit present\n");
    return 0;
}
