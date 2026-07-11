@echo off
REM AU11M Windows Host Helper config.
REM Use R36SX_LIST_AUDIO_DEVICES.cmd first and replace these values with Command-Line Friendly IDs.
REM Wildcards are accepted by SoundVolumeView 2.29+.

set "R36SX_RENDER=*R36SX*USB Audio*\Device\Speakers\Render"
set "R36SX_CAPTURE=*R36SX*USB Audio*\Device\Microphone\Capture"

REM IMPORTANT: This must be your physical PC output, NOT R36SX.
REM Examples: *BEHRINGER*\Device\Speakers\Render or *Realtek*\Device\Speakers\Render or *Headphones*\Device\Speakers\Render
set "MONITOR_RENDER=*BEHRINGER*\Device\Speakers\Render"

REM If you do not use BEHRINGER, change MONITOR_RENDER above before applying.
set "R36SX_SET_AS_DEFAULT_OUTPUT=1"
set "R36SX_SET_AS_DEFAULT_INPUT=1"
set "R36SX_DISABLE_EXCLUSIVE=1"
