# The deploy window's notices on the players' Discord channel (dot-sourced by deployWithProgress.ps1): one message per
# run, edited as it goes, so the players see an update being built, the server restarting and a new client released.
#
# The webhook is a secret: it lives in localTools/deploy.local.json (gitignored), { "discordWebhook": "<url>" }. Without
# that file nothing is sent. Discord being unreachable never stops a deploy: every call is tried once, briefly.
#
# The message lists the run's steps as the players would name them, and what changed since the last deploy that reached
# them (a restart or a release): the subjects of the commits since then, remembered in
# %LOCALAPPDATA%\Evolutions\discord-state.json.

$deployDiscord = @{ Url = $null; MessageId = $null; StatePath = Join-Path $env:LOCALAPPDATA 'Evolutions\discord-state.json' }
$deployDiscordConfig = Join-Path $PSScriptRoot 'deploy.local.json'
if (Test-Path -LiteralPath $deployDiscordConfig) {
    try { $deployDiscord.Url = (Get-Content -LiteralPath $deployDiscordConfig -Raw | ConvertFrom-Json).discordWebhook }
    catch { }
}

# How each step reads for the players
$deployDiscordLabels = @{
    server = 'Compilation du serveur'; dll = 'Extension du client'; client = 'Construction du patch client'
    restart = 'Redémarrage du serveur'; publish = 'Publication de la nouvelle version'
}

function Get-DeployDiscordChanges {
    $last = $null
    if (Test-Path -LiteralPath $deployDiscord.StatePath) {
        try { $last = (Get-Content -LiteralPath $deployDiscord.StatePath -Raw | ConvertFrom-Json).lastCommit } catch { }
    }
    $range = if ($last) { "$last..HEAD" } else { 'HEAD~5..HEAD' }
    $subjects = @(& git -C $repoRoot log --no-merges --format=%s $range 2>$null | Select-Object -First 8)
    foreach ($subject in $subjects) {
        # "feat(barbarian): the Barbare..." reads as "barbarian : the Barbare..."
        $text = $subject -replace '^\w+\(([^)]+)\)!?:\s*', '$1 : ' -replace '^\w+!?:\s*', ''
        if ($text.Length -gt 140) { $text = $text.Substring(0, 137) + '...' }
        "• $text"
    }
}

function Send-DeployDiscord([object[]]$plan, [string]$heading, [switch]$final, $failedStep) {
    if (-not $deployDiscord.Url) { return }
    try {
        $lines = foreach ($step in $plan) {
            $icon = switch ($step.State) {
                'done' { ':white_check_mark:' } 'running' { ':hourglass_flowing_sand:' } 'failed' { ':x:' }
                'blocked' { ':pause_button:' } 'skipped' { ':heavy_minus_sign:' } default { ':white_small_square:' }
            }
            $time = if ($step.Elapsed) { ' — {0:mm\:ss}' -f $step.Elapsed } else { '' }
            $label = $deployDiscordLabels[$step.Name]
            if ($step.Name -eq 'restart' -and $step.State -eq 'running') { $label += ' (le jeu revient dans une minute)' }
            if ($step.Name -eq 'publish' -and $step.State -eq 'done' -and $step.Version) {
                $label += " : **$($step.Version)**, disponible dans le lanceur"
            }
            "$icon $label$time"
        }
        $color = 15105570                      # in progress: amber
        $title = ":hammer_and_wrench: $heading en cours"
        if ($final -and $failedStep) { $color = 15158332; $title = ":x: $heading interrompue" }
        elseif ($final) { $color = 3066993; $title = ":white_check_mark: $heading terminée" }
        $embed = @{ title = $title; description = ($lines -join "`n"); color = $color
            timestamp = (Get-Date).ToUniversalTime().ToString('o') }
        $changes = @(Get-DeployDiscordChanges)
        if ($changes.Count) {
            $value = $changes -join "`n"
            if ($value.Length -gt 1024) { $value = $value.Substring(0, 1021) + '...' }
            $embed.fields = @(@{ name = 'Au programme'; value = $value })
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
        # A deploy that reached the players: what it listed is now theirs
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
