# LOCAL DEV ONLY. EvolutionsAudio capture: the FX lab runner's client with sound on (effects only), the speakers
# recorded (WASAPI loopback) for the run. -local: the built DLL and the repository's sounds.txt for this run only.
param([switch]$local, [string]$wavName = 'capture.wav')
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $here '..\..\..')
$client = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK'
$config = Join-Path $client 'WTF\Config.wtf'
$saved = Join-Path $env:TEMP 'EvaCapture.Config.wtf'
$addon = Join-Path $client 'Interface\AddOns\FxLabShots'
$dll = Join-Path $client 'AwesomeWotlkLib.dll'
$bank = Join-Path $client 'Interface\AddOns\EvolutionsAudio\sounds.txt'
$wav = Join-Path $here $wavName
Copy-Item $config $saved -Force
if ($local) {
    Copy-Item $dll (Join-Path $env:TEMP 'EvaCapture.dll') -Force
    Copy-Item $bank (Join-Path $env:TEMP 'EvaCapture.sounds.txt') -Force
    Copy-Item 'C:\Users\alexi\Documents\GitHub\awesome_wotlk\build\Release\AwesomeWotlkLib.dll' $dll -Force
    Copy-Item (Join-Path $repo 'clientPatcher\addons\EvolutionsAudio\sounds.txt') $bank -Force
}
New-Item -ItemType Directory -Force $addon | Out-Null
Copy-Item (Join-Path $repo 'localTools\fxLab\shots\FxLabShots\*') $addon -Force
Copy-Item (Join-Path $here 'steps.lua') (Join-Path $addon 'Steps.lua') -Force
$drop = '^SET (gxResolution|gxMaximize|gxWindow|realmName|Sound_MasterVolume|Sound_MusicVolume|Sound_SFXVolume|' +
    'Sound_AmbienceVolume|Sound_EnableSFX|Sound_EnableAmbience|Sound_EnableMusic|Sound_EnableAllSound|' +
    'Sound_EnableSoundWhenGameIsInBG) '
$lines = Get-Content $saved | Where-Object { $_ -notmatch $drop }
$lines += 'SET gxWindow "1"', 'SET gxMaximize "0"', 'SET gxResolution "1280x720"', 'SET realmName "Evolutions"',
    'SET Sound_EnableAllSound "1"', 'SET Sound_MasterVolume "1"', 'SET Sound_EnableMusic "0"',
    'SET Sound_MusicVolume "0"', 'SET Sound_EnableSFX "1"', 'SET Sound_SFXVolume "1"',
    'SET Sound_EnableAmbience "0"', 'SET Sound_AmbienceVolume "0"', 'SET Sound_EnableSoundWhenGameIsInBG "1"'
Set-Content $config $lines -Encoding ASCII
$recorder = Start-Process python -ArgumentList (Join-Path $here 'record.py'), 110, $wav -PassThru -NoNewWindow
try {
    Start-Sleep -Seconds 1
    $process = Start-Process (Join-Path $client 'Wow.exe') -WorkingDirectory $client -PassThru -ArgumentList '-login',
        'EVODEV', '-password', 'evodev', '-realmname', 'Evolutions', '-character', 'Evoguerrier'
    if (-not $process.WaitForExit(105000)) { $process.Kill(); Write-Host 'client timed out' }
}
finally {
    Start-Sleep -Seconds 2
    Copy-Item $saved $config -Force
    if ($local) {
        Copy-Item (Join-Path $env:TEMP 'EvaCapture.dll') $dll -Force
        Copy-Item (Join-Path $env:TEMP 'EvaCapture.sounds.txt') $bank -Force
    }
    Remove-Item $addon -Recurse -Force -ErrorAction SilentlyContinue
}
$recorder.WaitForExit()
Write-Host 'done'
