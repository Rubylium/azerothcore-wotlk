# The simulation bench's shared pieces (simBench.ps1, simWorkerHost.ps1): where its workers live, their databases and
# configuration, and the request files a worker host answers (the bench session's protocol, bench.ps1). Dot-sourced.
# Guide: .agents/docs/systems/sim-bench.md.

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$serverRoot = Join-Path $repositoryRoot 'server'
$buildOutput = Join-Path $repositoryRoot 'build\bin\RelWithDebInfo'
$simRoot = Join-Path $repositoryRoot 'var\simBench'
$simBin = Join-Path $simRoot 'bin'
$simDumps = Join-Path $simRoot 'dumps'
$mysqlBin = 'C:\laragon\bin\mysql\mysql-8.0.30-winx64\bin'

# The live realm's databases: the workers copy characters, playerbots and auth, and share the world (read only: no
# database update, live tuning in memory - LiveTuning.cpp)
$dbHost = '127.0.0.1'
$dbPort = 3307
$dbUser = 'acore'
$dbPassword = 'acore'
$simCopied = @('characters', 'playerbots', 'auth')
$liveDatabase = @{ characters = 'acore_characters'; playerbots = 'acore_playerbots'; auth = 'acore_auth' }

# The bench's owner in every worker: a game master character of the live realm, copied with the rest (Sim.Owner)
$simOwnerName = 'Evoguerrier'

function Get-SimDatabaseName([int]$worker, [string]$kind) { "acore_sim${worker}_$kind" }
function Get-SimWorkerDirectory([int]$worker) { Join-Path $simRoot "w$worker" }

function Invoke-SimMysql([string[]]$arguments, [string]$inputFile) {
    $all = @("--host=$dbHost", "--port=$dbPort", "--user=$dbUser", '--protocol=TCP') + $arguments
    $env:MYSQL_PWD = $dbPassword
    if ($inputFile) {
        # mysql reads the dump itself (a PowerShell pipe would re-encode it)
        $all += @('-e', "source $($inputFile.Replace('\', '/'))")
    }
    $output = & (Join-Path $mysqlBin 'mysql.exe') @all 2>&1
    if ($LASTEXITCODE -ne 0) { throw "mysql failed: $($output -join ' ')" }
    $output | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] }
}

# --- Preparing the workers -------------------------------------------------------------------------------------------

# The world server built (build\bin\RelWithDebInfo) beside the live install's libraries: the workers run that copy,
# never the live server's files, so a build for the simulation never touches the realm players are on
function Update-SimBinaries {
    New-Item -ItemType Directory -Force $simBin | Out-Null
    $built = Join-Path $buildOutput 'worldserver.exe'
    if (-not (Test-Path $built)) { throw "No built world server at $built (localTools\buildServer.ps1)" }
    Copy-Item $built, (Join-Path $buildOutput 'worldserver.pdb') $simBin -Force -ErrorAction SilentlyContinue
    Get-ChildItem $serverRoot -Filter '*.dll' | Copy-Item -Destination $simBin -Force
    # A name of its own: the live tools (startAll, stopAll, the status watcher) find "worldserver" by name
    Copy-Item (Join-Path $simBin 'worldserver.exe') (Join-Path $simBin 'simworld.exe') -Force
}

# The live databases copied once (mysqldump, then loaded into each worker's own): the bench bots' characters, their
# accounts, the owner. Again after a schema change or to take the live characters' state.
function Update-SimDatabases([int]$workers) {
    New-Item -ItemType Directory -Force $simDumps | Out-Null
    # The realm's user may make and use the workers' own (local Laragon root, no password)
    $grant = 'GRANT ALL PRIVILEGES ON `acore\_sim%`.* TO `acore`@`localhost`;'
    Remove-Item Env:MYSQL_PWD -ErrorAction SilentlyContinue
    & (Join-Path $mysqlBin 'mysql.exe') "--host=$dbHost" "--port=$dbPort" '--user=root' '--protocol=TCP' -e $grant
    if ($LASTEXITCODE -ne 0) { throw "Could not grant $dbUser the acore_sim* databases (as root)" }
    $env:MYSQL_PWD = $dbPassword
    foreach ($kind in $simCopied) {
        $dump = Join-Path $simDumps "$kind.sql"
        Write-Host "Dumping $($liveDatabase[$kind])..."
        $arguments = @("--host=$dbHost", "--port=$dbPort", "--user=$dbUser", '--protocol=TCP', '--single-transaction',
            '--quick', '--skip-lock-tables', '--no-tablespaces', '--set-gtid-purged=OFF', "--result-file=$dump",
            $liveDatabase[$kind])
        & (Join-Path $mysqlBin 'mysqldump.exe') @arguments
        if ($LASTEXITCODE -ne 0) { throw "mysqldump of $($liveDatabase[$kind]) failed" }
    }
    $jobs = foreach ($worker in 1..$workers) {
        Start-Job -ArgumentList $PSScriptRoot, $worker -ScriptBlock {
            param($scriptRoot, $worker)
            . (Join-Path $scriptRoot 'SimCommon.ps1')
            foreach ($kind in $simCopied) {
                $name = Get-SimDatabaseName $worker $kind
                $create = "DROP DATABASE IF EXISTS $name; " +
                    "CREATE DATABASE $name DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
                Invoke-SimMysql @('-e', $create) | Out-Null
                Invoke-SimMysql @($name) (Join-Path $simDumps "$kind.sql") | Out-Null
            }
            "worker $worker databases loaded"
        }
    }
    $jobs | Wait-Job | Receive-Job
    $jobs | Remove-Job
}

