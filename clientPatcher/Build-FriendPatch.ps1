param(
    [string]$version,
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK',
    # Package the current patch-Z.MPQ as is, without regenerating the spell data first
    [switch]$skipSpellData,
    # Reuse the installed glue/interface MPQs. Useful while WoW has its stock locale archives locked.
    [switch]$skipInterfacePatches,
    # awesome_wotlk (github.com/Rubylium/awesome_wotlk), built: MSDF font rendering and client fixes
    [string]$awesomeWotlkPath = 'C:\Users\alexi\Documents\GitHub\awesome_wotlk'
)

$ErrorActionPreference = 'Stop'

# The interface step rewrites the MPQs inside the client folder, which a running WoW holds open. Checking up
# front turns a failure several minutes in - after the icons, the DBCs and the class data have been rebuilt -
# into an immediate, obvious one.
if (-not $skipInterfacePatches -and (Get-Process -Name 'Wow' -ErrorAction SilentlyContinue)) {
    throw 'World of Warcraft is running and holds the patch archives. Close it, or pass -skipInterfacePatches.'
}
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
    & (Join-Path $repoRoot 'localTools\buildPestifereClientAssets.ps1')
    & (Join-Path $repoRoot 'localTools\buildNecromancerClientAssets.ps1')

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

# Custom playable classes: regenerates the class DBCs (client copies land in clientPatcher/interface, server
# copies in the worldserver dbc folder), the world SQL and the client class table used by the interface files.
Write-Host 'Generating custom classes...'
& python (Join-Path $repoRoot 'localTools\customClasses\buildCustomClasses.py') --client $clientPath | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Custom class generation failed (exit $LASTEXITCODE)."
}

# Every change made to Wow.exe, applied by the installer through Patch-WowExe.ps1: the awesome_wotlk loader (its
# bytes read from the fork's Patch.h) and the Dungeon Finder roles of the custom classes
Write-Host 'Generating the Wow.exe patches...'
& python (Join-Path $repoRoot 'localTools\clientExe\buildWowExePatch.py') --awesome-wotlk $awesomeWotlkPath | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Wow.exe patch generation failed (exit $LASTEXITCODE)."
}

# The custom classes' icons, painted into Details' class icon sheet (it has no cell for a class it does not know)
Write-Host 'Painting custom class icons for Details...'
& python (Join-Path $repoRoot 'localTools\interface\buildDetailsClassIcons.py') --client $clientPath | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Details class icon generation failed (exit $LASTEXITCODE)."
}

# Interface patches: RetailUI windows + Shadowlands character creation (patch-<locale>-R) and the Shadowlands
# login screen assets with the custom menu music (patch-L). Built straight into the client folder.
if (-not $skipInterfacePatches) {
    Write-Host 'Compiling Evolutions Glue-screen logo...'
    & python (Join-Path $repoRoot 'localTools\interface\buildGlueLogo.py') | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Evolutions logo build failed (exit $LASTEXITCODE)."
    }

    Write-Host 'Compiling Paragon interface art...'
    & python (Join-Path $repoRoot 'localTools\interface\buildParagonArt.py') | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Paragon art build failed (exit $LASTEXITCODE)."
    }

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
}
else {
    Write-Host 'Reusing installed interface patches.'
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
    # Registers the custom classes with Details, which errors on every bar for a class it does not know
    'Interface\AddOns\DetailsCustomClasses',
    # The interface the server is played with: DragonUI, and Details with its plugins (as installed in the client)
    'Interface\AddOns\DragonUI',
    'Interface\AddOns\DragonUI_Options',
    # Retail-style raid frames (Blizzard's Compact Raid Frames backported to a stock 3.3.5a client)
    'Interface\AddOns\CompactRaidFrame',
    'Interface\AddOns\Details',
    'Interface\AddOns\Details_3DModelsPaths',
    'Interface\AddOns\Details_ChartViewer',
    'Interface\AddOns\Details_DataStorage',
    'Interface\AddOns\Details_DeathGraphs',
    'Interface\AddOns\Details_EncounterDetails',
    'Interface\AddOns\Details_SunderCount',
    'Interface\AddOns\Details_TimeLine',
    'Interface\AddOns\Details_TinyThreat',
    # Details' class icon sheets with the custom classes painted in. Listed after the Details folder, so these
    # replace its stock sheets in the package
    'Interface\AddOns\Details\images\classes_small.tga',
    'Interface\AddOns\Details\images\classes_small_bw.tga',
    'Interface\AddOns\Details\images\classes_small_alpha.tga',
    'Interface\AddOns\Details\images\classes_small_alpha_bw.tga',
    # WDM-patch 2.4.5 (Trimitor, github.com/Trimitor/WDM-patch): built-in world map (M) for Classic and TBC
    # instances. Client-only map data and textures; it touches none of the DBCs in patch-Z.MPQ
    'Data\frFR\patch-frFR-M.MPQ',
    # RetailUI window chrome and the Shadowlands character creation screen with retail round icons
    'Data\frFR\patch-frFR-R.MPQ',
    # Shadowlands login screen assets (gongel / warfoll02) and the custom menu music
    'Data\patch-L.MPQ',
    # awesome_wotlk: loaded by Wow.exe once Patch-WowExe.ps1 has patched it in. skia.dll is its renderer and
    # must sit next to it, or the library does not load at all.
    'AwesomeWotlkLib.dll',
    'skia.dll'
)

# Files built outside this repository ship from where they are built, never from a client folder
$externalSources = @{
    'AwesomeWotlkLib.dll' = Join-Path $awesomeWotlkPath 'build\Release\AwesomeWotlkLib.dll'
    'skia.dll' = Join-Path $awesomeWotlkPath 'deps\skia\skia.dll'
}

$repoAddonPath = Join-Path $patcherRoot 'addons'
foreach ($relativePath in $sources) {
    $repoSourcePath = Join-Path $repoAddonPath ($relativePath -replace '^Interface\\AddOns\\', '')
    $sourcePath = if ($relativePath -eq 'Data\patch-Z.MPQ') {
        $packageMpqPath
    }
    elseif ($externalSources.ContainsKey($relativePath)) {
        $externalSources[$relativePath]
    }
    elseif ($relativePath -like 'Interface\AddOns\*' -and (Test-Path -LiteralPath $repoSourcePath) -and
        ((Test-Path -LiteralPath $repoSourcePath -PathType Leaf) -or
         (Get-ChildItem -LiteralPath $repoSourcePath -Filter '*.toc' -File))) {
        # Addons and single addon files kept in the repository ship from there; the client copy is only for local
        # testing. A repository folder without a .toc (addons\Details only holds the painted icon sheets) is not an
        # addon: the addon itself comes from the client.
        $repoSourcePath
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
