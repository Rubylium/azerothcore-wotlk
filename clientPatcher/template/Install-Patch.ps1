param(
    [string]$clientPath,
    [string]$serverAddress
)

$ErrorActionPreference = 'Stop'
$packageRoot = $PSScriptRoot
$payloadPath = Join-Path $packageRoot 'payload'
$manifestPath = Join-Path $packageRoot 'manifest.json'

function Read-RequiredValue {
    param([string]$prompt, [string]$value)
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        return $value.Trim().Trim('"')
    }
    return (Read-Host $prompt).Trim().Trim('"')
}

function Copy-WithBackup {
    param(
        [string]$sourcePath,
        [string]$destinationPath,
        [string]$backupPath,
        [string]$relativePath
    )

    if (Test-Path -LiteralPath $destinationPath) {
        $backupFile = Join-Path $backupPath $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $backupFile) -Force | Out-Null
        Copy-Item -LiteralPath $destinationPath -Destination $backupFile -Force
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $destinationPath) -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

if (-not (Test-Path -LiteralPath $manifestPath) -or -not (Test-Path -LiteralPath $payloadPath)) {
    throw 'This patch package is incomplete. Extract the entire ZIP before running the installer.'
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$clientPath = Read-RequiredValue 'WotLK 3.3.5a folder path' $clientPath
$serverAddress = Read-RequiredValue 'Server IP or hostname' $serverAddress
$wowPath = Join-Path $clientPath 'Wow.exe'

if (-not (Test-Path -LiteralPath $wowPath) -or -not (Test-Path -LiteralPath (Join-Path $clientPath 'Data'))) {
    throw 'The selected folder is not a valid WotLK client folder.'
}

$wowVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($wowPath)
if ($wowVersion.FileMajorPart -ne 3 -or $wowVersion.FileMinorPart -ne 3 -or
    $wowVersion.FileBuildPart -ne 5 -or $wowVersion.FilePrivatePart -ne 12340) {
    throw "Unsupported WoW client version: $($wowVersion.FileVersion). Required: 3.3.5a build 12340."
}

$runningClient = Get-Process -Name Wow -ErrorAction SilentlyContinue | Where-Object {
    try { [System.IO.Path]::GetFullPath($_.Path) -eq [System.IO.Path]::GetFullPath($wowPath) } catch { $false }
}
if ($runningClient) {
    throw 'Close the WoW client before installing the patch.'
}

if ($serverAddress -notmatch '^[a-zA-Z0-9][a-zA-Z0-9.:-]*$') {
    throw 'The server address contains invalid characters.'
}

foreach ($file in $manifest.files) {
    $sourceFile = Join-Path $payloadPath ($file.path.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $sourceFile)) {
        throw "Package file missing: $($file.path)"
    }
    if ((Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash -ne $file.sha256) {
        throw "Package checksum failed: $($file.path)"
    }
}

$backupPath = Join-Path $clientPath (Join-Path '_RubyEbonBackup' (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupPath -Force | Out-Null
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $backupPath 'installed-manifest.json')

foreach ($file in $manifest.files) {
    $relativePath = $file.path.Replace('/', '\')
    $sourceFile = Join-Path $payloadPath $relativePath
    $destinationFile = Join-Path $clientPath $relativePath
    Copy-WithBackup $sourceFile $destinationFile $backupPath $relativePath

    if ((Get-FileHash -LiteralPath $destinationFile -Algorithm SHA256).Hash -ne $file.sha256) {
        throw "Installed file verification failed: $($file.path)"
    }
}

# Wow.exe itself needs two changes: the awesome_wotlk loader, which loads AwesomeWotlkLib.dll (MSDF fonts and client
# fixes, copied above), and the Dungeon Finder roles of the custom classes, without which the client offers a
# custom class no role and refuses to queue it. The original Wow.exe is kept in the backup folder.
& (Join-Path $PSScriptRoot 'Patch-WowExe.ps1') -ClientPath $clientPath -BackupPath $backupPath

$localePattern = '^(enUS|enGB|frFR|deDE|esES|esMX|ruRU|koKR|zhCN|zhTW)$'
$localeFolders = Get-ChildItem -LiteralPath (Join-Path $clientPath 'Data') -Directory | Where-Object { $_.Name -match $localePattern }
if (-not $localeFolders) {
    throw 'No supported locale folder was found under Data.'
}

foreach ($localeFolder in $localeFolders) {
    $realmlistPath = Join-Path $localeFolder.FullName 'realmlist.wtf'
    if (Test-Path -LiteralPath $realmlistPath) {
        $relativePath = $realmlistPath.Substring($clientPath.Length + 1)
        $backupFile = Join-Path $backupPath $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $backupFile) -Force | Out-Null
        Copy-Item -LiteralPath $realmlistPath -Destination $backupFile -Force
    }
    "set realmlist $serverAddress" | Set-Content -LiteralPath $realmlistPath -Encoding ASCII
}

# Atlas shipped with 1.0.10 and 1.0.11, replaced by the built-in dungeon maps: move it into the backup
$removedAddonNames = @('Atlas', 'Atlas_Battlegrounds', 'Atlas_DungeonLocs', 'Atlas_OutdoorRaids', 'Atlas_Transportation')
foreach ($addonName in $removedAddonNames) {
    $relativeAddonPath = Join-Path 'Interface\AddOns' $addonName
    $addonPath = Join-Path $clientPath $relativeAddonPath
    if (Test-Path -LiteralPath $addonPath) {
        $addonBackupPath = Join-Path $backupPath $relativeAddonPath
        New-Item -ItemType Directory -Path (Split-Path -Parent $addonBackupPath) -Force | Out-Null
        Move-Item -LiteralPath $addonPath -Destination $addonBackupPath
    }
}

$addonNames = @('DungeonBots', 'PersonalLoot', 'DetailsCustomClasses')
Get-ChildItem -LiteralPath (Join-Path $clientPath 'WTF') -Filter 'AddOns.txt' -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
    $lines = @(Get-Content -LiteralPath $_.FullName -ErrorAction SilentlyContinue)
    foreach ($addonName in $addonNames) {
        $entry = "${addonName}: enabled"
        $existingIndex = -1
        for ($index = 0; $index -lt $lines.Count; $index++) {
            if ($lines[$index] -match "^$([regex]::Escape($addonName)):") {
                $existingIndex = $index
                break
            }
        }
        if ($existingIndex -ge 0) {
            $lines[$existingIndex] = $entry
        } else {
            $lines += $entry
        }
    }
    $lines | Set-Content -LiteralPath $_.FullName -Encoding UTF8
}

$cachePath = Join-Path $clientPath 'Cache'
if (Test-Path -LiteralPath $cachePath) {
    Move-Item -LiteralPath $cachePath -Destination (Join-Path $backupPath 'Cache')
}

Write-Host ''
Write-Host "Installed $($manifest.name) $($manifest.version) successfully." -ForegroundColor Green
Write-Host "Server: $serverAddress"
Write-Host "Backup: $backupPath"
Write-Host 'Start the game with Wow.exe.'
