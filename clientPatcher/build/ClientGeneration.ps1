# The client build's generation, in order:
# 1. the generators of clientPatcher/build/stages.json (sounds, ground indicators, icons, the Wow.exe patches...), each
#    a cached step of its own: nothing has to be run by hand before a deploy any more;
# 2. the class data (patchSinisterStrike.ps1's spells, the custom classes, the talent trees), cached on the files it
#    is generated from (Get-ClassDataInputs) - not on every script under localTools;
# 3. our base patches (patch-X, -Y and -Z.MPQ, localTools/mpq-builder/buildPatch.js), built in the background while
# 4. the interface art and patches (patch-L, patch-<locale>-R and -S) are built.
# Each step's time goes to .build/timings.json (Save-BuildTimings).

function Invoke-StageGenerators([string]$toolStamp) {
    foreach ($stage in @((Get-StageManifest).generators)) {
        $inputs = @(Get-StageInputs $stage)
        $outputs = @($stage.outputs | ForEach-Object { Resolve-StagePath $_ })
        $command = @($stage.command | ForEach-Object {
            if ($_ -like 'localTools/*' -or $_ -like '{*') { Resolve-StagePath $_ } else { $_ }
        })
        Invoke-CachedBuildStep $stage.name $inputs $outputs -salt $toolStamp -force:$forceRebuild -action {
            Write-Host "Generating $($stage.label)..."
            Invoke-BuildTool $command[0] @($command | Select-Object -Skip 1) -label $stage.name
        }
    }
}

# The server's DBCs the class data writes: when they change, the server only reads them at its next start
function Get-ServerDbcStamp {
    $dbc = Join-Path $repoRoot 'server/Data/dbc'
    (@('Spell.dbc', 'SpellIcon.dbc', 'SkillLineAbility.dbc', 'ChrClasses.dbc', 'Item.dbc') | ForEach-Object {
        $path = Join-Path $dbc $_
        if (Test-Path -LiteralPath $path) { Get-CachedFileHash $path }
    }) -join '|'
}

