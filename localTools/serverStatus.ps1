param(
    [Parameter(Mandatory)]
    [ValidateSet('starting', 'online', 'offline', 'watch')]
    [string]$state
)

# The PTR's state on the players' Discord status channel: one message, edited in place - starting, online, offline (a
# deliberate stop) or down (the world went away without one: a crash). Plain facts, no prose: the state, since when, and
# how many players are on. Saved with a byte order mark: Windows PowerShell reads a script without one as ANSI, and the
# title's middle dot came out as "Â·".
#
#   serverStatus.ps1 starting|online|offline   sets the state (startAll.ps1, stopAll.ps1)
#   serverStatus.ps1 watch                       waits for the world to open, then checks it every 30 sec until it
#                                                closes (startAll.ps1 runs it hidden, one at a time)
#
# The webhook is a secret: localTools/deploy.local.json (gitignored), "statusWebhook". Without it nothing is sent.
# The message and the state are remembered in %LOCALAPPDATA%\Evolutions\status-state.json. Discord being unreachable
# never stops a start or a stop.
$ErrorActionPreference = 'Stop'

$statusConfig = Join-Path $PSScriptRoot 'deploy.local.json'
$statusPath = Join-Path $env:LOCALAPPDATA 'Evolutions\status-state.json'
$watcherPath = Join-Path $env:LOCALAPPDATA 'Evolutions\status-watcher.pid'
$worldPort = 8085
$mysql = 'C:\laragon\bin\mysql\mysql-8.0.30-winx64\bin\mysql.exe'

$url = $null
if (Test-Path -LiteralPath $statusConfig) {
    try { $url = (Get-Content -LiteralPath $statusConfig -Raw | ConvertFrom-Json).statusWebhook } catch { }
}

function Read-StatusState {
    if (Test-Path -LiteralPath $statusPath) {
        try { return Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json } catch { }
    }
    return [pscustomobject]@{ messageId = $null; state = $null; since = $null; players = $null }
}

function Save-StatusState($saved) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $statusPath) | Out-Null
    $saved | ConvertTo-Json | Set-Content -LiteralPath $statusPath -Encoding UTF8
}

function Test-WorldOpen {
    return [bool](Get-NetTCPConnection -LocalPort $worldPort -State Listen -ErrorAction SilentlyContinue)
}

# Real players in the world (not the bots)
function Get-PlayersOnline {
    if (-not (Test-Path -LiteralPath $mysql)) { return $null }
    # mysql warns about the password on stderr: under "Stop" that warning became an error and the count was lost
    $ErrorActionPreference = 'Continue'
    try {
        $query = "SELECT COUNT(*) FROM acore_characters.characters c JOIN acore_auth.account a ON a.id = c.account " +
            "WHERE c.online = 1 AND a.username NOT LIKE 'RNDBOT%'"
        $count = & $mysql -h 127.0.0.1 -P 3307 -uacore -pacore -N -e $query 2>$null
        if ($count -match '^\d+$') { return [int]$count }
    }
    catch { }
    return $null
}

function Send-Status([string]$newState, $players) {
    $saved = Read-StatusState
    if ($saved.state -ne $newState) {
        $saved.since = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    }
    $saved.state = $newState
    $saved.players = $players
    Save-StatusState $saved
    if (-not $url) { return }

    $look = @{
        starting = @('Starting', 16766720); online = @('Online', 5763719)
        offline = @('Offline', 9807270); down = @('Down', 15548997)
    }[$newState]
    $fields = @()
    if ($newState -eq 'online' -and $null -ne $players) {
        $fields += @{ name = 'Players'; value = "$players"; inline = $true }
    }
    $fields += @{ name = 'Since'; value = "<t:$($saved.since):R>"; inline = $true }
    $embed = @{ title = "PTR · $($look[0])"; color = $look[1]; fields = $fields }
    $body = [Text.Encoding]::UTF8.GetBytes((@{ embeds = @($embed) } | ConvertTo-Json -Depth 6))
    try {
        if ($saved.messageId) {
            try {
                Invoke-RestMethod -Method Patch -Uri "$url/messages/$($saved.messageId)" -Body $body `
                    -ContentType 'application/json; charset=utf-8' -TimeoutSec 5 | Out-Null
                return
            }
            catch {
                # The message was deleted: a new one below
            }
        }
        $message = Invoke-RestMethod -Method Post -Uri "$url`?wait=true" -Body $body `
            -ContentType 'application/json; charset=utf-8' -TimeoutSec 5
        $saved.messageId = $message.id
        Save-StatusState $saved
    }
    catch { }
}

# A start or a stop ends the watcher of the last run: one still asleep would keep the next one from starting
function Stop-Watcher {
    if (-not (Test-Path -LiteralPath $watcherPath)) { return }
    $other = Get-Content -LiteralPath $watcherPath -ErrorAction SilentlyContinue
    if ($other -and [int]$other -ne $PID) {
        Stop-Process -Id ([int]$other) -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $watcherPath -ErrorAction SilentlyContinue
}

if ($state -ne 'watch') {
    if ($state -in 'starting', 'offline') { Stop-Watcher }
    Send-Status $state $(if ($state -eq 'online') { Get-PlayersOnline } else { $null })
    exit 0
}

# --- The watcher: one at a time ---
if (Test-Path -LiteralPath $watcherPath) {
    $other = Get-Content -LiteralPath $watcherPath -ErrorAction SilentlyContinue
    if ($other -and (Get-Process -Id ([int]$other) -ErrorAction SilentlyContinue)) { exit 0 }
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $watcherPath) | Out-Null
Set-Content -LiteralPath $watcherPath -Value $PID

try {
    # The world opening: up to 15 min (a fresh build loads for a minute or two)
    $deadline = (Get-Date).AddMinutes(15)
    while (-not (Test-WorldOpen)) {
        if ((Read-StatusState).state -eq 'offline') { exit 0 }
        if ((Get-Date) -gt $deadline) {
            Send-Status 'down' $null
            exit 0
        }
        Start-Sleep -Seconds 5
    }
    $players = Get-PlayersOnline
    Send-Status 'online' $players

    # Open: the players' count kept up to date, until it closes
    while (Test-WorldOpen) {
        Start-Sleep -Seconds 30
        $now = Get-PlayersOnline
        if ($null -ne $now -and $now -ne $players -and (Test-WorldOpen)) {
            $players = $now
            Send-Status 'online' $players
        }
    }
    # Closed without stopAll.ps1 saying so: down
    if ((Read-StatusState).state -ne 'offline') { Send-Status 'down' $null }
}
finally {
    Remove-Item -LiteralPath $watcherPath -ErrorAction SilentlyContinue
}
