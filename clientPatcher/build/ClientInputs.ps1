# What the class data (the spells' DBCs, the custom classes, the talent trees) is generated from: the scripts that run
# and the data files they read, by name wherever they are - a class's Spells.ps1, StockSpells.ps1, Looks.ps1,
# talentTree.json and visuals - rather than every script under localTools. A bench script, a boss's art builder or
# the deploy tooling no longer regenerates 40 s of spell data. A new data file read by patchSinisterStrike.ps1 must be
# named here (or -forceRebuild).
$classDataNames = @('Spells.ps1', 'StockSpells.ps1', 'Looks.ps1', 'TalentRankSpells.ps1', 'talentTree.json',
    'ascensionVisuals.json', 'classes.json', 'glyphItemDisplays.json', 'shapes.json', 'retailItems.generated.json',
    'patchSinisterStrike.ps1', 'buildRogueClientAssets.ps1', 'buildPestifereClientAssets.ps1',
    'buildNecromancerClientAssets.ps1')
$classDataScripts = @('localTools/oathblade/buildSounds.py', 'localTools/oathblade/buildBlueEffects.py',
    'localTools/mpq-builder/extractEffectiveClientFile.js', 'localTools/mpq-builder/package-lock.json',
    'localTools/fxLab/buildFxLab.py', 'localTools/customClasses', 'localTools/talentTree')

function Get-ClassDataInputs {
    $names = [Collections.Generic.HashSet[string]]::new([string[]]$classDataNames, [StringComparer]::OrdinalIgnoreCase)
    @(Get-BuildFiles @((Join-Path $repoRoot 'localTools'))) |
        Where-Object { $names.Contains($_.Name) -and $_.FullName -notmatch '\\(cache|bin|obj)\\' } |
        ForEach-Object { $_.FullName }
    foreach ($relative in $classDataScripts) { Join-Path $repoRoot $relative }
    # How it is run, not the rest of the build's scripts (stages.json, the cache)
    Join-Path $patcherRoot 'build/ClientGeneration.ps1'
    Join-Path $patcherRoot 'build/ClientInputs.ps1'
}

# Everything a release is built from, for its fingerprint (Get-ClientReleaseFingerprint): conservative on purpose,
# every script and data file under localTools, but the tooling that never feeds a release (benches, telemetry, map
# editing, art briefs and generation logs, the deploy and server scripts at the top of localTools).
$nonReleaseTooling = @(
    '\\localTools\\(combatBench|combatTelemetry|simBench|tuning|mapEditing|fxLab\\shots|audio\\capture)\\',
    '\\localTools\\[^\\]+\.ps1$', 'Generation\.json$', 'generationLog\.json$', '\\(cache|bin|obj)\\') -join '|'
$releaseScripts = @('patchSinisterStrike.ps1', 'buildRogueClientAssets.ps1', 'buildPestifereClientAssets.ps1',
    'buildNecromancerClientAssets.ps1')

function Get-ClientCodeInputs {
    @(Get-BuildFiles @((Join-Path $repoRoot 'localTools'))) |
        Where-Object { $_.Extension -in '.ps1', '.py', '.js', '.json' -and
            ($_.FullName -notmatch $nonReleaseTooling -or $_.Name -in $releaseScripts) } |
        ForEach-Object { $_.FullName }
    Join-Path $patcherRoot 'build'
    Join-Path $patcherRoot 'Build-FriendPatch.ps1'
    Join-Path $repoRoot 'localTools/interface/assets'
}

function Get-ClientAssetRoots([string]$part = '') {
    Get-ChildItem (Join-Path $repoRoot 'modules') -Directory | ForEach-Object {
        $assets = Join-Path $_.FullName 'client-assets'
        if ($part) { $assets = Join-Path $assets $part }
        if (Test-Path -LiteralPath $assets) { $assets }
    }
}

function Get-TalentArtInputs {
    foreach ($relative in 'interface/buildTalentTreeArt.py', 'interface/buildParagonArt.py',
        'interface/extractRetailUi.py', 'mpq-builder/extractClientFiles.js', 'mpq-builder/package-lock.json',
        'customClasses/classes.json', 'interface/cache/talents') {
        Join-Path $repoRoot "localTools/$relative"
    }
    $definitions = Read-BuildJson (Join-Path $repoRoot 'localTools/customClasses/classes.json')
    foreach ($entry in @($definitions.classes) + @($definitions.stockTalentTrees)) {
        if (-not $entry.talentTree) { continue }
        $treePath = Join-Path $repoRoot $entry.talentTree
        $treePath
        $tree = Read-BuildJson $treePath
        foreach ($definition in @($tree) + @($tree.trees)) {
            if ($definition.backgroundArt.file) { Join-Path $repoRoot $definition.backgroundArt.file }
            if ($definition.specArt.file) { Join-Path $repoRoot $definition.specArt.file }
        }
    }
    # The role badges (the support medallion)
    Join-Path $repoRoot 'localTools/interface/assets/roles'
    # The art generator resolves custom icons from TGA files under modules.
    Get-BuildFiles @(Get-ClientAssetRoots) | Where-Object { $_.Extension -eq '.tga' } |
        ForEach-Object { $_.FullName }
}

# --- The generators' stages (clientPatcher/build/stages.json): their inputs, for the build and the release check ---
function Get-StageManifest {
    Read-BuildJson (Join-Path $patcherRoot 'build/stages.json')
}

