# AU11M R36SX Host Helper watcher.
# Poll-based helper: detects R36SX USB Audio in SoundVolumeView CSV and applies the audio profile.
param(
  [int]$PollSeconds = 3,
  [int]$MaxMinutes = 0
)
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$apply = Join-Path $here "R36SX_APPLY_AUDIO_PROFILE.cmd"
$list = Join-Path $here "R36SX_LIST_AUDIO_DEVICES.cmd"
$csv = Join-Path $here "r36sx_audio_devices.csv"
$lastSeen = $false
$connectWav = Join-Path $env:WINDIR "Media\Windows Hardware Insert.wav"
$disconnectWav = Join-Path $env:WINDIR "Media\Windows Hardware Remove.wav"
function Play-R36SXSound([string]$Path) {
  try {
    if (Test-Path $Path) {
      $p = New-Object System.Media.SoundPlayer $Path
      $p.Play()
    } else {
      [System.Media.SystemSounds]::Asterisk.Play()
    }
  } catch { }
}
$deadline = if ($MaxMinutes -gt 0) { (Get-Date).AddMinutes($MaxMinutes) } else { $null }
while ($true) {
  if ($deadline -and (Get-Date) -gt $deadline) { break }
  cmd /c "`"$list`"" | Out-Null
  $seen = $false
  if (Test-Path $csv) {
    $txt = Get-Content $csv -Raw -ErrorAction SilentlyContinue
    if ($txt -match 'R36SX|R 36SX|R36S|R 36S') { $seen = $true }
  }
  if ($seen -and -not $lastSeen) {
    Play-R36SXSound $connectWav
    cmd /c "`"$apply`"" | Out-Null
  }
  if ((-not $seen) -and $lastSeen) {
    Play-R36SXSound $disconnectWav
  }
  $lastSeen = $seen
  Start-Sleep -Seconds $PollSeconds
}
