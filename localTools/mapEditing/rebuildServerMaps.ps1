# Gives the server the maps edited with Noggit (clientPatcher/maps): its terrain heights and liquids (maps), its
# collision and line of sight (vmaps) and its pathing (mmaps) are extracted from the client, so an edited hill a
# creature walks through, or a new wall a spell passes, is the server still using the old map. README.md here.
#
# The extractors (build: TOOLS_BUILD=maps-only, the four map tools) only open the stock archive names, never our
# lettered patches: they read a staging copy of the client's stock archives (hard links, nothing copied) with the
# edits packed as patch-5.MPQ. Only the edited maps' files are copied into server/Data; its dbc (ours, custom) is never
# touched. Restart the world server afterwards.
param(
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK',
    # Map ids to rebuild; by default the ones that have edited files in clientPatcher/maps
    [int[]]$maps = @(),
    # Pathing for every tile of those maps (hours for a continent) rather than only the edited tiles
    [switch]$allTiles,
    # Keep the server's current collision data (terrain only edits: no object added, moved or removed)
    [switch]$skipVmaps,
    [int]$threads = [Math]::Max(1, [Environment]::ProcessorCount - 2)
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tools = Join-Path $repoRoot 'build\bin\RelWithDebInfo'
$mapsRoot = Join-Path $repoRoot 'clientPatcher\maps'
$serverData = Join-Path $repoRoot 'server\Data'
$work = Join-Path $PSScriptRoot '.work'
$staging = Join-Path $work 'client'
$output = Join-Path $work 'output'

foreach ($tool in 'map_extractor', 'vmap4_extractor', 'vmap4_assembler', 'mmaps_generator') {
    if (-not (Test-Path -LiteralPath (Join-Path $tools "$tool.exe"))) {
        throw "$tool.exe is not built (README.md: building the map tools)."
    }
}

# --- What was edited: the maps (Map.dbc's directory names) and the tiles (<Map>_<x>_<y>.adt) ---------------------------
function Read-MapDirectories {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $serverData 'dbc\Map.dbc'))
    $count = [BitConverter]::ToInt32($bytes, 4)
    $size = [BitConverter]::ToInt32($bytes, 12)
    $strings = 20 + $count * $size
    $byName = @{}
    for ($index = 0; $index -lt $count; ++$index) {
        $row = 20 + $index * $size
        $id = [BitConverter]::ToInt32($bytes, $row)
        $offset = [BitConverter]::ToInt32($bytes, $row + 4)
        $end = [Array]::IndexOf($bytes, [byte]0, $strings + $offset)
        $byName[[Text.Encoding]::ASCII.GetString($bytes, $strings + $offset, $end - $strings - $offset).ToLower()] = $id
    }
    return $byName
}

$directories = Read-MapDirectories
$tiles = @{}        # map id -> list of "x,y"
$mapsFolder = Join-Path $mapsRoot 'World\Maps'
if (Test-Path -LiteralPath $mapsFolder) {
    foreach ($adt in Get-ChildItem -LiteralPath $mapsFolder -Recurse -Filter '*.adt') {
        if ($adt.BaseName -notmatch '^(.+)_(\d+)_(\d+)$') { continue }     # _obj0, _tex0 split files are Cataclysm's
        $id = $directories[$Matches[1].ToLower()]
        if ($null -eq $id) { Write-Warning "No map named $($Matches[1]) in Map.dbc: $($adt.FullName)"; continue }
        if (-not $tiles.ContainsKey($id)) { $tiles[$id] = New-Object System.Collections.Generic.List[string] }
        $tiles[$id].Add("$($Matches[2]),$($Matches[3])")
    }
}
if ($maps.Count -eq 0) { $maps = @($tiles.Keys | Sort-Object) }
if ($maps.Count -eq 0) {
    Write-Host 'Nothing edited in clientPatcher/maps and no -maps given: nothing to rebuild.'
    return
}
Write-Host "Maps to rebuild: $($maps -join ', ')"

# --- The staging client: the stock archives, hard linked, and the edits as patch-5 -------------------------------------
if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $staging 'Data'), $output | Out-Null
$stock = '^(common(-2)?|expansion|lichking|patch(-[2-4])?|(expansion-|lichking-)?locale-[a-z]{2}[A-Z]{2}|' +
    'patch-[a-z]{2}[A-Z]{2}(-[2-4])?)\.MPQ$'
