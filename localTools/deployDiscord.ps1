# The deploy window's status on the players' Discord channel (dot-sourced by deployWithProgress.ps1): one message per
# run, edited as it goes, showing development activity only - the pipeline's stages, their state and time, the build's
# revision and how much code moved since the last deploy. Never what changed: this is not a patch note.
#
# The webhook is a secret: it lives in localTools/deploy.local.json (gitignored), { "discordWebhook": "<url>" }. Without
# that file nothing is sent. Discord being unreachable never stops a deploy: every call is tried once, briefly.
#
# The last deploy that reached the players (a restart or a release) is remembered in
# %LOCALAPPDATA%\Evolutions\discord-state.json, so the change counts start from it.

$deployDiscord = @{ Url = $null; MessageId = $null; Started = Get-Date
    StatePath = Join-Path $env:LOCALAPPDATA 'Evolutions\discord-state.json' }
$deployDiscordConfig = Join-Path $PSScriptRoot 'deploy.local.json'
if (Test-Path -LiteralPath $deployDiscordConfig) {
    try { $deployDiscord.Url = (Get-Content -LiteralPath $deployDiscordConfig -Raw | ConvertFrom-Json).discordWebhook }
    catch { }
}

# Each stage as the pipeline names it: its target, and what it does
$deployDiscordStages = [ordered]@{
    server = @('worldserver', 'compile'); dll = @('client-ext', 'build'); client = @('client-patch', 'package')
    restart = @('realm', 'restart'); publish = @('release', 'publish')
}

function Get-DeployDiscordRevision {
    $last = $null
    if (Test-Path -LiteralPath $deployDiscord.StatePath) {
        try { $last = (Get-Content -LiteralPath $deployDiscord.StatePath -Raw | ConvertFrom-Json).lastCommit } catch { }
    }
    $revision = @{ Head = (& git -C $repoRoot rev-parse --short=7 HEAD 2>$null)
        Branch = (& git -C $repoRoot rev-parse --abbrev-ref HEAD 2>$null); Changes = '' }
    if ($last) {
        $commits = & git -C $repoRoot rev-list --count "$last..HEAD" 2>$null
        $stat = (& git -C $repoRoot diff --shortstat $last HEAD 2>$null) -join ''
        $files = if ($stat -match '(\d+) files? changed') { $Matches[1] } else { '0' }
        $added = if ($stat -match '(\d+) insertions?') { $Matches[1] } else { '0' }
        $removed = if ($stat -match '(\d+) deletions?') { $Matches[1] } else { '0' }
        $commitWord = if ([int]$commits -eq 1) { 'commit' } else { 'commits' }
        $fileWord = if ([int]$files -eq 1) { 'file' } else { 'files' }
        $revision.Changes = "$commits $commitWord · $files $fileWord · +$added / -$removed"
    }
    return $revision
}

function Format-DeployDiscordTime([TimeSpan]$span) {
    return '{0:00}:{1:00}' -f [int][Math]::Floor($span.TotalMinutes), $span.Seconds
}

function Send-DeployDiscord([object[]]$plan, [string]$heading, [switch]$final, $failedStep) {
    if (-not $deployDiscord.Url) { return }
    try {
        $rows = foreach ($step in $plan) {
            $stage = $deployDiscordStages[$step.Name]
            $mark = switch ($step.State) {
                'done' { 'ok' } 'running' { '...' } 'failed' { 'FAIL' } 'blocked' { 'wait' } 'skipped' { '-' }
                default { '' }
            }
            $detail = $stage[1]
            if ($step.Name -eq 'client' -and $step.Version) { $detail = "v$($step.Version)" }
            if ($step.Name -eq 'publish' -and $step.Version) { $detail = $step.Version }
            $time = if ($step.Elapsed) { Format-DeployDiscordTime $step.Elapsed } else { '' }
            '{0,-13}{1,-11}{2,-6}{3}' -f $stage[0], $detail, $mark, $time
        }
        $state = 'running'; $color = 5793266
        if ($final -and $failedStep) { $state = 'failed'; $color = 15548997 }
        elseif ($final) { $state = 'succeeded'; $color = 5763719 }
        $revision = Get-DeployDiscordRevision
        $fields = @(
            @{ name = 'Revision'; value = "``$($revision.Branch)@$($revision.Head)``"; inline = $true },
            @{ name = 'Duration'; value = "``$(Format-DeployDiscordTime ((Get-Date) - $deployDiscord.Started))``"; inline = $true })
        if ($revision.Changes) { $fields += @{ name = 'Since last deploy'; value = "``$($revision.Changes)``" } }
        $embed = @{
            title = "Deploy pipeline · $state"
            description = "``````text`n$($rows -join "`n")`n``````"
            color = $color; fields = $fields
            footer = @{ text = 'Evolutions · build & deploy' }
            timestamp = (Get-Date).ToUniversalTime().ToString('o')
        }
        $body = [Text.Encoding]::UTF8.GetBytes((@{ embeds = @($embed) } | ConvertTo-Json -Depth 6))
        if ($deployDiscord.MessageId) {
            Invoke-RestMethod -Method Patch -Uri "$($deployDiscord.Url)/messages/$($deployDiscord.MessageId)" -Body $body `
                -ContentType 'application/json; charset=utf-8' -TimeoutSec 5 | Out-Null
        }
        else {
            $message = Invoke-RestMethod -Method Post -Uri "$($deployDiscord.Url)?wait=true" -Body $body `
                -ContentType 'application/json; charset=utf-8' -TimeoutSec 5
            $deployDiscord.MessageId = $message.id
        }
        # A deploy that reached the players: the change counts start again from here
        $reached = @($plan | Where-Object { $_.Name -in 'restart', 'publish' -and $_.State -eq 'done' })
        if ($final -and $reached.Count) {
            $head = & git -C $repoRoot rev-parse HEAD 2>$null
            if ($head) {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $deployDiscord.StatePath) | Out-Null
                @{ lastCommit = $head } | ConvertTo-Json | Set-Content -LiteralPath $deployDiscord.StatePath -Encoding UTF8
            }
        }
    }
    catch { }
}
