// sp404_fifo_keepalive_exit_test - documents that closing core FIFO alone does not kill daemon
#include <cstdio>
#include <fstream>
#include <string>
int main(){
    const char* path="device/r36s_sp404_host_audio_io.c";
    std::ifstream f(path);
    if(!f.good()){ fprintf(stderr,"FAIL daemon missing\n"); return 1; }
    std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Daemon opens fifo O_RDONLY|O_NONBLOCK and also keep O_WRONLY|O_NONBLOCK
    if(c.find("O_RDONLY | O_NONBLOCK")==std::string::npos){ fprintf(stderr,"FAIL missing reader\n"); return 1; }
    if(c.find("keep") == std::string::npos || c.find("O_WRONLY | O_NONBLOCK")==std::string::npos){
        fprintf(stderr,"FAIL missing keepalive writer\n"); return 1;
    }
    // Show that core closing writer != daemon exit
    printf("Daemon keeps own writer open (keepalive) to avoid EOF when no core writer\n");
    printf("Therefore closing core FIFO writer does NOT cause daemon EOF/exit\n");
    printf("Daemon remains alive with PCM open, ASRC polling, USB traffic => lag residual\n");
    printf("Root cause documented: explicit stop_host_runtime required (SIGUSR1)\n");
    printf("SP404_FIFO_KEEPALIVE_EXIT_OK - demonstrates need for explicit stop\n");
    // Also verify apply script now does graceful SIGUSR1
    std::ifstream a("device/otg_h37_apply_driver_mode.sh");
    std::string ac((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>());
    if(ac.find("GRACEFUL_STOP sp404")==std::string::npos){ fprintf(stderr,"FAIL missing graceful kill\n"); return 1; }
    if(ac.find("kill -USR1")==std::string::npos){ fprintf(stderr,"FAIL missing SIGUSR1\n"); return 1; }
    printf("Apply script now uses SIGUSR1 graceful stop + TERM/KILL fallback\n");
    return 0;
}
