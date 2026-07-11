@echo off
setlocal
call "%~dp0R36SX_AUDIO_PROFILE_CONFIG.cmd"
set "SVV=%~dp0SoundVolumeView.exe"
if not exist "%SVV%" set "SVV=%~dp0svcl.exe"
if not exist "%SVV%" set "SVV=SoundVolumeView.exe"

echo Applying R36SX audio profile...
echo R36SX_RENDER=%R36SX_RENDER%
echo R36SX_CAPTURE=%R36SX_CAPTURE%
echo MONITOR_RENDER=%MONITOR_RENDER%

if "%R36SX_SET_AS_DEFAULT_OUTPUT%"=="1" (
  "%SVV%" /SetDefault "%R36SX_RENDER%" all /WaitForItem 30
)
if "%R36SX_SET_AS_DEFAULT_INPUT%"=="1" (
  "%SVV%" /SetDefault "%R36SX_CAPTURE%" all /WaitForItem 30
)

REM Route console/project audio received as a recording endpoint to the physical PC output.
"%SVV%" /SetListenToThisDevice "%R36SX_CAPTURE%" 1 /WaitForItem 30
"%SVV%" /SetPlaybackThroughDevice "%R36SX_CAPTURE%" "%MONITOR_RENDER%" /WaitForItem 30
"%SVV%" /SetVolume "%R36SX_CAPTURE%" 100 /WaitForItem 30
"%SVV%" /Unmute "%R36SX_CAPTURE%" /WaitForItem 30

if "%R36SX_DISABLE_EXCLUSIVE%"=="1" (
  "%SVV%" /SetAllowExclusive "%R36SX_RENDER%" 0 /WaitForItem 30
  "%SVV%" /SetExclusivePriority "%R36SX_RENDER%" 0 /WaitForItem 30
  "%SVV%" /SetAllowExclusive "%R36SX_CAPTURE%" 0 /WaitForItem 30
  "%SVV%" /SetExclusivePriority "%R36SX_CAPTURE%" 0 /WaitForItem 30
)

echo Done. If there is no monitoring, edit MONITOR_RENDER in R36SX_AUDIO_PROFILE_CONFIG.cmd.
