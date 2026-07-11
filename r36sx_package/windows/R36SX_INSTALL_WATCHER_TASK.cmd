@echo off
setlocal
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "SCRIPT=%~dp0R36SX_WATCH_AND_APPLY.ps1"
schtasks /Create /F /SC ONLOGON /TN "R36SX USB Audio Helper AU11M" /TR "\"%PS%\" -ExecutionPolicy Bypass -NoProfile -WindowStyle Hidden -File \"%SCRIPT%\"" 
echo Installed logon watcher task: R36SX USB Audio Helper AU11M
pause
