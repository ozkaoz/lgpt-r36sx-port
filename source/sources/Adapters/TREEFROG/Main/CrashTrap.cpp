/* U2.53.0: diagnostic-only crash trap.
 *
 * Purpose: when the console crashes in a way that leaves no trace (the
 * reported "chopper L1+R1 after stopping playback" hard crash), a normal
 * SIGSEGV kills the core before any log flush.  This trap records a minimal
 * register dump to tmpfs (/tmp/r36sx_lgpt_logs/crash.txt, mirrored to the
 * SD by the OTG supervisor) and then re-raises with the default disposition,
 * preserving the original death behavior.
 *
 * U2.53.1: the trap also opens a second best-effort fd straight to the SD
 * card (/mnt/sdcard/LGPT_OTG_LOGS/crash.txt, the supervisor's own log dir)
 * at install time.  The full dump is appended to both fds exactly once when
 * the crash fires: never periodic, never truncated, so a hard crash that
 * skips the clean-shutdown flush still reaches the card without any runtime
 * SD write traffic.  If the card is unreachable, the open fails silently
 * and tmpfs remains the only target.  The install-time "ARMED" marker is
 * deliberately written to tmpfs only, keeping the SD untouched at boot.
 *
 * Async-signal-safe by construction: only write() on pre-opened fds plus
 * a hand-rolled hex formatter.  No malloc, no stdio, no locks.  Register
 * layout is MIPS-specific (mcontext_t from <ucontext.h>); on non-MIPS host
 * builds (host_syntax_check.sh) only a minimal signature is emitted. */
#include "CrashTrap.h"

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ucontext.h>

static int gCrashFd = -1;
static int gCrashSdFd = -1;
static volatile sig_atomic_t gCrashHandling = 0;

static void crashWriteFd(int fd, const char *s) {
    if (fd >= 0) {
        size_t n = strlen(s);
        ssize_t r = write(fd, s, n);
        (void)r;
    }
}

static void crashWrite(const char *s) {
    crashWriteFd(gCrashFd, s);
    crashWriteFd(gCrashSdFd, s);
}

static void crashWriteTmp(const char *s) {
    crashWriteFd(gCrashFd, s);
}

static void crashHex(unsigned long long v) {
    char b[20];
    int i = 19;
    b[i] = 0;
    b[--i] = 'x';
    b[--i] = '0';
    do {
        int d = (int)(v & 0xF);
        b[--i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    } while (v != 0);
    crashWrite(b + i);
}

static void crashDec(int v) {
    char b[16];
    int i = 16;
    b[--i] = 0;
    if (v < 0) { crashWrite("--"); v = -v; }
    do {
        b[--i] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);
    crashWrite(b + i);
}

void LgptCrashTrapHandler(int sig, siginfo_t *info, void *ucontext) {
    (void)info;
    if (gCrashHandling) {
        /* Nested fault while dumping: kill cleanly, never loop. */
        _exit(128 + sig);
    }
    gCrashHandling = 1;

    crashWrite("LGPT_CRASH sig=");
    crashDec(sig);
    crashWrite("\n");

    ucontext_t *uc = (ucontext_t *)ucontext;
#if defined(__mips__)
    if (uc) {
        unsigned long long pc = (unsigned long long)uc->uc_mcontext.pc;
        unsigned long long sp = (unsigned long long)uc->uc_mcontext.gregs[29];
        unsigned long long ra = (unsigned long long)uc->uc_mcontext.gregs[31];
        crashWrite("pc="); crashHex(pc); crashWrite("\n");
        crashWrite("ra="); crashHex(ra); crashWrite("\n");
        crashWrite("sp="); crashHex(sp); crashWrite("\n");
        crashWrite("handler_at="); crashHex((unsigned long long)(unsigned long)&LgptCrashTrapHandler); crashWrite("\n");
        {
            int r;
            for (r = 0; r < 32; r++) {
                crashWrite("r");
                crashDec(r);
                crashWrite("=");
                crashHex((unsigned long long)uc->uc_mcontext.gregs[r]);
                crashWrite("\n");
            }
        }
        crashWrite("mdhi=");
        crashHex((unsigned long long)uc->uc_mcontext.mdhi);
        crashWrite("\nmdlo=");
        crashHex((unsigned long long)uc->uc_mcontext.mdlo);
        crashWrite("\n");
    } else {
        crashWrite("no_ucontext\n");
    }
#else
    /* Host (non-MIPS) builds: minimal signature only; a real crash dump is
     * only meaningful on the console. */
    crashWrite("non_mips_dump\n");
#endif

    /* Restore default handling and die exactly as before the trap. */
    signal(sig, SIG_DFL);
    raise(sig);
    _exit(128 + sig);
}

void LgptCrashTrapInstall(void) {
    if (gCrashFd >= 0) return;
    gCrashFd = open("/tmp/r36sx_lgpt_logs/crash.txt",
                    O_WRONLY | O_CREAT | O_APPEND, 0666);
    gCrashSdFd = open("/mnt/sdcard/LGPT_OTG_LOGS/crash.txt",
                      O_WRONLY | O_CREAT | O_APPEND, 0666);
    /* Marker doubles as a live probe that the trap is armed: it appears in
     * the mirrored crash.txt after boot and in the core binary (build.sh
     * greps the exact "U2.53.0" string, so it is kept verbatim).  tmpfs
     * only: the SD must not receive boot-time writes (zero-runtime-writes
     * rule). */
    crashWriteTmp("LGPT_CRASHTRAP_ARMED_U2.53.0\n");
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = LgptCrashTrapHandler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
    sigaction(SIGABRT, &sa, 0);
    sigaction(SIGILL, &sa, 0);
    sigaction(SIGFPE, &sa, 0);
}