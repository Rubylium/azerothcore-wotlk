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

if (-not (Test-Path $mysqlIni)) {
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
innodb_buffer_pool_size=1G
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
Start-Service $serviceName

$mysql = Join-Path $mysqlBin 'mysql.exe'
for ($i = 0; $i -lt 30; $i++) {
    & $mysql -uroot -e 'SELECT 1' 2>$null | Out-Null
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
