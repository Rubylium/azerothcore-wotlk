# Inputs are intentionally conservative: a new class/script/asset also invalidates the relevant build.
function Get-ClientCodeInputs {
    @(Get-BuildFiles @((Join-Path $repoRoot 'localTools'))) |
        Where-Object { $_.Extension -in '.ps1', '.py', '.js', '.json' -and
            $_.FullName -notmatch '\\(cache|bin|obj|combatBench|combatTelemetry)\\' } |
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

function Get-StockClientStamp {
    # Stock archives can be many GB; size/time identify installations without rehashing the whole game.
    $stock = Get-ChildItem (Join-Path $clientPath 'Data') -File -Recurse -Filter '*.MPQ' |
        Where-Object { $_.Name -match '^(common(-2)?|expansion|lichking|patch(-[23])?|locale-[a-z]{2}[A-Z]{2}|(expansion|lichking)-locale-[a-z]{2}[A-Z]{2}|patch-[a-z]{2}[A-Z]{2}(-[23])?)\.MPQ$' }
    (@($stock | Sort-Object FullName | ForEach-Object {
        "$($_.FullName):$($_.Length):$($_.LastWriteTimeUtc.Ticks)"
    }) -join '|')
}

function Get-ClientReleaseFingerprint {
    $paths = @(Get-ClientCodeInputs) + @(Get-ClientAssetRoots) + @(
        (Join-Path $patcherRoot 'interface'), (Join-Path $patcherRoot 'addons'), (Join-Path $patcherRoot 'maps'),
        (Join-Path $patcherRoot 'assets'), (Join-Path $patcherRoot 'vendor'),
        (Join-Path $patcherRoot 'template'), (Join-Path $repoRoot 'server/Data/dbc'),
        (Join-Path $patcherRoot '.dbccache'), (Join-Path $repoRoot 'localTools/interface/cache/talents'),
        (Join-Path $clientPath 'Wow.exe.before-custom-classes'),
        (Join-Path $clientPath 'Interface/AddOns'), (Join-Path $clientPath 'Data/patch-Z.MPQ'),
        (Join-Path $clientPath 'Data/patch-L.MPQ'), (Join-Path $clientPath 'Data/*/patch-*-R.MPQ'),
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
