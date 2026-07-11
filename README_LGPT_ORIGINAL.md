# Little Piggy Tracker

![Piggy](https://avatars.githubusercontent.com/u/180156201?s=400&u=ebb53bdea61a025edce0c3782ac75b532dd65dd7&v=4)

**Little Piggy Tracker** (f.k.a _'LittleGPTracker'_) is a music tracker optimised to run on portable game consoles. It is currently running on Windows, MacOS (intel/arm) & Linux, PSP, Miyoo Mini, and a collection of other retro gaming handhelds.

It implements the user interface of [littlesounddj](https://www.littlesounddj.com/lsd/index.php) and precedes [M8 tracker](https://www.dirtywave.com), two popular trackers greatly loved in the tracker community.

All versions are available for free under the [GPLv3 License](LICENSE). If you like the project and want to contribute, don't hesitate to make a pull request for this repo.

## About This Fork

This build is derivative of the work of the original author `m-.-n`
aka [Marc Nostromo](https://github.com/Mdashdotdashn/LittleGPTracker).
The original work and releasing the source code has laid the foundation for everything in this repo.

All implemented features have been tested not to break old
projects but make sure to backup your old cherished work
just to be safe &#9829;

## Releases

### Current Builds

Latest releases from this fork here:

- [Releases](https://github.com/djdiskmachine/LittleGPTracker/releases)


## Documentation

All the relevant documentation can be found in [Docs](docs) directory.

Recommended reading to get you started:

- [What is Little Piggy Tracker](docs/wiki/What-is-LittlePiggyTracker.md)
- [Quick-Start Guide](docs/wiki/quick_start_guide.md)
- [Little Piggy Tracker Configuration](docs/LittlePiggyTrackerConf.md)
- [Tips and Tricks](docs/wiki/tips_and_tricks.md)

## Features per platform

| Platform    | MIDI_Possible | MIDI_enabled | Soundfonts | Note                                 |
|-------------|---------------|--------------|------------|--------------------------------------|
| PSP         | NO            | NO           | YES        | [See notes](projects/resources/PSP/INSTALL_HOW_TO.txt) |
| DEB         | YES           | YES          | YES        |                                      |
| X64         | YES           | YES          | MAYBE      |                                      |
| X86         | YES           | YES          | YES        |                                      |
| STEAM       | YES           | YES          | MAYBE      |                                      |
| MIYOO       | NO            | NO           | YES        | Port by [Nine-H](https://ninethehacker.xyz) |
| W32         | YES           | YES          | YES        | Built in VS2008 with love            |
| RASPI       | YES           | YES          | YES        | Versatile platform                   |
| CHIP        | YES           | YES          | YES        | [See notes](projects/resources/CHIP/INSTALL_HOW_TO.txt) |
| BITTBOY     | MAYBE         | NO           | YES        |                                      |
| GARLIC      | NO         | NO           |NO        | No longer maintained, use Portmaster|
| GARLICPLUS  | MAYBE         | NO           | YES        | Port by [Simotek](http://simotek.net)|
| RG35XXPLUS  | MAYBE         | NO           | YES        | Port by [Simotek](http://simotek.net)|
| MACOS       | YES           | YES          | MAYBE      | Port by [clsource](https://genserver.social/clsource) |

* **MIDI functionality __greatly__ depends on kernel support, please feature request your favourite OS maintainer =)**
* **Install ffmpeg by following install instructions for your platform [here](https://www.ffmpeg.org/download.html)**
* **PrintFX requires full ffmpeg. If marked as TBA, it requires a redesign using [libav](https://trac.ffmpeg.org/wiki/Using%20libav*)**


## U2.38AU11M markers
U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG AU11U_FIRST_A_PRE_FIELDVIEW_GUARD AU11U_DUPLEX_CONTEXT_GATED_ALWAYS_VISIBLE AU11U_SCPI_R_USB_RECORD_SHORTCUT AU11_DISABLE_CHOPPER_L2A_USB_REC AU11_FAST_RECORD_MENU_NO_RELEASE_LAG


AU11U_DUPLEX_CONTEXT_GATED_ALWAYS_VISIBLE
AU11U_FIRST_INSTRUMENT_ENTRY_GUARD
AU11U_NO_GADGET_RECREATE_ON_RECORD


## AU11M notes

AU11M keeps the UAC2 descriptor duplex (`p_chmask=1`, `c_chmask=1`) so Windows can expose playback and recording at the same time. The important change is that the daemon continuously opens and drains `/dev/snd/pcmC0D0c` even outside USB-C RECORD. This prevents the Windows playback endpoint from back-pressuring the gadget and muting the opposite LGPT -> Windows path.

AU11M also removes the previous stop-on-navigation guard. The project may keep playing while Instrument is visible. Only the first plain A on the sample field is consumed to stop/yield/warm the sample import path; the second A opens Listen / Import / Manage / Exit.

Markers: AU11U_DUPLEX_STABLE_ALWAYS_OPEN_ENDPOINTS, AU11U_WINDOWS_MONITORING_FIX, AU11U_ALLOW_INSTRUMENT_VIEW_WHILE_PLAYING, AU11U_FIRST_MODAL_TRAMPOLINE.

## AU11M current correction note

Previous AU11M marker notes inherited from AU11H/AU11L may mention first-A guards or safe Instrument views. The effective AU11M correction is:

- `AU11U_REVERTED_INSTRUMENTVIEW_TO_FINAL_MIXER`: `InstrumentView.cpp` is restored from `LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE`.
- `AU11U_USB_REC_SHORTCUT_STABLE_INSTRUMENT`: only `Instrument + R1 + Right` was added to enter USB-C RECORD.
- `AU11U_CAPTURE_STAGING_COPY`: USB recordings are written first to `/mnt/sdcard/lgpt/usbrecs/` and then copied to `samples:` on `Save/load recording`.
- `AU11U_USB_RECORD_COUNTDOWN_120`: the 120 second countdown is wall-clock based.

This is a controlled comparison branch against the stable Final Mixer Master source, not another safe-view patch.
