param(
    [string]$version,
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK',
    # Package the current patch-Z.MPQ as is, without regenerating the spell data first
    [switch]$skipSpellData
)

$ErrorActionPreference = 'Stop'
$patcherRoot = $PSScriptRoot
$repoRoot = Split-Path -Parent $patcherRoot
$templatePath = Join-Path $patcherRoot 'template'
$distPath = Join-Path $patcherRoot 'dist'
$stagePath = Join-Path $patcherRoot '.stage'
$payloadPath = Join-Path $stagePath 'payload'
$clientMpqPath = Join-Path $clientPath 'Data\patch-Z.MPQ'
$packageMpqPath = $clientMpqPath

if (-not (Test-Path -LiteralPath (Join-Path $clientPath 'Wow.exe'))) {
    throw "WotLK client not found at: $clientPath"
}

# Default version: bump the patch number of the newest x.y.z package in dist
if ([string]::IsNullOrWhiteSpace($version)) {
    $latest = Get-ChildItem -LiteralPath $distPath -Filter 'CustomWotLKClientPatch-*.zip' -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            if ($_.BaseName -match '^CustomWotLKClientPatch-(\d+)\.(\d+)\.(\d+)$') {
                [version]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
            }
        } |
        Sort-Object -Descending |
        Select-Object -First 1
    $version = if ($latest) { "$($latest.Major).$($latest.Minor).$($latest.Build + 1)" } else { '1.0.0' }
}

# Regenerate custom spell data (Spell.dbc, SkillLineAbility.dbc, SpellIcon.dbc) and rebuild patch-Z.MPQ,
# so every spell change made in localTools/patchSinisterStrike.ps1 is part of the package.
if (-not $skipSpellData) {
    $mpqBuilderPath = Join-Path $repoRoot 'localTools\mpq-builder'
    $builtMpqPath = Join-Path $mpqBuilderPath 'patch-Z.MPQ'

    Write-Host 'Compiling custom icons...'
    & (Join-Path $repoRoot 'localTools\buildRogueClientAssets.ps1')

    Write-Host 'Patching spell data...'
    & (Join-Path $repoRoot 'localTools\patchSinisterStrike.ps1')

    Write-Host 'Building patch-Z.MPQ...'
    Push-Location $mpqBuilderPath
    try {
        & node buildPatch.js $builtMpqPath | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "patch-Z.MPQ build failed (exit $LASTEXITCODE)."
        }
    }
    finally {
        Pop-Location
    }

    # Always package the freshly built MPQ, even when the running client has its installed copy locked.
    $packageMpqPath = $builtMpqPath
    try {
        Copy-Item -LiteralPath $builtMpqPath -Destination $clientMpqPath -Force -ErrorAction Stop
        Write-Host "Installed client patch: $clientMpqPath"
    }
    catch [System.IO.IOException] {
        $pendingMpqPath = Join-Path $clientPath '_pending\patch-Z.MPQ'
        New-Item -ItemType Directory -Path (Split-Path -Parent $pendingMpqPath) -Force | Out-Null
        Copy-Item -LiteralPath $builtMpqPath -Destination $pendingMpqPath -Force
        Write-Warning "WoW is using patch-Z.MPQ. The updated patch was staged at: $pendingMpqPath"
    }
}

# Interface patches: RetailUI windows + Shadowlands character creation (patch-<locale>-R) and the Shadowlands
# login screen assets with the custom menu music (patch-L). Built straight into the client folder.
Write-Host 'Building interface patches...'
Push-Location (Join-Path $repoRoot 'localTools\mpq-builder')
try {
    & node buildInterfacePatch.js --client $clientPath | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Interface patch build failed (exit $LASTEXITCODE). Close WoW if it holds the patch files."
    }
}
finally {
    Pop-Location
}

if (Test-Path -LiteralPath $stagePath) {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}

New-Item -ItemType Directory -Path $payloadPath -Force | Out-Null
New-Item -ItemType Directory -Path $distPath -Force | Out-Null
Copy-Item -Path (Join-Path $templatePath '*') -Destination $stagePath -Recurse -Force

$sources = @(
    'Data\patch-Z.MPQ',
    'Interface\AddOns\DungeonBots',
    'Interface\AddOns\PersonalLoot',
    # WDM-patch 2.4.5 (Trimitor, github.com/Trimitor/WDM-patch): built-in world map (M) for Classic and TBC
    # instances. Client-only map data and textures; it touches none of the DBCs in patch-Z.MPQ
    'Data\frFR\patch-frFR-M.MPQ',
    # RetailUI window chrome and the Shadowlands character creation screen with retail round icons
    'Data\frFR\patch-frFR-R.MPQ',
    # Shadowlands login screen assets (gongel / warfoll02) and the custom menu music
    'Data\patch-L.MPQ'
)

foreach ($relativePath in $sources) {
    $sourcePath = if ($relativePath -eq 'Data\patch-Z.MPQ') {
        $packageMpqPath
    }
    else {
        Join-Path $clientPath $relativePath
    }
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Required client patch source is missing: $sourcePath"
    }

    $destinationPath = Join-Path $payloadPath $relativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $destinationPath) -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Recurse -Force
}

$excludedDirectoryNames = @('.git', '.kilo', '.vscode', '.idea', 'node_modules')
Get-ChildItem -LiteralPath $payloadPath -Directory -Recurse -Force |
    Where-Object { $_.Name -in $excludedDirectoryNames } |
    Sort-Object { $_.FullName.Length } -Descending |
    Remove-Item -Recurse -Force

$excludedFilePatterns = @('*.bak', '*.tmp', '*.log')
foreach ($pattern in $excludedFilePatterns) {
    Get-ChildItem -LiteralPath $payloadPath -File -Recurse -Force -Filter $pattern | Remove-Item -Force
}

$files = Get-ChildItem -LiteralPath $payloadPath -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = $_.FullName.Substring($payloadPath.Length + 1).Replace('\', '/')
        size = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
}

$manifest = [ordered]@{
    name = 'Custom WotLK Client Patch'
    version = $version
    requiredClient = 'Wrath of the Lich King 3.3.5a (build 12340)'
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
    files = @($files)
}

$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stagePath 'manifest.json') -Encoding UTF8

$archivePath = Join-Path $distPath "CustomWotLKClientPatch-$version.zip"
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}

Compress-Archive -Path (Join-Path $stagePath '*') -DestinationPath $archivePath -CompressionLevel Optimal
$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
Remove-Item -LiteralPath $stagePath -Recurse -Force

Write-Host "Patch created: $archivePath"
Write-Host "SHA256: $archiveHash"
