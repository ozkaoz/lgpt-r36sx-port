# device — Scoped Instructions

> Adds constraints for OTG / MUSB / audio daemon / kernel module work.
> Must not contradict root `AGENTS.md`; always read root first.

---

## Scope

`device/` — launcher, OTG scripts (`otg_*.sh`), daemons (`r36s_*`), MUSB routing.

## Rules

- **No speculative kernel/module switching changes** without evidence. `CONFIG_MODULE_UNLOAD=n` has been observed — verify `CONFIG_MODULE_UNLOAD` before assuming hot ALSA family replace. Default is shared/persistent lifecycle.
- **Do not hardcode strict-family architecture** as permanent policy without `DECISION` (see `DEC-2026-08-23-06`). Require evidence (kernel config, physical test).
- **OTG/MUSB changes require physical matrix:** `LOCAL | WINDOWS (detect/playback/record) | SP404 | ANDROID` per `docs/ai/RELEASE_CONTRACT.md`. `Detection != Runtime READY != PCM flow != physical PASS`.
- **Filesystem first:** before blaming runtime, verify SD `mounted && healthy && writable` (dirty exFAT previously faked USB failure). `scripts/agent_preflight.sh --sd <mount>` . Never auto `fsck` — needs user auth.
- **Volatile vs persistent:** persistent baseline is the 6 files in `docs/ai/RELEASE_CONTRACT.md`. Never package `FIFO / PID / daemon_pid / setup_result / sp404_card / aoa state / /tmp / logs`.
- **No destructive SD ops** without explicit user approval. `install.sh` here is legacy — see root `AGENTS.md` Section 9.
