function Invoke-ClientGeneration {
    $codeInputs = @(Get-ClientCodeInputs)
    $sourceAssets = @(Get-ClientAssetRoots 'source') + @(Get-ClientAssetRoots 'sounds')
    $compiledAssets = @(Get-ClientAssetRoots 'compiled')
    $stockStamp = Get-StockClientStamp
    $pythonVersion = & python -c 'import sys, PIL, numpy, pefile; print(sys.version, PIL.__version__, numpy.__version__, pefile.__version__)'
    if ($LASTEXITCODE -ne 0) { throw 'Client generator Python dependencies are unavailable.' }
    $nodeVersion = & node --version
    if ($LASTEXITCODE -ne 0) { throw 'Node.js is unavailable.' }
    $toolStamp = "$clientPath|$awesomeWotlkPath|$pythonVersion|$nodeVersion|$stockStamp"
    $dataInputs = $codeInputs + $sourceAssets + @(
        (Join-Path $repoRoot 'server/Data/dbc/*before*'), (Join-Path $patcherRoot '.dbccache'),
        (Join-Path $awesomeWotlkPath 'src/AwesomeWotlkPatch/Patch.h'),
        (Join-Path $clientPath 'Wow.exe.before-custom-classes'),
        (Join-Path $clientPath 'Interface/AddOns/Details/images/classes_small*.tga'))
    $dataOutputs = $compiledAssets + @(
        (Join-Path $repoRoot 'server/Data/dbc'), (Join-Path $clientPath 'Data/DBFilesClient'),
        (Join-Path $patcherRoot 'interface/DBFilesClient'),
        (Join-Path $patcherRoot 'interface/Interface/FrameXML/CustomClasses.lua'),
        (Join-Path $patcherRoot 'interface/Interface/GlueXML/CustomClasses.lua'),
        (Join-Path $patcherRoot 'interface/Interface/FrameXML/TalentTreeData.lua'),
        (Join-Path $repoRoot 'modules/mod-custom-classes/data/sql/db-world/base/custom_classes_generated.sql'),
        (Join-Path $repoRoot 'modules/mod-custom-classes/data/sql/db-world/base/custom_talent_tree.sql'),
        (Join-Path $patcherRoot 'template/WowExePatch.json'), (Join-Path $patcherRoot 'addons/Details/images'))
    $dataStamp = "$toolStamp|skipSpellData=$skipSpellData|skipInterfacePatches=$skipInterfacePatches"
    $mutableDataInputs = @((Join-Path $repoRoot 'server/Data/dbc/*before*'),
        (Join-Path $patcherRoot '.dbccache'),
        (Join-Path $clientPath 'Interface/AddOns/Details/images/classes_small*.tga'))
    Invoke-CachedBuildStep 'classData' $dataInputs $dataOutputs -salt $dataStamp -force:$forceRebuild `
        -mutableInputs $mutableDataInputs -action {
        if (-not $skipSpellData) {
            Write-Host 'Compiling custom icons...'
            foreach ($script in 'buildRogueClientAssets.ps1', 'buildPestifereClientAssets.ps1',
                'buildNecromancerClientAssets.ps1', 'patchSinisterStrike.ps1') {
                & (Join-Path $repoRoot "localTools/$script")
            }
            # The FX lab's light rows (map 451): the server's DBC folder is not in git
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/fxLab/buildFxLab.py'), '--light-only')
        }
        # These icons must exist before patch-Z is built, not one release later.
        if (-not $skipInterfacePatches -or -not $skipSpellData) {
            Write-Host 'Compiling Paragon node icons...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/paragon/buildParagonIcons.py'))
        }
        Write-Host 'Generating custom classes...'
        Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/customClasses/buildCustomClasses.py'),
            '--client', $clientPath)
        Write-Host 'Generating talent trees...'
        Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/talentTree/buildTalentTree.py'))
        Write-Host 'Generating the Wow.exe patches...'
        Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/clientExe/buildWowExePatch.py'),
            '--awesome-wotlk', $awesomeWotlkPath)
        Write-Host 'Painting custom class icons for Details...'
        Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildDetailsClassIcons.py'),
            '--client', $clientPath)
    }
    if (-not $skipSpellData) {
        Write-Host 'Building patch-Z.MPQ...'
        Invoke-BuildTool node @((Join-Path $repoRoot 'localTools/mpq-builder/buildPatch.js'), $builtMpqPath)
        try { Copy-BuildFile $builtMpqPath $clientMpqPath }
        catch [IO.IOException] {
            $pendingPath = Join-Path $clientPath '_pending/patch-Z.MPQ'
            Copy-BuildFile $builtMpqPath $pendingPath
            Write-Warning "Client patch is locked; installed copy deferred to $pendingPath"
        }
    }
    if (-not $skipInterfacePatches) {
        Invoke-CachedBuildStep 'glueLogo' @(
            (Join-Path $repoRoot 'localTools/interface/buildGlueLogo.py'),
            (Join-Path $patcherRoot 'assets/logo/WorldOfWarcraft-Evolution.png')) @(
            (Join-Path $patcherRoot 'assets/logo/Glues-WoW-WotLKLogo.blp')) -salt $pythonVersion -force:$forceRebuild -action {
            Write-Host 'Compiling Evolutions Glue-screen logo...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildGlueLogo.py'))
        }
        Invoke-CachedBuildStep 'paragonArt' @(
            (Join-Path $repoRoot 'localTools/interface/buildParagonArt.py'),
            (Join-Path $patcherRoot 'assets/paragon/source'),
            (Join-Path $patcherRoot 'interface/Interface/FrameXML/ParagonBoard.lua')) @(
            (Join-Path $patcherRoot 'interface/Interface/Paragon')) -salt $pythonVersion -force:$forceRebuild -action {
            Write-Host 'Compiling Paragon interface art...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildParagonArt.py'))
        }
        # Spell balance/script changes do not require regenerating thousands of unchanged art files.
        $talentInputs = @(Get-TalentArtInputs)
        Invoke-CachedBuildStep 'talentArt' $talentInputs @(
            (Join-Path $patcherRoot 'interface/Interface/TalentTree'),
            (Join-Path $patcherRoot 'interface/Interface/FrameXML/TalentTreeArt.lua')) -salt $toolStamp -force:$forceRebuild `
            -mutableInputs @((Join-Path $repoRoot 'localTools/interface/cache/talents')) -action {
            Write-Host 'Compiling talent tree art...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildTalentTreeArt.py'), '--client', $clientPath)
        }
        $interfaceInputs = @((Join-Path $patcherRoot 'interface'), (Join-Path $patcherRoot 'vendor'),
            (Join-Path $patcherRoot 'assets/logo/Glues-WoW-WotLKLogo.blp'),
            (Join-Path $repoRoot 'localTools/mpq-builder/*.js'),
            (Join-Path $repoRoot 'localTools/mpq-builder/package-lock.json'))
        $locale = Get-ChildItem (Join-Path $clientPath 'Data') -Directory | Where-Object {
            Test-Path (Join-Path $_.FullName "locale-$($_.Name).MPQ")
        } | Select-Object -First 1
        if (-not $locale) { throw 'No installed client locale found.' }
        Invoke-CachedBuildStep 'interfacePatches' $interfaceInputs @(
            (Join-Path $clientPath 'Data/patch-L.MPQ'),
            (Join-Path $locale.FullName "patch-$($locale.Name)-R.MPQ")) -salt $toolStamp -force:$forceRebuild -action {
            Write-Host 'Building interface patches...'
            Invoke-BuildTool node @((Join-Path $repoRoot 'localTools/mpq-builder/buildInterfacePatch.js'), '--client', $clientPath)
        }
    }
}
