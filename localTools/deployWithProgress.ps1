param(
    # What to run, in this order: any of server (compile), restart (install and restart), client (build the client
    # patch), publish (release it). Steps not listed are skipped.
    [ValidateSet('server', 'restart', 'client', 'publish')]
    [string[]]$steps = @('server', 'restart', 'client', 'publish'),

    # What is being shipped, shown at the top of the window
    [string]$title = 'Mise à jour'
)

# Runs a deploy in a small window: every step with its state, a progress bar that follows the steps' own output,
# the line each step is on and the time it has taken. Each step runs in its own hidden PowerShell with its output in
# a log (%TEMP%\evolutions-deploy\), which the window reads as it grows. The client step waits, saying so, while WoW
# is open: the patch is copied into the client. The window closes itself a few seconds after the end.
#
# Exit code 0 when every step succeeded; otherwise 1, and the summary names the failed step and its log.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$repoRoot = Split-Path -Parent $PSScriptRoot
$logRoot = Join-Path $env:TEMP 'evolutions-deploy'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

# Each step: the script it runs, its weight on the overall bar, and the lines in its output that mark its way
# through, in order. A step without markers shows a moving bar.
$catalog = [ordered]@{
    server  = @{
        Label = 'Compilation du serveur'; Weight = 40
        Script = Join-Path $PSScriptRoot 'buildServer.ps1'; Arguments = @()
        Markers = @('common.vcxproj', 'database.vcxproj', 'shared.vcxproj', 'authserver.vcxproj', 'game.vcxproj',
            'scripts.vcxproj', 'modules.vcxproj', 'worldserver.vcxproj')
    }
    restart = @{
        Label = 'Redémarrage du serveur'; Weight = 15
        Script = Join-Path $PSScriptRoot 'installAndRestart.ps1'; Arguments = @()
        Markers = @('Stopping the servers', 'Installing the new build', 'Starting the servers',
            'Waiting for the world', 'World server online')
    }
    client  = @{
        Label = 'Construction du patch client'; Weight = 30
        Script = Join-Path $repoRoot 'clientPatcher\Build-FriendPatch.ps1'; Arguments = @()
        Markers = @('Compiling custom icons', 'Patching spell data', 'Building patch-Z.MPQ',
            'Generating custom classes', 'Generating talent trees', 'Generating the Wow.exe patches',
            'Compiling Evolutions Glue', 'Compiling Paragon node icons', 'Compiling talent tree art',
            'Building interface patches', 'Patch created')
        NeedsWowClosed = $true
    }
    publish = @{
        Label = 'Publication de la version'; Weight = 15
        Script = Join-Path $repoRoot 'clientPatcher\Publish-Release.ps1'; Arguments = @('-skipBuild')
        Markers = @('Package:', 'Building the launcher', 'Publishing v', 'Published:')
    }
}

$plan = @(foreach ($name in $catalog.Keys) {
        if ($steps -contains $name) {
            $entry = $catalog[$name]
            [pscustomobject]@{
                Name = $name; Label = $entry.Label; Weight = $entry.Weight; Script = $entry.Script
                Arguments = $entry.Arguments; Markers = $entry.Markers; NeedsWowClosed = [bool]$entry.NeedsWowClosed
                State = 'waiting'; Reached = 0; Log = Join-Path $logRoot "$name.log"; Started = $null; Elapsed = $null
                Summary = ''
            }
        }
    })
$totalWeight = ($plan | Measure-Object -Property Weight -Sum).Sum

# --- The window: the challenge board's palette, gold and parchment on dark ------------------------------------------
$colorBack = [System.Drawing.Color]::FromArgb(24, 19, 14)
$colorPanel = [System.Drawing.Color]::FromArgb(38, 30, 22)
$colorGold = [System.Drawing.Color]::FromArgb(230, 196, 122)
$colorText = [System.Drawing.Color]::FromArgb(217, 201, 163)
$colorMuted = [System.Drawing.Color]::FromArgb(140, 124, 98)
$colorFail = [System.Drawing.Color]::FromArgb(201, 120, 90)
$fontTitle = New-Object System.Drawing.Font('Georgia', 13, [System.Drawing.FontStyle]::Bold)
$fontText = New-Object System.Drawing.Font('Segoe UI', 9.5)
$fontSmall = New-Object System.Drawing.Font('Consolas', 8.5)

$form = New-Object System.Windows.Forms.Form
$form.Text = "Evolutions - $title"
$form.ClientSize = New-Object System.Drawing.Size(520, (170 + 26 * $plan.Count))
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.TopMost = $true
$form.BackColor = $colorBack