# A stage's path from the repository root, {client} and {awesome} replaced
function Resolve-StagePath([string]$path) {
    $resolved = $path.Replace('{client}', $clientPath).Replace('{awesome}', $awesomeWotlkPath)
    if ([IO.Path]::IsPathRooted($resolved)) { return $resolved }
    Join-Path $repoRoot $resolved
}

# The ground indicators' pictures: the art scripts shapes.json names (and the paintings they read from art/, as
# their SIZES list names them) and its image files
function Get-GroundIndicatorInputs {
    $shapes = Read-BuildJson (Join-Path $repoRoot 'localTools/groundIndicators/shapes.json')
    $scripts = @{}
    foreach ($shape in @($shapes.shapes)) {
        if ($shape.picture) { $scripts[($shape.picture -split ':')[0]] = $true }
        if ($shape.image) { Join-Path $repoRoot $shape.image }
    }
    foreach ($script in $scripts.Keys) {
        $scriptPath = Join-Path $repoRoot $script
        $scriptPath
        $artRoot = Join-Path (Split-Path -Parent $scriptPath) 'art'
        foreach ($match in [regex]::Matches([IO.File]::ReadAllText($scriptPath), "'([A-Za-z0-9_]+\.png)'\s*:")) {
            Join-Path $artRoot $match.Groups[1].Value
        }
    }
}

# The class and spec icons classes.json names, painted into Details' sheets
function Get-DetailsIconInputs {
    $definitions = Read-BuildJson (Join-Path $repoRoot 'localTools/customClasses/classes.json')
    foreach ($class in @($definitions.classes)) {
        if ($class.classIcon) { Join-Path $repoRoot $class.classIcon }
        foreach ($spec in @($class.specs)) {
            if ($spec.icon) { Join-Path $repoRoot $spec.icon }
        }
    }
}

function Get-StageInputs($stage) {
    @($stage.inputs) | ForEach-Object { Resolve-StagePath $_ }
    switch ($stage.inputsFrom) {
        'groundIndicators' { Get-GroundIndicatorInputs }
        'detailsIcons' { Get-DetailsIconInputs }
    }
}

# Every generator's inputs, for the release's fingerprint (paintings are not code: the fingerprint would miss them)
function Get-AllStageInputs {
    foreach ($stage in @((Get-StageManifest).generators)) { Get-StageInputs $stage }
}

function Get-StockClientStamp {
    # Stock archives can be many GB; size/time identify installations without rehashing the whole game.
    $stock = Get-ChildItem (Join-Path $clientPath 'Data') -File -Recurse -Filter '*.MPQ' |
        Where-Object { $_.Name -match '^(common(-2)?|expansion|lichking|patch(-[23])?|locale-[a-z]{2}[A-Z]{2}|(expansion|lichking)-locale-[a-z]{2}[A-Z]{2}|patch-[a-z]{2}[A-Z]{2}(-[23])?)\.MPQ$' }
    (@($stock | Sort-Object FullName | ForEach-Object {
        "$($_.FullName):$($_.Length):$($_.LastWriteTimeUtc.Ticks)"
    }) -join '|')
}

function Get-ClientReleaseFingerprint {
    $paths = @(Get-ClientCodeInputs) + @(Get-ClientAssetRoots) + @(Get-AllStageInputs) + @(
        (Join-Path $patcherRoot 'interface'), (Join-Path $patcherRoot 'addons'), (Join-Path $patcherRoot 'maps'),
        (Join-Path $patcherRoot 'assets'), (Join-Path $patcherRoot 'vendor'),
        (Join-Path $patcherRoot 'template'), (Join-Path $repoRoot 'server/Data/dbc'),
        (Join-Path $patcherRoot '.dbccache'), (Join-Path $repoRoot 'localTools/interface/cache/talents'),
        (Join-Path $clientPath 'Wow.exe.before-custom-classes'),
        (Join-Path $clientPath 'Interface/AddOns'), (Join-Path $clientPath 'Data/patch-[XYZ].MPQ'),
        (Join-Path $clientPath 'Data/patch-L.MPQ'), (Join-Path $clientPath 'Data/*/patch-*-[RS].MPQ'),
        (Join-Path $clientPath 'Data/*/patch-*-M.MPQ'),
        (Join-Path $awesomeWotlkPath 'build/Release/AwesomeWotlkLib.dll'),
        (Join-Path $awesomeWotlkPath 'deps/skia/skia.dll'),
        (Join-Path $awesomeWotlkPath 'src/AwesomeWotlkPatch/Patch.h'))
    Get-BuildFingerprint $paths (Get-StockClientStamp)
}

function Assert-ClientBuildReady([string]$root) {
    $ready = Read-BuildJson (Join-Path $root 'ready.json')
    if (-not $ready) { throw 'No completed client build. Run Build-FriendPatch.ps1 first.' }
    $clientPath = $ready.clientPath
    $awesomeWotlkPath = $ready.awesomeWotlkPath
    if ($ready.sources -ne (Get-ClientReleaseFingerprint) -or
        $ready.payload -ne (Get-BuildFingerprint @((Join-Path $root 'current')))) {
        throw 'Client sources or payload changed since the build. Run Build-FriendPatch.ps1 again.'
    }
    return $ready
}
