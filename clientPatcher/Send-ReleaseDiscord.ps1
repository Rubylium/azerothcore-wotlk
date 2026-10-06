# The webhook lives in release.local.json (gitignored), like the deploy window's local configuration.
function Send-ReleaseDiscord([string]$version) {
    $configPath = Join-Path $PSScriptRoot 'release.local.json'
    if (-not (Test-Path -LiteralPath $configPath)) { return }

    try {
        $webhookUrl = (Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json).discordWebhook
        if ([string]::IsNullOrWhiteSpace($webhookUrl)) { return }

        $payload = @{
            content = "Evolutions launcher v$version is available."
            allowed_mentions = @{ parse = @() }
        } | ConvertTo-Json -Depth 3
        $body = [Text.Encoding]::UTF8.GetBytes($payload)
        Invoke-RestMethod -Method Post -Uri $webhookUrl -Body $body `
            -ContentType 'application/json; charset=utf-8' -TimeoutSec 5 | Out-Null
    }
    catch {
        # Do not print the exception: HTTP errors can include the secret webhook URL.
        Write-Warning 'Release published, but the Discord notification could not be sent.'
    }
}
