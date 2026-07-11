# U2.41 - R36SX WAV Export Release Notes

## Summary

U2.41 stabilizes the WAV export workflow for the R36SX/TreeFrogUI LGPT port.

## User-facing changes

- `Project > Render` is now presented as `Project > Export`.
- Export modes are `Off`, `Song WAV`, and `Multitrack`.
- Exports use a non-blocking progress screen.
- Song WAV export writes a complete song render without requiring real-time listening.
- Multitrack export writes stems only for tracks with explicitly assigned instruments in the Song arrangement.
- Export output is grouped by project name.
- Multitrack filenames include project name, instrument name, and track number.
- The only visible launcher is `roms/lgpt/start.lgpt`.

## Expected SD output

```text
lgpt/exports/<ProjectName>/<ProjectName>.wav
lgpt/exports/<ProjectName>/multitrack/<ProjectName>_<InstrumentName>_track_01.wav
```

## Developer notes

Main code paths:

```text
sources/Application/Mixer/MixerService.cpp
sources/Application/Mixer/MixerService.h
sources/Application/Player/Player.cpp
sources/Application/Instruments/WavFileWriter.cpp
```

Runtime log:

```text
/mnt/sdcard/lgpt/wav_export_debug.log
```
