#ifndef _LGPT_CRASHTRAP_H_
#define _LGPT_CRASHTRAP_H_

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* U2.53.1: diagnostic-only crash trap.  Catches hard faults (SEGV/BUS/ABRT/
 * ILL/FPE), writes a minimal register dump to /tmp/r36sx_lgpt_logs/crash.txt
 * (RAM; the OTG supervisor mirrors it to /mnt/sdcard/LGPT_OTG_LOGS on
 * clean shutdown) and, best-effort, appends the same dump directly to
 * /mnt/sdcard/LGPT_OTG_LOGS/crash.txt once at crash time via a pre-opened
 * fd (never periodic, never truncated; safe for the SD card), then
 * re-raises the signal with the default disposition so the process dies
 * exactly as it did before.  No audio/UI path is touched; install is O(1)
 * at retro_init. */
void LgptCrashTrapInstall(void);

/* Address of the trap handler itself, exported so a post-crash analysis on
 * the host can compute the runtime load base:
 *   base = &LgptCrashTrapHandler - nm(LgptCrashTrapHandler)   (VMA of .so)
 * then   addr2line -e lgpt_r36sx_u2523.so (pc_abs - base). */
void LgptCrashTrapHandler(int sig, siginfo_t *info, void *ucontext);

#ifdef __cplusplus
}
#endif

#endif /* _LGPT_CRASHTRAP_H_ */