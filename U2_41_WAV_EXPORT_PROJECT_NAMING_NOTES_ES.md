# U2.41 - WAV export project naming and stem filtering

This build is based on U2.40 and keeps the non-blocking export progress screen.

## Changes

- Export output is grouped by project folder:
  - `/mnt/sdcard/lgpt/exports/<ProjectName>/`
  - Windows view: `F:\lgpt\exports\<ProjectName>\`
- Song WAV file name now uses the project folder name:
  - `<ProjectName>.wav`
  - `<ProjectName>_001.wav` if a previous export exists.
- Multitrack exports are written under:
  - `<ProjectName>/multitrack/`
- Multitrack file names now include project name, instrument name, and track number:
  - `<ProjectName>_<InstrumentName>_track_01.wav`
  - `<ProjectName>_<InstrumentName>_track_02.wav`
- Channels without an explicitly assigned instrument in the Song arrangement are skipped during multitrack export.
- Empty sample instruments are skipped. MIDI instruments remain valid stems.
- If a channel uses more than one explicit instrument, the first assigned instrument is used for the filename and `_multi` is appended.

## Expected SD layout

```text
lgpt/
  exports/
    lgpt_QuirkySurfer/
      lgpt_QuirkySurfer.wav
      multitrack/
        lgpt_QuirkySurfer_Kick_track_01.wav
        lgpt_QuirkySurfer_Snare_track_02.wav
```

## Test protocol

1. Install the core.
2. Open the port only through `roms/lgpt/start.lgpt`.
3. Open a project with Song rows assigned.
4. Use `Project > Export > Song WAV`.
5. Confirm that the WAV appears under `lgpt/exports/<ProjectName>/`.
6. Use `Project > Export > Multitrack`.
7. Confirm that only channels with explicit instrument assignments in the Song arrangement are exported.
8. Inspect `/mnt/sdcard/lgpt/wav_export_debug.log` if a stem is skipped unexpectedly.
