# Runs ON the live VPS (called by deployLive.ps1): unpacks what was pushed to incoming\, then restarts the servers.
param(
    [string]$root = 'C:\Evolutions',
    [string]$publicAddress = '51.77.214.33',
    [switch]$importDatabases
)
$ErrorActionPreference = 'Stop'

$incoming = Join-Path $root 'incoming'
$serverRoot = Join-Path $root 'server'
$sourceRoot = Join-Path $root 'source'
$mysql = Join-Path $root 'mysql\bin\mysql.exe'
$control = Join-Path $PSScriptRoot 'serverControl.ps1'

function Expand-Tar([string]$archive, [string]$destination) {
    New-Item -ItemType Directory -Force $destination | Out-Null
    tar.exe -xf $archive -C $destination
    if ($LASTEXITCODE) { throw "Unpacking $archive failed" }
    Remove-Item -Force $archive
}

if (Get-ScheduledTask -TaskName EvolutionsWorld -ErrorAction SilentlyContinue) {
    & $control stop
}

$serverTar = Join-Path $incoming 'server.tar'
if (Test-Path $serverTar) {
    Write-Host '== Server binaries and configs'
    Expand-Tar $serverTar $serverRoot
}

$dataTar = Join-Path $incoming 'data.tar'
if (Test-Path $dataTar) {
    Write-Host '== Data (maps, vmaps, mmaps, dbc)'
    Remove-Item -Recurse -Force (Join-Path $serverRoot 'Data') -ErrorAction SilentlyContinue
    Expand-Tar $dataTar $serverRoot
}

$sourceTar = Join-Path $incoming 'source.tar'
if (Test-Path $sourceTar) {
    Write-Host '== SQL updates'
    # Replaced whole, so a file removed from the repo is gone here too
    Remove-Item -Recurse -Force $sourceRoot -ErrorAction SilentlyContinue
    Expand-Tar $sourceTar $sourceRoot
}

if ($importDatabases) {
    Write-Host '== Databases (replacing the live ones)'
    foreach ($dump in Get-ChildItem (Join-Path $incoming 'db') -Filter '*.sql.gz') {
        $database = $dump.Name -replace '\.sql\.gz$', ''
        $sql = Join-Path $env:TEMP "$database.sql"
        $in = [IO.File]::OpenRead($dump.FullName)
        $out = [IO.File]::Create($sql)
        try {
            $gzip = New-Object IO.Compression.GZipStream($in, [IO.Compression.CompressionMode]::Decompress)
            $gzip.CopyTo($out)
        } finally {
            $out.Dispose(); $in.Dispose()
        }
        Write-Host "Importing $database..."
        & $mysql -uroot -e "DROP DATABASE IF EXISTS ``$database``;"
        cmd.exe /c "`"$mysql`" -uroot --default-character-set=utf8mb4 < `"$sql`""
        if ($LASTEXITCODE) { throw "Importing $database failed" }
        Remove-Item -Force $sql, $dump.FullName
    }
    # The realm players are sent to: this machine, not the PTR's address the dump carries
    & $mysql -uroot -e "UPDATE acore_auth.realmlist SET address = '$publicAddress', localAddress = '127.0.0.1' WHERE id = 1;"
    Write-Host "Realm address set to $publicAddress."
}

& $control register
& $control start
