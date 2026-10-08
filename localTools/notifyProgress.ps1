param(
    # What happened: one short message, Markdown allowed (Discord)
    [Parameter(Mandatory = $true)][string]$message,
    # A title line in bold above it
    [string]$title = ''
)
# Posts a progress report to the developer's own Discord channel (the `progressWebhook` of the gitignored
# localTools/deploy.local.json; nothing is sent without it): a task finished, something ready to test in game. Not the
# players' channel (deployDiscord.ps1) nor the server status (serverStatus.ps1).
$ErrorActionPreference = 'Stop'
$settings = Join-Path $PSScriptRoot 'deploy.local.json'
if (-not (Test-Path $settings)) { Write-Host 'No deploy.local.json: nothing sent.'; return }
$webhook = (Get-Content $settings -Raw -Encoding UTF8 | ConvertFrom-Json).progressWebhook
if (-not $webhook) { Write-Host 'No progressWebhook in deploy.local.json: nothing sent.'; return }
$content = if ($title) { "**$title**`n$message" } else { $message }
# Discord takes at most 2000 characters a message
if ($content.Length -gt 1990) { $content = $content.Substring(0, 1990) + '...' }
$body = [System.Text.Encoding]::UTF8.GetBytes((@{ content = $content } | ConvertTo-Json -Compress))
Invoke-RestMethod -Uri $webhook -Method Post -ContentType 'application/json; charset=utf-8' -Body $body | Out-Null
Write-Host 'Sent.'
