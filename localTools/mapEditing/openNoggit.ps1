# Opens the map editor (Noggit 3, localTools/mapEditing/.deps/noggit, README.md here) set up for this server: it reads
# the game client's archives (ours included) and saves what is edited, at its archive path, into clientPatcher/maps -
# which the client build packs into patch-X, and rebuildServerMaps.ps1 gives the server.
param(
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK',
    # Noggit's faster renderer, on the graphics cards that support it (bindless textures)
    [switch]$bindless
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$noggitRoot = Join-Path $PSScriptRoot '.deps\noggit'
$projectPath = Join-Path $repoRoot 'clientPatcher\maps'

if (-not (Test-Path -LiteralPath (Join-Path $noggitRoot 'noggit.exe'))) {
    throw "Noggit is not installed in $noggitRoot (README.md: how to get it)."
}
if (-not (Test-Path -LiteralPath (Join-Path $clientPath 'Wow.exe'))) {
    throw "WotLK client not found at: $clientPath"
}
New-Item -ItemType Directory -Force -Path $projectPath | Out-Null

# What Noggit reads: a view of the client - its archives hard linked, and only the locale folders that hold a locale
# (the launcher leaves a realmlist.wtf in an enUS folder; Noggit takes any locale folder for the game's, tries enUS
# first and dies on DBC files it does not have). Made again every time: a rebuilt patch-X is a new file.
$viewPath = Join-Path $PSScriptRoot '.view'
if (Test-Path -LiteralPath $viewPath) {
    # Remove the junctions without following them into the client
    Get-ChildItem -LiteralPath (Join-Path $viewPath 'Data') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { [IO.Directory]::Delete($_.FullName) }
    Remove-Item -LiteralPath $viewPath -Recurse -Force
}
$viewData = Join-Path $viewPath 'Data'
New-Item -ItemType Directory -Force -Path $viewData | Out-Null
foreach ($archive in Get-ChildItem -LiteralPath (Join-Path $clientPath 'Data') -File -Filter '*.MPQ') {
    New-Item -ItemType HardLink -Path (Join-Path $viewData $archive.Name) -Target $archive.FullName | Out-Null
}
foreach ($locale in Get-ChildItem -LiteralPath (Join-Path $clientPath 'Data') -Directory) {
    if (Test-Path -LiteralPath (Join-Path $locale.FullName "locale-$($locale.Name).MPQ")) {
        New-Item -ItemType Junction -Path (Join-Path $viewData $locale.Name) -Target $locale.FullName | Out-Null
    }
}

# Its settings (Qt's ini format, beside the executable): the game it reads, the project it writes to
$settings = @"
[project]
game_path=$(($viewPath -replace '\\', '/').TrimEnd('/'))/
path=$(($projectPath -replace '\\', '/').TrimEnd('/'))/
"@
$settingsPath = Join-Path $noggitRoot 'settings.ini'
if (Test-Path -LiteralPath $settingsPath) {
    # Keep everything else Noggit saved (its own options, window state); only the two paths are ours
    $existing = Get-Content -LiteralPath $settingsPath -Raw
    $existing = $existing -replace '(?m)^game_path=.*$', "game_path=$(($viewPath -replace '\\', '/').TrimEnd('/'))/"
    $existing = $existing -replace '(?m)^path=.*$', "path=$(($projectPath -replace '\\', '/').TrimEnd('/'))/"
    if ($existing -notmatch '(?m)^game_path=') { $existing = $settings + "`r`n" + $existing }
    Set-Content -LiteralPath $settingsPath -Value $existing -Encoding ASCII
}
else {
    Set-Content -LiteralPath $settingsPath -Value $settings -Encoding ASCII
}

$executable = if ($bindless) { 'noggit_bindless.exe' } else { 'noggit.exe' }
Start-Process -FilePath (Join-Path $noggitRoot $executable) -WorkingDirectory $noggitRoot
Write-Host "Noggit started: game $clientPath, edits saved to $projectPath"
