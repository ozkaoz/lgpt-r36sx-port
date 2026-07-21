# English installation guide

## Requirements

- R36SX handheld with TreeFrogUI already installed.
- SD card accessible from Windows/WSL.
- `LGPT_R36SX_U2523.zip` downloaded from GitHub Releases.

TreeFrogUI and `picoarch` are not included.

## Installation

1. Fully power off the handheld and remove the SD card.
2. Insert the SD card into the PC.
3. Extract the release ZIP.
4. Open WSL in the extracted folder.
5. Run:

```bash
SD_MOUNT=/mnt/f bash INSTALL_TO_SD.sh
SD_MOUNT=/mnt/f bash VERIFY_SD_INSTALL.sh
sync
```

The installer creates a backup before copying files.

## First test

1. Safely eject the SD card.
2. Boot without OTG and test LGPT controls and local audio.
3. Fully power off.
4. Connect USB-C OTG, boot, and select `R36SX USB AUDIO 48K` in Windows.

The validated profile is mono, 48 kHz, 16-bit, 480 frames and four periods.