foreach ($archive in Get-ChildItem -LiteralPath (Join-Path $clientPath 'Data') -Recurse -File -Filter '*.MPQ') {
    if ($archive.Name -cnotmatch $stock) { continue }
    $relative = $archive.FullName.Substring((Join-Path $clientPath 'Data').Length + 1)
    $target = Join-Path (Join-Path $staging 'Data') $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    New-Item -ItemType HardLink -Path $target -Target $archive.FullName | Out-Null
}
if (Test-Path -LiteralPath $mapsFolder) {
    & node (Join-Path $repoRoot 'localTools\mpq-builder\buildMapArchive.js') $mapsRoot `
        (Join-Path $staging 'Data\patch-5.MPQ')
    if ($LASTEXITCODE -ne 0) { throw 'Packing the edited maps failed.' }
}

# --- Extraction ------------------------------------------------------------------------------------------------------
Push-Location $output
try {
    Write-Host 'Extracting the maps (terrain, liquids)...'
    & (Join-Path $tools 'map_extractor.exe') -i $staging -o $output -e 1
    if ($LASTEXITCODE -ne 0) { throw "map_extractor failed with exit code $LASTEXITCODE." }

    if ($skipVmaps) {
        # The pathing reads collision from beside the maps: the server's own
        New-Item -ItemType Junction -Path (Join-Path $output 'vmaps') -Target (Join-Path $serverData 'vmaps') | Out-Null
    }
    else {
        Write-Host 'Extracting the collision (vmaps)...'
        & (Join-Path $tools 'vmap4_extractor.exe') -d (Join-Path $staging 'Data')
        if ($LASTEXITCODE -ne 0) { throw "vmap4_extractor failed with exit code $LASTEXITCODE." }
        New-Item -ItemType Directory -Force -Path (Join-Path $output 'vmaps') | Out-Null
        & (Join-Path $tools 'vmap4_assembler.exe') Buildings vmaps
        if ($LASTEXITCODE -ne 0) { throw "vmap4_assembler failed with exit code $LASTEXITCODE." }
    }

    # The pathing generator's settings, its data beside it
    $config = Get-Content -LiteralPath (Join-Path $repoRoot 'src\tools\mmaps_generator\mmaps-config.yaml') -Raw
    $config = $config -replace '(?m)^(\s*dataDir:).*$', "`$1 `"$($output -replace '\\', '/')`""
    $configPath = Join-Path $output 'mmaps-config.yaml'
    Set-Content -LiteralPath $configPath -Value $config -Encoding ASCII
    foreach ($map in $maps) {
        $edited = if ($tiles.ContainsKey($map)) { @($tiles[$map] | Sort-Object -Unique) } else { @() }
        if ($allTiles -or $edited.Count -eq 0) {
            Write-Host "Pathing for every tile of map $map..."
            & (Join-Path $tools 'mmaps_generator.exe') $map --threads $threads --config $configPath --silent
            if ($LASTEXITCODE -ne 0) { throw "mmaps_generator failed for map $map." }
        }
        else {
            foreach ($tile in $edited) {
                Write-Host "Pathing for map $map, tile $tile..."
                & (Join-Path $tools 'mmaps_generator.exe') $map --tile $tile --threads $threads --config $configPath --silent
                if ($LASTEXITCODE -ne 0) { throw "mmaps_generator failed for map $map tile $tile." }
            }
        }
    }
}
finally {
    Pop-Location
}

# --- Into the server: the edited maps' files only ------------------------------------------------------------------
foreach ($map in $maps) {
    $prefix = '{0:D3}' -f $map
    Copy-Item -Path (Join-Path $output "maps\$prefix*.map") -Destination (Join-Path $serverData 'maps') -Force
    Copy-Item -Path (Join-Path $output "mmaps\$prefix*") -Destination (Join-Path $serverData 'mmaps') -Force
    if (-not $skipVmaps) {
        Copy-Item -Path (Join-Path $output "vmaps\$prefix*") -Destination (Join-Path $serverData 'vmaps') -Force
    }
}
if (-not $skipVmaps) {
    # The collision models a tile places (a building, a rock): shared by every map, the new ones added
    foreach ($model in Get-ChildItem -LiteralPath (Join-Path $output 'vmaps') -File | Where-Object { $_.Name -notmatch '^\d{3}' }) {
        $target = Join-Path (Join-Path $serverData 'vmaps') $model.Name
        if (-not (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $model.FullName -Destination $target }
    }
}
Write-Host "Server map data rebuilt for map(s) $($maps -join ', '). Restart the world server to load it."
