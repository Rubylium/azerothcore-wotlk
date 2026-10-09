param(
    [string]$version,
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK',
    # Package the installed patch-X, -Y and -Z.MPQ as they are, without regenerating the spell data first
    [switch]$skipSpellData,
    # Reuse the installed glue/interface MPQs. Useful while WoW has its stock locale archives locked.
    [switch]$skipInterfacePatches,
    # awesome_wotlk (github.com/Rubylium/awesome_wotlk), built: MSDF font rendering and client fixes
    [string]$awesomeWotlkPath = 'C:\Users\alexi\Documents\GitHub\awesome_wotlk',
    [switch]$forceRebuild
)

$ErrorActionPreference = 'Stop'

# The interface step rewrites the MPQs inside the client folder, which a running WoW holds open. Checking up
# front turns a failure several minutes in - after the icons, the DBCs and the class data have been rebuilt -
# into an immediate, obvious one.
# Only a 3.3.5 client holds the patch archives: the retail game (_retail_, _classic_...) runs as Wow.exe too
$wotlkRunning = Get-Process -Name 'Wow' -ErrorAction SilentlyContinue |
    Where-Object { -not $_.Path -or $_.Path -notmatch '\\_(retail|classic|ptr|beta)[a-z_]*_\\' }
if (-not $skipInterfacePatches -and $wotlkRunning) {
    throw 'World of Warcraft is running and holds the patch archives. Close it, or pass -skipInterfacePatches.'
}
$patcherRoot = $PSScriptRoot
$repoRoot = Split-Path -Parent $patcherRoot
$buildRoot = Join-Path $patcherRoot '.build'
$buildCacheRoot = Join-Path $buildRoot 'cache'
. (Join-Path $patcherRoot 'build/BuildCache.ps1')
. (Join-Path $patcherRoot 'build/ClientInputs.ps1')
. (Join-Path $patcherRoot 'build/ClientGeneration.ps1')
. (Join-Path $patcherRoot 'build/ClientPayload.ps1')
$buildLock = Open-BuildLock $buildRoot
$previousForce = $env:EVOLUTIONS_FORCE_REBUILD
$buildTimer = [Diagnostics.Stopwatch]::StartNew()
try {
if ($forceRebuild) { $env:EVOLUTIONS_FORCE_REBUILD = '1' }
$readyPath = Join-Path $buildRoot 'ready.json'
$templatePath = Join-Path $patcherRoot 'template'
$stagePath = Join-Path $buildRoot 'current'
$payloadPath = Join-Path $stagePath 'payload'
# Our base patches (localTools/mpq-builder/buildPatch.js): built there, then copied into the client
$builtMpqRoot = Join-Path $repoRoot 'localTools/mpq-builder'
$basePatchNames = @('patch-X.MPQ', 'patch-Y.MPQ', 'patch-Z.MPQ')
if (-not (Test-Path -LiteralPath (Join-Path $clientPath 'Wow.exe'))) {
    throw "WotLK client not found at: $clientPath"
}

# Nothing changed since the last build (its sources, its payload): it stands as it is, in a few seconds. A deploy of
# the news or the launcher alone no longer rebuilds the client.
$ready = Read-BuildJson $readyPath
if (-not $forceRebuild -and -not $version -and -not $skipSpellData -and -not $skipInterfacePatches -and $ready -and
    $ready.clientPath -eq $clientPath -and $ready.awesomeWotlkPath -eq $awesomeWotlkPath -and
    $ready.sources -eq (Get-ClientReleaseFingerprint) -and $ready.payload -eq (Get-BuildFingerprint @($stagePath))) {
    Write-Host ("Client unchanged since build {0} ({1:N1}s)" -f $ready.version, $buildTimer.Elapsed.TotalSeconds)
    Write-Host "Client build ready: $($ready.version)"
    Write-Host "Payload: $payloadPath"
    return
}
# Invalidate before touching any output: a failed build must never be publishable.
if (Test-Path -LiteralPath $readyPath) { Remove-Item -LiteralPath $readyPath -Force }
if ([string]::IsNullOrWhiteSpace($version)) {
    $current = Read-BuildJson (Join-Path $stagePath 'manifest.json')
    $latest = if ($current) { [version]$current.version } else {
        Get-ChildItem (Join-Path $patcherRoot 'dist') -Filter 'CustomWotLKClientPatch-*.zip' -File -ErrorAction SilentlyContinue |
            Where-Object { $_.BaseName -match '^CustomWotLKClientPatch-\d+\.\d+\.\d+$' } |
            ForEach-Object { [version]($_.BaseName -replace '^CustomWotLKClientPatch-', '') } |
            Sort-Object -Descending | Select-Object -First 1
    }
    $version = if ($latest) { "$($latest.Major).$($latest.Minor).$($latest.Build + 1)" } else { '1.0.0' }
}
Invoke-ClientGeneration
$phase = [Diagnostics.Stopwatch]::StartNew()
New-Item -ItemType Directory -Path $payloadPath -Force | Out-Null
Copy-BuildFile (Join-Path $templatePath 'WowExePatch.json') (Join-Path $stagePath 'WowExePatch.json')

$sources = @(
    # Our base patches, split by how often they change (localTools/mpq-builder/patchFiles.js packOf)
    'Data\patch-X.MPQ',
    'Data\patch-Y.MPQ',
    'Data\patch-Z.MPQ',
    'Interface\AddOns\DungeonBots',
    'Interface\AddOns\PersonalLoot',
    # The sounds of our own sound engine (the client extension DLL's EvolutionsAudio), built by
    # localTools/audio/buildAudio.py: the engine reads them from there, nothing in the folder loads as an addon
    'Interface\AddOns\EvolutionsAudio',
    # Registers the custom classes with Details, which errors on every bar for a class it does not know
    'Interface\AddOns\DetailsCustomClasses',
    # The interface the server is played with: DragonUI, and Details with its plugins (as installed in the client)
    'Interface\AddOns\DragonUI',
    'Interface\AddOns\DragonUI_Options',
    # DragonUI's bag tint judged armor and weapons from stock class tables (mail red on a warrior, everything red
    # on a custom class); ours trusts the tooltip. Listed after the DragonUI folder, so it replaces the stock file
    'Interface\AddOns\DragonUI\modules\bags_usability.lua',
    # DragonUI's item levels read GetItemInfo, which only knows a legendary's base item (every copy shares its id);
    # ours asks FrameXML Legendary.lua for the copy's own rolled level. Replaces the stock file the same way
    'Interface\AddOns\DragonUI\modules\itemlevel.lua',
    # The character sheet's stat sidebar with the stat overflow (crit held at its cap, each cap and what lies past it,
    # the Surplus section: .agents/docs/systems/stat-overflow.md). Replaces the stock file the same way
    'Interface\AddOns\DragonUI\modules\characterpanel\sidebar.lua',
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
    # RetailUI window chrome and the Shadowlands character creation screen with retail round icons (the art), then
    # our interface's code over it (S: a Lua edit repacks a few MB, not the art's hundreds)
    'Data\frFR\patch-frFR-R.MPQ',
    'Data\frFR\patch-frFR-S.MPQ',
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
$payloadSources = @{}
foreach ($relativePath in $sources) {
    $repoSourcePath = Join-Path $repoAddonPath ($relativePath -replace '^Interface\\AddOns\\', '')
    $sourcePath = if ($relativePath -match '^Data\\patch-[XYZ]\.MPQ$') {
        $name = Split-Path -Leaf $relativePath
        if ($skipSpellData) { Join-Path $clientPath "Data\$name" } else { Join-Path $builtMpqRoot $name }
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

    Add-PayloadSource $payloadSources $sourcePath $relativePath
}
Sync-ClientPayload $payloadSources $payloadPath
Add-BuildTiming 'payload' $phase.Elapsed.TotalSeconds 'phase'
$phase.Restart()

$files = Get-ChildItem -LiteralPath $payloadPath -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = $_.FullName.Substring($payloadPath.Length + 1).Replace('\', '/')
        size = $_.Length
        sha256 = Get-CachedFileHash $_.FullName
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
Add-BuildTiming 'manifest' $phase.Elapsed.TotalSeconds 'phase'
$phase.Restart()

Write-BuildJson $readyPath @{
    version = $version
    clientPath = $clientPath
    awesomeWotlkPath = $awesomeWotlkPath
    sources = (Get-ClientReleaseFingerprint)
    payload = (Get-BuildFingerprint @($stagePath))
}
Add-BuildTiming 'readyRecord' $phase.Elapsed.TotalSeconds 'phase'
Save-BuildTimings (Join-Path $buildRoot 'timings.json') $buildTimer.Elapsed.TotalSeconds
Write-Host ("Client build ready: {0} ({1:N1}s)" -f $version, $buildTimer.Elapsed.TotalSeconds)
Write-Host "Payload: $payloadPath"
}
finally {
    $env:EVOLUTIONS_FORCE_REBUILD = $previousForce
    Save-FileHashCache
    $buildLock.Dispose()
}
