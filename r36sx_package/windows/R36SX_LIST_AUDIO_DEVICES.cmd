@echo off
setlocal
set "SVV=%~dp0SoundVolumeView.exe"
if not exist "%SVV%" set "SVV=%~dp0svcl.exe"
if not exist "%SVV%" set "SVV=SoundVolumeView.exe"
"%SVV%" /scomma "%~dp0r36sx_audio_devices.csv" /Columns "Name,Command-Line Friendly ID,Direction,Type,Device Name,Device State,Default,Default Multimedia,Default Communications,Volume Percent,Muted"
echo Generated: %~dp0r36sx_audio_devices.csv
pause