# A config line set (replaced where the file has it, added at the end otherwise)
function Set-SimConfigLines([string]$path, [hashtable]$values) {
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.AddRange([string[]](Get-Content $path -Encoding utf8))
    foreach ($key in $values.Keys) {
        $pattern = '^\s*' + [regex]::Escape($key) + '\s*='
        $line = "$key = $($values[$key])"
        $found = $false
        for ($index = 0; $index -lt $lines.Count; ++$index) {
            if ($lines[$index] -match $pattern) { $lines[$index] = $line; $found = $true }
        }
        if (-not $found) { $lines.Add($line) }
    }
    [System.IO.File]::WriteAllLines($path, $lines, [System.Text.UTF8Encoding]::new($false))
}

function Get-SimDatabaseInfo([string]$name) { "`"$dbHost;$dbPort;$dbUser;$dbPassword;$name`"" }

# A worker's folder: its configuration (the live one, its databases, port and simulation settings changed), its logs
function Initialize-SimWorker([int]$worker, [int]$stepMs) {
    $directory = Get-SimWorkerDirectory $worker
    $configs = Join-Path $directory 'configs'
    New-Item -ItemType Directory -Force (Join-Path $configs 'modules') | Out-Null
    Copy-Item (Join-Path $serverRoot 'configs\worldserver.conf') $configs -Force
    Get-ChildItem (Join-Path $serverRoot 'configs\modules') -Filter '*.conf' |
        Copy-Item -Destination (Join-Path $configs 'modules') -Force

    Set-SimConfigLines (Join-Path $configs 'worldserver.conf') @{
        'LoginDatabaseInfo' = Get-SimDatabaseInfo (Get-SimDatabaseName $worker 'auth')
        'CharacterDatabaseInfo' = Get-SimDatabaseInfo (Get-SimDatabaseName $worker 'characters')
        'WorldServerPort' = 8200 + $worker
        'DataDir' = "`"$((Join-Path $serverRoot 'Data').Replace('\', '/'))`""
        'LogsDir' = '""'
        'Updates.EnableDatabases' = 0
        'Updates.AutoSetup' = 0
        'Ra.Enable' = 0
        'SOAP.Enabled' = 0
        'Console.Enable' = 1
        'BeepAtStart' = 0
        'MaxCoreStuckTime' = 0
        'HitchProfiler.Enabled' = 1                       # its totals: each test's #simprofile
        'Appender.Console' = '1,2,0'
        'Sim.Enable' = 1
        'Sim.StepMs' = $stepMs
        'Sim.Owner' = "`"$simOwnerName`""
    }
    Set-SimConfigLines (Join-Path $configs 'modules\playerbots.conf') @{
        'PlayerbotsDatabaseInfo' = Get-SimDatabaseInfo (Get-SimDatabaseName $worker 'playerbots')
        'AiPlayerbot.RandomBotAutologin' = 0
        'AiPlayerbot.MinRandomBots' = 0
        'AiPlayerbot.MaxRandomBots' = 0
        'AiPlayerbot.ContentBotMaxLoading' = 30
        'AiPlayerbot.BotActiveAlone' = 100
        'AiPlayerbot.botActiveAloneSmartScale' = 0
        # The Raid Finder's own bot groups running keys: worlds of their own the worker would update
        'RaidFinder.AutoMythicTeams' = 0
    }
    Set-SimConfigLines (Join-Path $configs 'modules\mod_stat_growth.conf') @{ 'PersonalLoot.RequireAddon' = 0 }
    Set-SimConfigLines (Join-Path $configs 'modules\mod_ahbot.conf') @{
        'AuctionHouseBot.EnableSeller' = 0
        'AuctionHouseBot.EnableBuyer' = 0
    }
    $directory
}

# --- Talking to a worker host (its folder: req-<n>.txt in, res-<n>.txt out, ready once its owner is at the bench) ----

function Test-SimWorker([int]$worker) {
    $directory = Get-SimWorkerDirectory $worker
    $pidFile = Join-Path $directory 'host.pid'
    if (-not (Test-Path (Join-Path $directory 'ready')) -or -not (Test-Path $pidFile)) { return $false }
    [bool](Get-Process -Id ([int](Get-Content $pidFile)) -ErrorAction SilentlyContinue)
}

# One request (lines, the bench session's language: .<command>, bots, run, settle, wait, quit) and its answer
function Send-SimRequest([int]$worker, [string[]]$lines, [int]$timeoutSeconds) {
    if (-not (Test-SimWorker $worker)) { throw "Simulation worker $worker is not up (simBench.ps1 start)" }
    $directory = Get-SimWorkerDirectory $worker
    $name = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'), ([guid]::NewGuid().ToString('N').Substring(0, 6))
    $temporary = Join-Path $directory "tmp-$name.txt"
    [System.IO.File]::WriteAllText($temporary, ($lines -join "`n"), [System.Text.UTF8Encoding]::new($false))
    Move-Item $temporary (Join-Path $directory "req-$name.txt")

    $answer = Join-Path $directory "res-$name.txt"
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while (-not (Test-Path $answer)) {
        if ((Get-Date) -gt $deadline) {
            throw "Worker $worker did not answer in $timeoutSeconds s ($directory\host.log)"
        }
        if (-not (Test-SimWorker $worker)) { throw "Worker $worker ended ($directory\host.log)" }
        Start-Sleep -Milliseconds 100
    }
    $content = Get-Content $answer -Encoding utf8
    Remove-Item $answer
    $content
}