function New-Label($text, $font, $color, $x, $y, $width, $height) {
    $label = New-Object System.Windows.Forms.Label
    $label.Text = $text
    $label.Font = $font
    $label.ForeColor = $color
    $label.BackColor = [System.Drawing.Color]::Transparent
    $label.Location = New-Object System.Drawing.Point($x, $y)
    $label.Size = New-Object System.Drawing.Size($width, $height)
    $label.AutoEllipsis = $true
    $form.Controls.Add($label)
    return $label
}

$titleLabel = New-Label $title $fontTitle $colorGold 18 14 484 26
$stepLabels = @{}
$timeLabels = @{}
$y = 50
foreach ($step in $plan) {
    $stepLabels[$step.Name] = New-Label "   $($step.Label)" $fontText $colorMuted 18 $y 400 22
    $timeLabels[$step.Name] = New-Label '' $fontText $colorMuted 420 $y 82 22
    $timeLabels[$step.Name].TextAlign = 'TopRight'
    $y += 26
}

# Drawn rather than a ProgressBar: the stock one is green and cannot be recoloured with visual styles on
$barBack = New-Object System.Windows.Forms.Panel
$barBack.Location = New-Object System.Drawing.Point(18, ($y + 10))
$barBack.Size = New-Object System.Drawing.Size(484, 16)
$barBack.BackColor = $colorPanel
$form.Controls.Add($barBack)
$barFill = New-Object System.Windows.Forms.Panel
$barFill.Location = New-Object System.Drawing.Point(0, 0)
$barFill.Size = New-Object System.Drawing.Size(0, 16)
$barFill.BackColor = $colorGold
$barBack.Controls.Add($barFill)

$percentLabel = New-Label '0 %' $fontText $colorText 18 ($y + 32) 200 20
$totalLabel = New-Label '' $fontText $colorText 302 ($y + 32) 200 20
$totalLabel.TextAlign = 'TopRight'
$lineLabel = New-Label '' $fontSmall $colorMuted 18 ($y + 58) 484 36

# --- Running the steps: a state machine on the window's timer, so the window never freezes -----------------------
$script:current = -1
$script:process = $null
$script:readPosition = 0
$script:lastLine = ''
$script:startedAll = Get-Date
$script:finishedAt = $null
$script:failed = $null
$script:pulse = 0

function Format-Elapsed($span) {
    if (-not $span) { return '' }
    return '{0}:{1:00}' -f [int][Math]::Floor($span.TotalMinutes), $span.Seconds
}

# Reads what the running step has written since the last tick; the log is still open for writing
function Read-NewOutput($step) {
    if (-not (Test-Path -LiteralPath $step.Log)) { return }
    $stream = [System.IO.File]::Open($step.Log, 'Open', 'Read', 'ReadWrite')
    try {
        if ($stream.Length -le $script:readPosition) { return }
        $stream.Seek($script:readPosition, 'Begin') | Out-Null
        $buffer = New-Object byte[] ($stream.Length - $script:readPosition)
        $read = $stream.Read($buffer, 0, $buffer.Length)
        $script:readPosition += $read
        $text = [System.Text.Encoding]::Default.GetString($buffer, 0, $read)
    }
    finally {
        $stream.Dispose()
    }

    foreach ($line in ($text -split "`r?`n")) {
        $trimmed = $line.Trim()
        if (-not $trimmed) { continue }
        $script:lastLine = $trimmed
        for ($index = $step.Reached; $index -lt $step.Markers.Count; ++$index) {
            if ($trimmed.Contains($step.Markers[$index])) {
                $step.Reached = $index + 1
                break
            }
        }
    }
}

