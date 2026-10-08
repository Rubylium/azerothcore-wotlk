param(
    # The run's steps: a Lua file setting FxLabSteps (see FxLabShots\FxLabShots.lua)
    [Parameter(Mandatory = $true)][string]$steps,
    # Where the screenshots go (01.jpg, 02.jpg, ... in the order taken)
    [string]$out = (Join-Path $PSScriptRoot '..\..\..\.agents\plans\fx-lab\shots'),
    [string]$character = 'Evoguerrier',
    [string]$resolution = '1600x900',
    [string]$realm = 'Evolutions',
    [int]$timeoutSeconds = 300
)
# LOCAL DEV ONLY. The FX lab's eyes (.agents/docs/systems/fx-lab.md): logs the EVODEV account's character into the
# world (the client extension's -login/-character), plays the steps with a dev addon installed for this run only, and
# collects the screenshots. Config.wtf is put back and the addon removed whatever happens. The world server must be up;
# a character left "online" by a killed client takes about a minute to come free.
$ErrorActionPreference = 'Stop'
$client = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK'
$config = Join-Path $client 'WTF\Config.wtf'
$saved = Join-Path $env:TEMP 'FxLabShots.Config.wtf'
$shots = Join-Path $client 'Screenshots'
$addon = Join-Path $client 'Interface\AddOns\FxLabShots'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item
New-Item -ItemType Directory -Force $shots | Out-Null
$before = @(Get-ChildItem $shots -File | ForEach-Object Name)

Copy-Item $config $saved -Force
New-Item -ItemType Directory -Force $addon | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'FxLabShots\*') $addon -Force
Copy-Item $steps (Join-Path $addon 'Steps.lua') -Force
$lines = Get-Content $saved | Where-Object { $_ -notmatch '^SET (gxResolution|gxMaximize|gxWindow|realmName|Sound_EnableAllSound) ' }
$lines += 'SET gxWindow "1"', 'SET gxMaximize "0"', "SET gxResolution ""$resolution""", "SET realmName ""$realm""",
    'SET Sound_EnableAllSound "0"'
Set-Content $config $lines -Encoding ASCII
try {
    $process = Start-Process (Join-Path $client 'Wow.exe') -WorkingDirectory $client -PassThru -ArgumentList '-login',
        'EVODEV', '-password', 'evodev', '-realmname', $realm, '-character', $character
    if (-not $process.WaitForExit($timeoutSeconds * 1000)) {
        $process.Kill()
        Write-Host 'timed out'
    }
}
finally {
    Copy-Item $saved $config -Force
    Remove-Item $addon -Recurse -Force -ErrorAction SilentlyContinue
}
$index = 0
Get-ChildItem $shots -File | Where-Object { $before -notcontains $_.Name } | Sort-Object LastWriteTime | ForEach-Object {
    $index++
    Move-Item $_.FullName (Join-Path $out ('{0:D2}{1}' -f $index, $_.Extension))
}
Get-ChildItem $out | ForEach-Object { $_.FullName }
