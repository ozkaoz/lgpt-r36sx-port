@echo off
setlocal
cd /d "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\tools\windows-clean\R36SX_CLEAN_PRETEST_AUDIO_DEVICE_CACHE.ps1" -Mode Remove -RestartAudio
pause
