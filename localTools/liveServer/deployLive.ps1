# Pushes the local server (the PTR's build) to the live VPS and restarts it there.
#   deployLive.ps1                  binaries, configs (with the live overrides below), SQL updates
#   deployLive.ps1 -data            also the 3 GB of maps / vmaps / mmaps / dbc (after a map or DBC change)
#   deployLive.ps1 -copyDatabases   REPLACES the live databases with copies of the local ones (accounts, characters...)
# The VPS is prepared once with setupVpsSsh.bat (SSH) and remote\installPrereqs.ps1 (MySQL, runtime, ports).
param(
    [string]$vps = 'Administrator@51.77.214.33',
    [string]$publicAddress = '51.77.214.33',
    [switch]$data,
    [switch]$copyDatabases
)
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$serverRoot = Join-Path $repoRoot 'server'
$remoteRoot = 'C:/Evolutions'
$localMysqlBin = 'C:\laragon\bin\mysql\mysql-8.0.30-winx64\bin'
$localMysqlPort = 3307

# What the live server does differently from the PTR: its own MySQL and folders, fewer random bots.
# Instance bots (Dungeon Finder, Raid Finder, M+, Défi) come from the content-bot pool, not from these 30.
$liveDatabase = '127.0.0.1;3306;acore;acore'
$overrides = @{
    'worldserver.conf' = @{
        LoginDatabaseInfo = "`"$liveDatabase;acore_auth`""
        WorldDatabaseInfo = "`"$liveDatabase;acore_world`""
        CharacterDatabaseInfo = "`"$liveDatabase;acore_characters`""
        SourceDirectory = "`"$remoteRoot/source`""
        MySQLExecutable = "`"$remoteRoot/mysql/bin/mysql.exe`""
    }
    'authserver.conf' = @{
        LoginDatabaseInfo = "`"$liveDatabase;acore_auth`""
        SourceDirectory = "`"$remoteRoot/source`""
        MySQLExecutable = "`"$remoteRoot/mysql/bin/mysql.exe`""
    }
    'modules\playerbots.conf' = @{
        PlayerbotsDatabaseInfo = "`"$liveDatabase;acore_playerbots`""
        'AiPlayerbot.MinRandomBots' = '30'
        'AiPlayerbot.MaxRandomBots' = '30'
        # The bot-only M+ test teams would keep ~30 more bots online for nobody
        'RaidFinder.AutoMythicTeams' = '0'
    }
}

$stage = Join-Path $env:TEMP 'evolutions-live'
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
$stageServer = Join-Path $stage 'server'
New-Item -ItemType Directory -Force $stageServer, (Join-Path $stage 'db') | Out-Null

function Invoke-Tar([string[]]$arguments) {
    & "$env:SystemRoot\System32\tar.exe" @arguments
    if ($LASTEXITCODE) { throw "tar $arguments failed" }
}

Write-Host '== Server binaries and live configs'
Get-ChildItem $serverRoot -File | Where-Object { $_.Extension -in '.exe', '.dll' } | Copy-Item -Destination $stageServer
Copy-Item -Recurse (Join-Path $serverRoot 'configs') $stageServer
foreach ($file in $overrides.Keys) {
    $path = Join-Path $stageServer "configs\$file"
    $text = [IO.File]::ReadAllText($path)
    foreach ($key in $overrides[$file].Keys) {
        $pattern = "(?m)^$([regex]::Escape($key))[ \t]*=.*$"
        if ($text -notmatch $pattern) { throw "$key not found in $file" }
        $text = [regex]::Replace($text, $pattern, "$key = $($overrides[$file][$key])")
    }
    [IO.File]::WriteAllText($path, $text)
}
Invoke-Tar @('-cf', (Join-Path $stage 'server.tar'), '-C', $stageServer, '.')

Write-Host '== SQL updates'
# Only what the updater reads (updates_include and the modules' sql folders), never the 600 MB of base dumps
$sqlFolders = @('data/sql/archive', 'data/sql/custom', 'data/sql/updates') +
    (Get-ChildItem (Join-Path $repoRoot 'modules') -Directory |
        Where-Object { Test-Path (Join-Path $_.FullName 'data\sql') } |
        ForEach-Object { "modules/$($_.Name)/data/sql" })
Invoke-Tar (@('-cf', (Join-Path $stage 'source.tar'), '-C', $repoRoot) + $sqlFolders)

if ($data) {
    Write-Host '== Data'
    Invoke-Tar @('-cf', (Join-Path $stage 'data.tar'), '-C', $serverRoot, 'Data')
}

if ($copyDatabases) {
    Write-Host '== Local databases'
    foreach ($database in 'acore_auth', 'acore_world', 'acore_characters', 'acore_playerbots') {
        $dump = Join-Path $stage "db\$database.sql"
        # Its password warning on stderr would stop the script (PowerShell 5.1 and native stderr)
        $ErrorActionPreference = 'Continue'
        & (Join-Path $localMysqlBin 'mysqldump.exe') -uacore -pacore -h 127.0.0.1 -P $localMysqlPort --single-transaction `
            --quick --hex-blob --set-gtid-purged=OFF --no-tablespaces --routines --triggers `
            --default-character-set=utf8mb4 "--result-file=$dump" --databases $database 2>$null
        $ErrorActionPreference = 'Stop'
        if ($LASTEXITCODE) { throw "Dumping $database failed" }
        $in = [IO.File]::OpenRead($dump)
        $out = [IO.File]::Create("$dump.gz")
        try {
            $gzip = New-Object IO.Compression.GZipStream($out, [IO.Compression.CompressionLevel]::Fastest)
            $in.CopyTo($gzip)
            $gzip.Dispose()
        } finally {
            $out.Dispose(); $in.Dispose()
        }
        Remove-Item -Force $dump
    }
}

Write-Host "== Upload to $vps"
$ssh = @('-o', 'BatchMode=yes')
& ssh @ssh $vps "New-Item -ItemType Directory -Force $remoteRoot/tools, $remoteRoot/incoming/db | Out-Null"
& scp -q @ssh (Join-Path $PSScriptRoot 'remote\*.ps1') "${vps}:$remoteRoot/tools/"
& scp -q @ssh (Get-ChildItem $stage -Filter '*.tar').FullName "${vps}:$remoteRoot/incoming/"
$dumps = @(Get-ChildItem (Join-Path $stage 'db') -Filter '*.gz')
if ($dumps) { & scp -q @ssh $dumps.FullName "${vps}:$remoteRoot/incoming/db/" }
if ($LASTEXITCODE) { throw 'Upload failed' }

Write-Host '== Apply on the VPS'
$apply = "powershell -NoProfile -ExecutionPolicy Bypass -File $remoteRoot/tools/applyDeploy.ps1 -publicAddress $publicAddress"
if ($copyDatabases) { $apply += ' -importDatabases' }
& ssh @ssh $vps $apply
if ($LASTEXITCODE) { throw 'Applying the deploy on the VPS failed' }

Remove-Item -Recurse -Force $stage
Write-Host "Live server deployed to $publicAddress."
