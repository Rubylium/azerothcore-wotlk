# Runs ON the live VPS (pushed by deployLive.ps1): the VC++ runtime, MySQL as a service, the WoW ports.
# Safe to run again: each step skips what is already there.
param(
    [string]$root = 'C:\Evolutions',
    [string]$mysqlVersion = '8.4.9'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$mysqlRoot = Join-Path $root 'mysql'
$mysqlBin = Join-Path $mysqlRoot 'bin'
$mysqlData = Join-Path $root 'mysql-data'
$mysqlIni = Join-Path $mysqlRoot 'my.ini'
$serviceName = 'EvolutionsMySQL'

New-Item -ItemType Directory -Force $root | Out-Null

Write-Host '== Visual C++ runtime'
winget install --id Microsoft.VCRedist.2015+.x64 --exact --silent --accept-package-agreements --accept-source-agreements `
    --disable-interactivity | Out-Null
Write-Host 'Done.'

Write-Host '== Memory and clock'
# OVH's image has a fixed 512 MB pagefile: once the RAM was full, MySQL and the auth server died with no margin
$computer = Get-CimInstance Win32_ComputerSystem
if (-not $computer.AutomaticManagedPagefile) {
    Set-CimInstance -InputObject $computer -Property @{ AutomaticManagedPagefile = $true }
    Write-Host 'Pagefile managed by Windows (from the next reboot).'
}
# The players' time: the game's daily and weekly resets follow the server's clock
Set-TimeZone -Id 'Romance Standard Time'
Write-Host 'Done.'

Write-Host '== MySQL'
if (-not (Test-Path (Join-Path $mysqlBin 'mysqld.exe'))) {
    $zip = Join-Path $env:TEMP "mysql-$mysqlVersion.zip"
    $url = "https://cdn.mysql.com/Downloads/MySQL-8.4/mysql-$mysqlVersion-winx64.zip"
    Write-Host "Downloading $url"
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    $unpack = Join-Path $env:TEMP 'mysql-unpack'
    Remove-Item -Recurse -Force $unpack -ErrorAction SilentlyContinue
    Expand-Archive -Path $zip -DestinationPath $unpack
    Move-Item (Get-ChildItem $unpack -Directory | Select-Object -First 1).FullName $mysqlRoot
    Remove-Item -Force $zip
}

# Written every run: ours, so a setting changed here reaches an installed server
& {
    $iniRoot = $mysqlRoot -replace '\\', '/'
    $iniData = $mysqlData -replace '\\', '/'
    @"
[mysqld]
basedir=$iniRoot
datadir=$iniData
port=3306
# Never reachable from the internet: only the servers on this machine talk to it
bind-address=127.0.0.1
mysqlx=OFF
# Small: on 8 GB the worldserver needs the rest (it reached 5.3 GB with 30 bots and no player)
innodb_buffer_pool_size=512M
# No replication here: binary logs only filled the disk (560 MB in a night)
skip-log-bin
max_allowed_packet=256M
max_connections=300
character-set-server=utf8mb4
collation-server=utf8mb4_unicode_ci
"@ | Set-Content -Encoding ascii $mysqlIni
}

if (-not (Test-Path $mysqlData)) {
    & (Join-Path $mysqlBin 'mysqld.exe') "--defaults-file=$mysqlIni" --initialize-insecure --console
    if ($LASTEXITCODE) { throw 'mysqld --initialize failed' }
}

if (-not (Get-Service $serviceName -ErrorAction SilentlyContinue)) {
    & (Join-Path $mysqlBin 'mysqld.exe') --install $serviceName "--defaults-file=$mysqlIni"
    if ($LASTEXITCODE) { throw 'mysqld --install failed' }
}
Set-Service $serviceName -StartupType Automatic
# Brought back after a crash: the servers cannot run without it
sc.exe failure $serviceName reset= 86400 actions= restart/10000/restart/10000/restart/60000 | Out-Null
Restart-Service $serviceName

$mysql = Join-Path $mysqlBin 'mysql.exe'
for ($i = 0; $i -lt 30; $i++) {
    # Through cmd: under PowerShell 5.1 its "can't connect" on stderr would stop the script
    cmd.exe /c "`"$mysql`" -uroot -e `"SELECT 1`" >nul 2>nul"
    if (-not $LASTEXITCODE) { break }
    Start-Sleep -Seconds 1
}
& $mysql -uroot -e "CREATE USER IF NOT EXISTS 'acore'@'localhost' IDENTIFIED BY 'acore'; GRANT ALL PRIVILEGES ON *.* TO 'acore'@'localhost' WITH GRANT OPTION; FLUSH PRIVILEGES;"
if ($LASTEXITCODE) { throw 'Creating the acore user failed' }
Write-Host 'MySQL running (127.0.0.1:3306).'

Write-Host '== Firewall (auth 3724, world 8085)'
foreach ($port in 3724, 8085) {
    $name = "Evolutions-$port"
    if (-not (Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -Name $name -DisplayName "Evolutions WoW $port" -Direction Inbound -Protocol TCP `
            -LocalPort $port -Action Allow -Profile Any | Out-Null
    }
}
Write-Host 'Done.'