function Start-Step($step) {
    Remove-Item -LiteralPath $step.Log, "$($step.Log).err" -ErrorAction SilentlyContinue
    $script:readPosition = 0
    $script:lastLine = ''
    $step.Started = Get-Date
    $step.State = 'running'
    # The build scripts fail by throwing, and one that succeeds can still end on a tool's stray exit code, so the
    # step's result is whether it threw
    $call = "& '$($step.Script)' $($step.Arguments -join ' ')"
    $command = "try { $call; exit 0 } catch { [Console]::Error.WriteLine(`$_); exit 1 }"
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', "`"$command`"")
    $script:process = Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments -WorkingDirectory $repoRoot `
        -RedirectStandardOutput $step.Log -RedirectStandardError "$($step.Log).err" -WindowStyle Hidden -PassThru
}

function Update-View {
    $done = 0.0
    foreach ($step in $plan) {
        $label = $stepLabels[$step.Name]
        switch ($step.State) {
            'waiting' { $label.Text = "   $($step.Label)"; $label.ForeColor = $colorMuted }
            'blocked' { $label.Text = "…  $($step.Label) : fermez WoW pour continuer"; $label.ForeColor = $colorGold }
            'running' { $label.Text = "►  $($step.Label)"; $label.ForeColor = $colorGold }
            'done' { $label.Text = "✓  $($step.Label)"; $label.ForeColor = $colorText; $done += $step.Weight }
            'failed' { $label.Text = "✗  $($step.Label) : échec"; $label.ForeColor = $colorFail }
            'skipped' { $label.Text = "–  $($step.Label) (annulé)"; $label.ForeColor = $colorMuted }
        }
        if ($step.State -eq 'running') {
            $timeLabels[$step.Name].Text = Format-Elapsed ((Get-Date) - $step.Started)
            if ($step.Markers.Count) {
                $done += $step.Weight * [Math]::Min(0.97, $step.Reached / $step.Markers.Count)
            }
        }
        elseif ($step.Elapsed) {
            $timeLabels[$step.Name].Text = Format-Elapsed $step.Elapsed
        }
    }

    $fraction = if ($totalWeight) { $done / $totalWeight } else { 1 }
    $barFill.Width = [int]($barBack.Width * $fraction)
    $percentLabel.Text = '{0} %' -f [int]($fraction * 100)
    $totalLabel.Text = "Total $(Format-Elapsed ((Get-Date) - $script:startedAll))"
    if (-not $script:finishedAt) {
        $lineLabel.Text = $script:lastLine
    }
}

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 400
$timer.Add_Tick({
        if ($script:finishedAt) {
            if (((Get-Date) - $script:finishedAt).TotalSeconds -ge ($(if ($script:failed) { 12 } else { 5 }))) {
                $timer.Stop()
                $form.Close()
            }
            return
        }

        if ($script:current -ge 0) {
            $step = $plan[$script:current]
            if ($step.State -eq 'running') {
                Read-NewOutput $step
                if (-not $script:process.HasExited) {
                    Update-View
                    return
                }
                $script:process.WaitForExit()
                Read-NewOutput $step
                $step.Elapsed = (Get-Date) - $step.Started
                if ($script:process.ExitCode -eq 0) {
                    $step.State = 'done'
                }
                else {
                    $step.State = 'failed'
                    $script:failed = $step
                    $errorLog = "$($step.Log).err"
                    $errors = @()
                    if (Test-Path -LiteralPath $errorLog) { $errors = Get-Content -LiteralPath $errorLog -Tail 3 }
                    $step.Summary = (@($errors) -join ' ').Trim()
                    foreach ($rest in $plan) { if ($rest.State -eq 'waiting') { $rest.State = 'skipped' } }
                    $script:finishedAt = Get-Date
                    $titleLabel.Text = "$title : échec"
                    $titleLabel.ForeColor = $colorFail
                    $lineLabel.Text = "Journal : $($step.Log)"
                    Update-View
                    return
                }
            }
        }

        # The next step, or the end
        $next = $script:current + 1
        if ($next -ge $plan.Count) {
            $script:finishedAt = Get-Date
            $titleLabel.Text = "$title : terminé"
            $lineLabel.Text = 'Tout est à jour.'
            Update-View
            return
        }

        $step = $plan[$next]
        if ($step.NeedsWowClosed -and (Get-Process -Name 'Wow' -ErrorAction SilentlyContinue)) {
            $step.State = 'blocked'
            Update-View
            return
        }
        $script:current = $next
        Start-Step $step
        Update-View
    })

$form.Add_Shown({ $timer.Start() })
$form.Add_FormClosing({
        param($sender, $closing)
        # Closing the window by hand does not leave a step running unseen
        if (-not $script:finishedAt -and $script:process -and -not $script:process.HasExited) {
            $answer = [System.Windows.Forms.MessageBox]::Show('Une étape est en cours. Arrêter la mise à jour ?',
                'Evolutions', 'YesNo', 'Warning')
            if ($answer -ne 'Yes') {
                $closing.Cancel = $true
                return
            }
            & taskkill.exe /PID $script:process.Id /T /F | Out-Null
            $script:failed = $plan[$script:current]
            $plan[$script:current].State = 'failed'
            $plan[$script:current].Summary = 'arrêtée depuis la fenêtre'
        }
    })

[System.Windows.Forms.Application]::Run($form)

# --- The summary, for whoever started it -----------------------------------------------------------------------
foreach ($step in $plan) {
    $line = '{0,-8} {1,-8} {2}' -f $step.Name, $step.State, (Format-Elapsed $step.Elapsed)
    if ($step.Summary) { $line += "  $($step.Summary)" }
    Write-Output $line
    if ($step.State -eq 'done' -or $step.State -eq 'failed') {
        $tail = if ($step.State -eq 'failed') { 25 } else { 2 }
        Get-Content -LiteralPath $step.Log -Tail $tail -ErrorAction SilentlyContinue |
            ForEach-Object { Write-Output "    $_" }
    }
}
if ($script:failed -or ($plan | Where-Object { $_.State -ne 'done' })) {
    exit 1
}
exit 0
