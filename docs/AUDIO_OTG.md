# USB-C OTG audio architecture

LGPT runs internally at 44.1 kHz. The R36SX USB Audio gadget operates at 48 kHz, S16_LE. The port uses continuous fixed-ratio resampling (`160/147`) and an ALSA-clocked playback path.

Validated safe profile:

- USB rate: 48,000 Hz.
- Channels: mono.
- Period: 480 frames.
- Period count: 4.
- USB daemon ABI: 7.
- Capture ABI: 2.

LGPT project audio is sent to Windows through USB. Windows-to-console monitoring is only active inside Record when Input Monitor is enabled.
