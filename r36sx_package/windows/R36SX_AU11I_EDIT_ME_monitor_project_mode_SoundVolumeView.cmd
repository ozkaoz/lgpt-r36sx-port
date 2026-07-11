@echo off
REM AU11M optional Windows helper template.
REM Requires SoundVolumeView.exe in the same folder. Download separately from NirSoft.
REM Edit the three device names below to match your Windows names exactly.

set SVV=%~dp0SoundVolumeView.exe
set R36SX_MIC=Micrófono (6- R36SX USB Audio)
set R36SX_PLAY=Conector AUX interno (6- R36SX USB Audio)
set MONITOR_OUT=Altavoces (BEHRINGER UMC 1820)

if not exist "%SVV%" (
  echo Missing SoundVolumeView.exe beside this .cmd
  echo Place SoundVolumeView.exe in this folder, then edit device names.
  pause
  exit /b 1
)

"%SVV%" /SetListenToThisDevice "%R36SX_MIC%" 1
"%SVV%" /SetPlaybackThroughDevice "%R36SX_MIC%" "%MONITOR_OUT%"
"%SVV%" /SetDefault "%MONITOR_OUT%" all

echo R36SX project monitor mode requested.
pause