function Invoke-ClientGeneration {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    $sourceAssets = @(Get-ClientAssetRoots 'source') + @(Get-ClientAssetRoots 'sounds')
    $stockStamp = Get-StockClientStamp
    $pythonVersion = & python -c 'import sys, PIL, numpy, pefile; print(sys.version, PIL.__version__, numpy.__version__, pefile.__version__)'
    if ($LASTEXITCODE -ne 0) { throw 'Client generator Python dependencies are unavailable.' }
    $nodeVersion = & node --version
    if ($LASTEXITCODE -ne 0) { throw 'Node.js is unavailable.' }
    $toolStamp = "$clientPath|$awesomeWotlkPath|$pythonVersion|$nodeVersion|$stockStamp"
    Add-BuildTiming 'toolCheck' $phase.Elapsed.TotalSeconds 'phase'

    Invoke-StageGenerators $toolStamp

    # The class data. The compiled icons are read by it (a spell's painted icon once it is there) and written by its
    # class asset builders: inputs it may change itself.
    $compiledIcons = @(Get-BuildFiles @(Get-ClientAssetRoots 'compiled') |
        Where-Object { $_.Extension -eq '.tga' -and $_.FullName -notmatch '\\indicators\\' } |
        ForEach-Object { $_.FullName })
    $mutableDataInputs = @((Join-Path $repoRoot 'server/Data/dbc/*before*'), (Join-Path $patcherRoot '.dbccache')) +
        $compiledIcons
    $dataInputs = @(Get-ClassDataInputs) + $sourceAssets + $mutableDataInputs
    $dataOutputs = @(
        (Join-Path $repoRoot 'server/Data/dbc'), (Join-Path $clientPath 'Data/DBFilesClient'),
        (Join-Path $patcherRoot 'interface/DBFilesClient'),
        (Join-Path $patcherRoot 'interface/Interface/FrameXML/CustomClasses.lua'),
        (Join-Path $patcherRoot 'interface/Interface/GlueXML/CustomClasses.lua'),
        (Join-Path $patcherRoot 'interface/Interface/FrameXML/TalentTreeData.lua'),
        (Join-Path $repoRoot 'modules/mod-custom-classes/data/sql/db-world/base/custom_classes_generated.sql'),
        (Join-Path $repoRoot 'modules/mod-custom-classes/data/sql/db-world/base/custom_talent_tree.sql'))
    $dataStamp = "$toolStamp|skipSpellData=$skipSpellData|skipInterfacePatches=$skipInterfacePatches"
    $serverDbcBefore = Get-ServerDbcStamp
    Invoke-CachedBuildStep 'classData' $dataInputs $dataOutputs -salt $dataStamp -force:$forceRebuild `
        -mutableInputs $mutableDataInputs -action {
        if (-not $skipSpellData) {
            Write-Host 'Compiling custom icons...'
            foreach ($script in 'buildRogueClientAssets.ps1', 'buildPestifereClientAssets.ps1',
                'buildNecromancerClientAssets.ps1', 'patchSinisterStrike.ps1') {
                Invoke-Timed $script { & (Join-Path $repoRoot "localTools/$script") }
            }
            # The FX lab's light rows (map 451): the server's DBC folder is not in git
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/fxLab/buildFxLab.py'), '--light-only') `
                -label 'buildFxLab'
        }
        Write-Host 'Generating custom classes...'
        Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/customClasses/buildCustomClasses.py'),
            '--client', $clientPath) -label 'buildCustomClasses'
        Write-Host 'Generating talent trees...'
        Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/talentTree/buildTalentTree.py')) `
            -label 'buildTalentTree'
    }
    if ((Get-ServerDbcStamp) -ne $serverDbcBefore) {
        Write-Host 'Server DBCs changed: restart the server to load them.'
    }

    # Our base patches in the background while the interface is built: they share nothing
    $basePatches = $null
    if (-not $skipSpellData) {
        Write-Host 'Building patch-X, -Y and -Z.MPQ...'
        $basePatchLog = Join-Path $buildRoot 'basePatches.log'
        $basePatches = [Diagnostics.Stopwatch]::StartNew()
        $basePatchProcess = Start-Process node -NoNewWindow -PassThru -RedirectStandardOutput $basePatchLog `
            -RedirectStandardError "$basePatchLog.err" -ArgumentList @(
                "`"$(Join-Path $repoRoot 'localTools/mpq-builder/buildPatch.js')`"", "`"$builtMpqRoot`"")
        # Windows PowerShell only keeps a started process's exit code once its handle has been read
        $null = $basePatchProcess.Handle
    }

    if (-not $skipInterfacePatches) {
        Invoke-CachedBuildStep 'glueLogo' @(
            (Join-Path $repoRoot 'localTools/interface/buildGlueLogo.py'),
            (Join-Path $patcherRoot 'assets/logo/WorldOfWarcraft-Evolution.png')) @(
            (Join-Path $patcherRoot 'assets/logo/Glues-WoW-WotLKLogo.blp')) -salt $pythonVersion -force:$forceRebuild -action {
            Write-Host 'Compiling Evolutions Glue-screen logo...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildGlueLogo.py')) `
                -label 'buildGlueLogo'
        }
        Invoke-CachedBuildStep 'paragonArt' @(
            (Join-Path $repoRoot 'localTools/interface/buildParagonArt.py'),
            (Join-Path $patcherRoot 'assets/paragon/source'),
            (Join-Path $patcherRoot 'interface/Interface/FrameXML/ParagonBoard.lua')) @(
            (Join-Path $patcherRoot 'interface/Interface/Paragon')) -salt $pythonVersion -force:$forceRebuild -action {
            Write-Host 'Compiling Paragon interface art...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildParagonArt.py')) `
                -label 'buildParagonArt'
        }
        # Spell balance/script changes do not require regenerating thousands of unchanged art files.
        $talentInputs = @(Get-TalentArtInputs)
        Invoke-CachedBuildStep 'talentArt' $talentInputs @(
            (Join-Path $patcherRoot 'interface/Interface/TalentTree'),
            (Join-Path $patcherRoot 'interface/Interface/FrameXML/TalentTreeArt.lua')) -salt $toolStamp -force:$forceRebuild `
            -mutableInputs @((Join-Path $repoRoot 'localTools/interface/cache/talents')) -action {
            Write-Host 'Compiling talent tree art...'
            Invoke-BuildTool python @((Join-Path $repoRoot 'localTools/interface/buildTalentTreeArt.py'), '--client',
                $clientPath) -label 'buildTalentTreeArt'
        }
        $interfaceInputs = @((Join-Path $patcherRoot 'interface'), (Join-Path $patcherRoot 'vendor'),
            (Join-Path $patcherRoot 'assets/logo/Glues-WoW-WotLKLogo.blp'),
            (Join-Path $repoRoot 'localTools/mpq-builder/*.js'),
            (Join-Path $repoRoot 'localTools/mpq-builder/package-lock.json'))
        $locale = Get-ClientLocale
        Invoke-CachedBuildStep 'interfacePatches' $interfaceInputs @(
            (Join-Path $clientPath 'Data/patch-L.MPQ'),
            (Join-Path $clientPath "Data/$locale/patch-$locale-R.MPQ"),
            (Join-Path $clientPath "Data/$locale/patch-$locale-S.MPQ")) -salt $toolStamp -force:$forceRebuild -action {
            Write-Host 'Building interface patches...'
            Invoke-BuildTool node @((Join-Path $repoRoot 'localTools/mpq-builder/buildInterfacePatch.js'), '--client',
                $clientPath) -label 'buildInterfacePatch'
        }
    }

    if ($basePatches) {
        $basePatchProcess.WaitForExit()
        Get-Content -LiteralPath $basePatchLog | Out-Host
        # Its own time (it ran beside the interface), not the wait for it
        Add-BuildTiming 'basePatches' ($basePatchProcess.ExitTime - $basePatchProcess.StartTime).TotalSeconds
        if ($basePatchProcess.ExitCode -ne 0) {
            Get-Content -LiteralPath "$basePatchLog.err" | Out-Host
            throw "buildPatch.js failed (exit $($basePatchProcess.ExitCode))."
        }
        foreach ($pack in $basePatchNames) {
            try { Copy-BuildFile (Join-Path $builtMpqRoot $pack) (Join-Path $clientPath "Data/$pack") }
            catch [IO.IOException] {
                $pendingPath = Join-Path $clientPath "_pending/$pack"
                Copy-BuildFile (Join-Path $builtMpqRoot $pack) $pendingPath
                Write-Warning "Client patch is locked; installed copy deferred to $pendingPath"
            }
        }
    }
}

function Get-ClientLocale {
    $locale = Get-ChildItem (Join-Path $clientPath 'Data') -Directory | Where-Object {
        Test-Path (Join-Path $_.FullName "locale-$($_.Name).MPQ")
    } | Select-Object -First 1
    if (-not $locale) { throw 'No installed client locale found.' }
    $locale.Name
}
