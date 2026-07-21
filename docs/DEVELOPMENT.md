# Development notes

The stable U2.52.3 tree intentionally excludes experimental patches, historical apply scripts, prompts, copied baselines and raw debugging evidence. Git history preserves development history.

Before a change:

1. Run `bash scripts/audit.sh`.
2. Modify only the required subsystem.
3. Re-run the host tests.
4. Compile with the MIPS toolchain.
5. Install incrementally and collect logs from every physical test.

Do not change audio rate, channel count and period size in the same experiment. Keep the validated profile as the baseline.
