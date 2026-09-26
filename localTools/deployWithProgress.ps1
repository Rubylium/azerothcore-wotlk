param(
    # What to run. Always in this order, whatever order they are given in: server (compile), dll (the client extension,
    # awesome_wotlk), client (the client patch), restart (install and restart the server), publish (release the client).
    [ValidateSet('server', 'dll', 'client', 'restart', 'publish')]
    [string[]]$steps = @('server', 'dll', 'client', 'restart', 'publish'),

    # What is being shipped, the window's heading
    [string]$title = 'Mise à jour',

    # Re-run CMake before compiling the server (a new module, a new source file)
    [switch]$reconfigure,

    # Renders the window with made-up progress to this PNG and exits, without showing anything
    [string]$preview
)

# Runs a deploy in a window of its own: each step with its state and time, the part of it being worked on, one bar for
# the whole run with the time left, and the last lines of output. Each step runs in a hidden PowerShell with its output
# in %TEMP%\evolutions-deploy\<step>.log, which the window reads as it grows. How long each step took is remembered
# (%LOCALAPPDATA%\Evolutions\deploy-history.json) so the bar and the time left follow real durations. The client step
# waits, saying so, while WoW is open. The window never takes the focus and closes itself after the end.
#
# Exit code 0 when every step succeeded, otherwise 1; a summary with the log tails is written either way.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase, System.Xaml

$repoRoot = Split-Path -Parent $PSScriptRoot
$logRoot = Join-Path $env:TEMP 'evolutions-deploy'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$historyPath = Join-Path $env:LOCALAPPDATA 'Evolutions\deploy-history.json'

# --- The steps -------------------------------------------------------------------------------------------------
# Markers are lines of the step's own output, in order; each one starts the part named with it. Expected is the
# duration in seconds used until a real one has been recorded.
function Marker($match, $label) { @{ Match = $match; Label = $label } }

$serverArguments = @()
if ($reconfigure) { $serverArguments = @('-configure') }

$catalog = [ordered]@{
    server  = @{
        Label = 'Serveur'; Caption = 'Compilation'; Expected = 240; Initial = 'Dépendances'
        Script = Join-Path $PSScriptRoot 'buildServer.ps1'; Arguments = $serverArguments
        Markers = @(
            (Marker 'sfmt.vcxproj' 'Bibliothèques communes'), (Marker 'common.vcxproj' 'Base de données'),
            (Marker 'database.vcxproj' 'Code partagé'), (Marker 'authserver.vcxproj' 'Moteur de jeu'),
            (Marker 'game.vcxproj' 'Scripts'), (Marker 'scripts.vcxproj' 'Modules'),
            (Marker 'modules.vcxproj' 'Liaison du worldserver'), (Marker 'worldserver.vcxproj' 'Terminé'))
    }
    dll     = @{
        Label = 'Extension client'; Caption = 'awesome_wotlk'; Expected = 60; Initial = 'Préparation'
        Script = Join-Path $PSScriptRoot 'buildClientDll.ps1'; Arguments = @()
        Markers = @((Marker 'Building AwesomeWotlkLib' 'Compilation'), (Marker 'Built ' 'Terminé'))
    }
    client  = @{
        Label = 'Patch client'; Caption = 'Données, modèles et interface'; Expected = 170; Initial = 'Préparation'
        Script = Join-Path $repoRoot 'clientPatcher\Build-FriendPatch.ps1'; Arguments = @(); NeedsWowClosed = $true
        Markers = @(
            (Marker 'Compiling custom icons' 'Icônes des sorts'), (Marker 'Patching spell data' 'Données des sorts'),
            (Marker 'Building patch-Z.MPQ' 'Archive patch-Z'), (Marker 'Generating custom classes' 'Classes'),
            (Marker 'Generating talent trees' 'Arbres de talents'),
            (Marker 'Generating the Wow.exe' 'Patchs de Wow.exe'),
            (Marker 'Painting custom class icons' 'Icônes de classe'), (Marker 'Compiling Evolutions Glue' 'Logo'),
            (Marker 'Compiling Paragon node icons' 'Icônes du parangon'),
            (Marker 'Compiling Paragon interface art' 'Plateau du parangon'),
            (Marker 'Compiling talent tree art' 'Art des talents'), (Marker 'Building interface patches' 'Interface'),
            (Marker 'Patch created' 'Archive finale'))
    }
    restart = @{
        Label = 'Redémarrage du serveur'; Caption = 'Installation et relance'; Expected = 60; Initial = 'Préparation'
        Script = Join-Path $PSScriptRoot 'installAndRestart.ps1'; Arguments = @()
        Markers = @(
            (Marker 'Stopping the servers' 'Arrêt des serveurs'), (Marker 'Installing the new build' 'Installation'),
            (Marker 'Starting the servers' 'Démarrage'), (Marker 'Waiting for the world' 'Ouverture du monde'),
            (Marker 'World server online' 'En ligne'))
    }
    publish = @{
        Label = 'Publication'; Caption = 'Nouvelle version du lanceur'; Expected = 90; Initial = 'Préparation'
        Script = Join-Path $repoRoot 'clientPatcher\Publish-Release.ps1'; Arguments = @('-skipBuild')
        Markers = @(
            (Marker 'Package:' 'Paquet'), (Marker 'Building the launcher' 'Lanceur'),
            (Marker 'Publishing v' 'Envoi sur GitHub'), (Marker 'Published:' 'Publié'))
    }
}

$history = @{}
if (Test-Path -LiteralPath $historyPath) {
    try {
        (Get-Content -LiteralPath $historyPath -Raw | ConvertFrom-Json).PSObject.Properties |
            ForEach-Object { $history[$_.Name] = [double]$_.Value }
    }
    catch { }
}

$plan = @(foreach ($name in $catalog.Keys) {
        if ($steps -notcontains $name) { continue }
        $entry = $catalog[$name]
        $expected = if ($history.ContainsKey($name)) { $history[$name] } else { $entry.Expected }
        [pscustomobject]@{
            Name = $name; Label = $entry.Label; Caption = $entry.Caption; Script = $entry.Script
            Arguments = $entry.Arguments; Markers = $entry.Markers; Initial = $entry.Initial
            NeedsWowClosed = [bool]$entry.NeedsWowClosed; Expected = [double]$expected
            State = 'waiting'; Reached = 0; Fraction = 0.0; Detail = ''; File = ''
            Log = Join-Path $logRoot "$name.log"; Started = $null; Elapsed = $null; Summary = ''; Ui = $null
        }
    })

# --- The window: the challenge board's palette, gold and parchment on dark ----------------------------------------
$windowXaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="Evolutions" Width="620" SizeToContent="Height" WindowStyle="None" AllowsTransparency="True"
        Background="Transparent" ResizeMode="NoResize" WindowStartupLocation="CenterScreen" ShowActivated="False"
        UseLayoutRounding="True" TextOptions.TextFormattingMode="Display"/>
'@

$rootXaml = @'
<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Margin="22" CornerRadius="14" BorderThickness="1" Width="576" UseLayoutRounding="True">
  <Border.Background>
    <LinearGradientBrush StartPoint="0,0" EndPoint="0,1">
      <GradientStop Color="#FF2A2016" Offset="0"/>
      <GradientStop Color="#FF18120D" Offset="0.45"/>
      <GradientStop Color="#FF100C08" Offset="1"/>
    </LinearGradientBrush>
  </Border.Background>
  <Border.BorderBrush>
    <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
      <GradientStop Color="#FF8C6B30" Offset="0"/>
      <GradientStop Color="#FFE9CB84" Offset="0.35"/>
      <GradientStop Color="#FF5C4520" Offset="0.7"/>
      <GradientStop Color="#FFB8924A" Offset="1"/>
    </LinearGradientBrush>
  </Border.BorderBrush>
  <Border.Effect>
    <DropShadowEffect BlurRadius="26" ShadowDepth="4" Direction="270" Opacity="0.65" Color="#FF000000"/>
  </Border.Effect>
  <Grid>
    <Grid.RowDefinitions>
      <RowDefinition Height="Auto"/>
      <RowDefinition Height="Auto"/>
      <RowDefinition Height="Auto"/>
      <RowDefinition Height="Auto"/>
    </Grid.RowDefinitions>

    <!-- A warm glow behind the heading, as if lit by the forge -->
    <Border Grid.RowSpan="2" CornerRadius="14,14,0,0" IsHitTestVisible="False">
      <Border.Background>
        <RadialGradientBrush Center="0.18,0" GradientOrigin="0.18,0" RadiusX="0.75" RadiusY="1.1">
          <GradientStop Color="#40D9A441" Offset="0"/>
          <GradientStop Color="#00D9A441" Offset="1"/>
        </RadialGradientBrush>
      </Border.Background>
    </Border>

    <!-- Heading -->
    <Grid Grid.Row="0" Margin="26,20,18,0">
      <Grid.ColumnDefinitions>
        <ColumnDefinition Width="Auto"/>
        <ColumnDefinition Width="*"/>
        <ColumnDefinition Width="Auto"/>
      </Grid.ColumnDefinitions>
      <Grid Width="44" Height="44" VerticalAlignment="Center">
        <Ellipse StrokeThickness="1.5">
          <Ellipse.Stroke>
            <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
              <GradientStop Color="#FFF1D896" Offset="0"/>
              <GradientStop Color="#FF7A5A24" Offset="1"/>
            </LinearGradientBrush>
          </Ellipse.Stroke>
          <Ellipse.Fill>
            <RadialGradientBrush>
              <GradientStop Color="#FF3A2B19" Offset="0"/>
              <GradientStop Color="#FF1A130C" Offset="1"/>
            </RadialGradientBrush>
          </Ellipse.Fill>
        </Ellipse>
        <!-- An anvil -->
        <Path Data="M 11,17 L 33,17 L 33,20 C 29,20 27,22 26,25 L 26,28 L 30,31 L 14,31 L 18,28 L 18,25
                    C 17,22 14,21 9,20 C 9,18 10,17 11,17 Z"
              Stretch="None">
          <Path.Fill>
            <LinearGradientBrush StartPoint="0,0" EndPoint="0,1">
              <GradientStop Color="#FFF3DFA2" Offset="0"/>
              <GradientStop Color="#FFB08840" Offset="1"/>
            </LinearGradientBrush>
          </Path.Fill>
        </Path>
      </Grid>
      <StackPanel Grid.Column="1" Margin="14,0,0,0" VerticalAlignment="Center">
        <TextBlock Text="E V O L U T I O N S" FontFamily="Palatino Linotype" FontSize="11" FontWeight="SemiBold"
                   Foreground="#FFB8964F"/>
        <TextBlock x:Name="TitleText" FontFamily="Palatino Linotype" FontSize="22" Foreground="#FFF2E6C8"
                   TextTrimming="CharacterEllipsis" Margin="0,1,0,0"/>
      </StackPanel>
      <StackPanel Grid.Column="2" Orientation="Horizontal" VerticalAlignment="Top">
        <Border x:Name="StatusPill" CornerRadius="11" Padding="11,3,11,4" VerticalAlignment="Center"
                BorderThickness="1" BorderBrush="#FF6E5530" Background="#FF2C2216">
          <StackPanel Orientation="Horizontal">
            <Ellipse x:Name="StatusDot" Width="7" Height="7" Fill="#FFE6C47A" VerticalAlignment="Center"/>
            <TextBlock x:Name="StatusText" Margin="7,0,0,0" FontFamily="Segoe UI" FontSize="11.5"
                       FontWeight="SemiBold" Foreground="#FFE6C47A"/>
          </StackPanel>
        </Border>
        <Button x:Name="CloseButton" Width="26" Height="26" Margin="10,0,0,0" Cursor="Hand"
                VerticalAlignment="Center" Focusable="False">
          <Button.Template>
            <ControlTemplate TargetType="Button">
              <Border x:Name="Back" CornerRadius="13" Background="Transparent">
                <Path Data="M 8,8 L 18,18 M 18,8 L 8,18" Stroke="#FF8C7A5A" StrokeThickness="1.6"
                      StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
              </Border>
              <ControlTemplate.Triggers>
                <Trigger Property="IsMouseOver" Value="True">
                  <Setter TargetName="Back" Property="Background" Value="#FF3A2D1D"/>
                </Trigger>
              </ControlTemplate.Triggers>
            </ControlTemplate>
          </Button.Template>
        </Button>
      </StackPanel>
    </Grid>

    <!-- The whole run -->
    <StackPanel Grid.Row="1" Margin="26,18,26,0">
      <Grid>
        <Grid.ColumnDefinitions>
          <ColumnDefinition Width="Auto"/>
          <ColumnDefinition Width="*"/>
        </Grid.ColumnDefinitions>
        <TextBlock x:Name="PercentText" FontFamily="Palatino Linotype" FontSize="34" FontWeight="SemiBold"
                   VerticalAlignment="Bottom">
          <TextBlock.Foreground>
            <LinearGradientBrush StartPoint="0,0" EndPoint="0,1">
              <GradientStop Color="#FFF6E4AE" Offset="0"/>
              <GradientStop Color="#FFC99A45" Offset="1"/>
            </LinearGradientBrush>
          </TextBlock.Foreground>
        </TextBlock>
        <StackPanel Grid.Column="1" HorizontalAlignment="Right" VerticalAlignment="Bottom" Margin="0,0,0,7">
          <TextBlock x:Name="RemainingText" HorizontalAlignment="Right" FontFamily="Segoe UI" FontSize="13"
                     FontWeight="SemiBold" Foreground="#FFE8D9B5"/>
          <TextBlock x:Name="ElapsedText" HorizontalAlignment="Right" FontFamily="Segoe UI" FontSize="11.5"
                     Foreground="#FF8E7C5C" Margin="0,1,0,0"/>
        </StackPanel>
      </Grid>
      <Border x:Name="BarTrack" Height="14" CornerRadius="7" Margin="0,8,0,0" BorderThickness="1"
              BorderBrush="#FF3D3021" Background="#FF0C0906" ClipToBounds="True">
        <Grid>
          <Border x:Name="BarFill" HorizontalAlignment="Left" Width="0" CornerRadius="6">
            <Border.Background>
              <LinearGradientBrush StartPoint="0,0" EndPoint="1,0">
                <GradientStop Color="#FF7E5C22" Offset="0"/>
                <GradientStop Color="#FFD2A953" Offset="0.6"/>
                <GradientStop Color="#FFF6E3A9" Offset="1"/>
              </LinearGradientBrush>
            </Border.Background>
            <Grid ClipToBounds="True">
              <Border CornerRadius="6,6,0,0" Height="6" VerticalAlignment="Top" Background="#2EFFFFFF"/>
              <Rectangle x:Name="Shimmer" Width="90" HorizontalAlignment="Left">
                <Rectangle.Fill>
                  <LinearGradientBrush StartPoint="0,0" EndPoint="1,0">
                    <GradientStop Color="#00FFF6D8" Offset="0"/>
                    <GradientStop Color="#66FFF6D8" Offset="0.5"/>
                    <GradientStop Color="#00FFF6D8" Offset="1"/>
                  </LinearGradientBrush>
                </Rectangle.Fill>
                <Rectangle.RenderTransform>
                  <TranslateTransform x:Name="ShimmerMove" X="-90"/>
                </Rectangle.RenderTransform>
              </Rectangle>
            </Grid>
          </Border>
        </Grid>
      </Border>
    </StackPanel>

    <!-- The steps -->
    <StackPanel Grid.Row="2" x:Name="StepList" Margin="16,18,16,16"/>

    <!-- The last lines of output -->
    <Border Grid.Row="3" CornerRadius="0,0,14,14" Background="#FF0B0806" BorderThickness="0,1,0,0"
            BorderBrush="#FF2E2418" Padding="26,11,26,13">
      <StackPanel>
        <TextBlock x:Name="LogOld2" FontFamily="Consolas" FontSize="11" Foreground="#FF4E4332"
                   TextTrimming="CharacterEllipsis"/>
        <TextBlock x:Name="LogOld1" FontFamily="Consolas" FontSize="11" Foreground="#FF6A5B43"
                   TextTrimming="CharacterEllipsis" Margin="0,2,0,0"/>
        <TextBlock x:Name="LogNow" FontFamily="Consolas" FontSize="11" Foreground="#FFA89574"
                   TextTrimming="CharacterEllipsis" Margin="0,2,0,0"/>
      </StackPanel>
    </Border>
  </Grid>
</Border>
'@

# One step: its state icon (every variant, the right one shown), its name and what it is doing, its time
$rowXaml = @'
<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        x:Name="Row" CornerRadius="9" Padding="10,9,14,9" Margin="0,0,0,4" Background="Transparent">
  <Grid>
    <Grid.ColumnDefinitions>
      <ColumnDefinition Width="3"/>
      <ColumnDefinition Width="40"/>
      <ColumnDefinition Width="*"/>
      <ColumnDefinition Width="Auto"/>
    </Grid.ColumnDefinitions>
    <Border x:Name="Accent" Grid.Column="0" CornerRadius="2" Background="#FFE0B862" Margin="0,2" Visibility="Hidden"/>

    <Grid Grid.Column="1" Width="22" Height="22" HorizontalAlignment="Center" VerticalAlignment="Center">
      <Ellipse x:Name="IconWaiting" Stroke="#FF55462F" StrokeThickness="1.6"/>
      <Grid x:Name="IconRunning" Visibility="Collapsed">
        <Ellipse Stroke="#FF3A2E1F" StrokeThickness="2.4"/>
        <Path Width="22" Height="22" Data="M 11,1.2 A 9.8,9.8 0 0 1 20.8,11" Stroke="#FFF0CF7E" StrokeThickness="2.4"
              StrokeStartLineCap="Round" StrokeEndLineCap="Round" RenderTransformOrigin="0.5,0.5">
          <Path.RenderTransform>
            <RotateTransform x:Name="Spin" Angle="0"/>
          </Path.RenderTransform>
        </Path>
      </Grid>
      <Grid x:Name="IconDone" Visibility="Collapsed">
        <Ellipse>
          <Ellipse.Fill>
            <LinearGradientBrush StartPoint="0,0" EndPoint="0,1">
              <GradientStop Color="#FFF3DC9C" Offset="0"/>
              <GradientStop Color="#FFB88A3A" Offset="1"/>
            </LinearGradientBrush>
          </Ellipse.Fill>
        </Ellipse>
        <Path Data="M 6.5,11.5 L 9.7,14.6 L 15.8,7.8" Stroke="#FF1B140C" StrokeThickness="2.1"
              StrokeStartLineCap="Round" StrokeEndLineCap="Round" StrokeLineJoin="Round"/>
      </Grid>
      <Grid x:Name="IconFailed" Visibility="Collapsed">
        <Ellipse Fill="#FFA4563B"/>
        <Path Data="M 7.5,7.5 L 14.5,14.5 M 14.5,7.5 L 7.5,14.5" Stroke="#FFF6E6D6" StrokeThickness="2"
              StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
      </Grid>
      <Grid x:Name="IconBlocked" Visibility="Collapsed">
        <Ellipse Stroke="#FFE0B862" StrokeThickness="1.6" StrokeDashArray="2.2,1.6"/>
        <Path Data="M 8.7,7 L 8.7,15 M 13.3,7 L 13.3,15" Stroke="#FFE0B862" StrokeThickness="2"
              StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
      </Grid>
      <Grid x:Name="IconSkipped" Visibility="Collapsed">
        <Ellipse Stroke="#FF4A3E2B" StrokeThickness="1.6"/>
        <Path Data="M 7,11 L 15,11" Stroke="#FF6A5A40" StrokeThickness="1.8" StrokeStartLineCap="Round"
              StrokeEndLineCap="Round"/>
      </Grid>
    </Grid>

    <StackPanel Grid.Column="2" VerticalAlignment="Center" Margin="2,0,10,0">
      <StackPanel Orientation="Horizontal">
        <TextBlock x:Name="Name" FontFamily="Segoe UI" FontSize="14" FontWeight="SemiBold" Foreground="#FF7D6C51"/>
        <TextBlock x:Name="Caption" FontFamily="Segoe UI" FontSize="11.5" Foreground="#FF5E5039" Margin="8,0,0,0"
                   VerticalAlignment="Bottom" Padding="0,0,0,1"/>
      </StackPanel>
      <TextBlock x:Name="Detail" FontFamily="Segoe UI" FontSize="11.5" Foreground="#FF8E7C5C" Margin="0,2,0,0"
                 TextTrimming="CharacterEllipsis" Visibility="Collapsed"/>
      <Border x:Name="MiniTrack" Height="3" CornerRadius="1.5" Background="#FF302518" Margin="0,6,0,1"
              Visibility="Collapsed">
        <Border x:Name="MiniFill" HorizontalAlignment="Left" Width="0" CornerRadius="1.5" Background="#FFD9B25F"/>
      </Border>
    </StackPanel>

    <TextBlock x:Name="Time" Grid.Column="3" FontFamily="Consolas" FontSize="12.5" Foreground="#FF6E5F46"
               VerticalAlignment="Center"/>
  </Grid>
</Border>
'@

function New-Brush([string]$hex) {
    $color = [System.Windows.Media.ColorConverter]::ConvertFromString($hex)
    $brush = New-Object System.Windows.Media.SolidColorBrush $color
    $brush.Freeze()
    return $brush
}
$brushName = @{ waiting = New-Brush '#FF7D6C51'; active = New-Brush '#FFF4E7C6'; done = New-Brush '#FFD8C8A2'
    failed = New-Brush '#FFE0A084'; skipped = New-Brush '#FF5A4C37'
}
$brushRowActive = New-Brush '#FF261D13'
$brushRowFailed = New-Brush '#FF2A1A12'
$brushGold = New-Brush '#FFE6C47A'
$brushRust = New-Brush '#FFD98E6C'
$brushPillGold = New-Brush '#FF2C2216'
$brushPillRust = New-Brush '#FF34201A'

$root = [System.Windows.Markup.XamlReader]::Parse($rootXaml)
$ui = @{}
foreach ($name in 'TitleText', 'StatusPill', 'StatusDot', 'StatusText', 'CloseButton', 'PercentText', 'RemainingText',
    'ElapsedText', 'BarTrack', 'BarFill', 'Shimmer', 'ShimmerMove', 'StepList', 'LogOld2', 'LogOld1', 'LogNow') {
    $ui[$name] = $root.FindName($name)
}
$ui.TitleText.Text = $title

foreach ($step in $plan) {
    $row = [System.Windows.Markup.XamlReader]::Parse($rowXaml)
    $parts = @{ Root = $row }
    foreach ($name in 'Accent', 'IconWaiting', 'IconRunning', 'IconDone', 'IconFailed', 'IconBlocked', 'IconSkipped',
        'Spin', 'Name', 'Caption', 'Detail', 'MiniTrack', 'MiniFill', 'Time') {
        $parts[$name] = $row.FindName($name)
    }
    $parts.Name.Text = $step.Label
    $parts.Caption.Text = $step.Caption
    $step.Ui = $parts
    [void]$ui.StepList.Children.Add($row)
}

# --- Progress ----------------------------------------------------------------------------------------------------
$script:startedAll = Get-Date
$script:finishedAt = $null
$script:failed = $null
$script:current = -1
$script:process = $null
$script:readPosition = 0
$script:lines = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)

function Format-Duration([double]$seconds) {
    $seconds = [Math]::Max(0, [Math]::Round($seconds))
    return '{0}:{1:00}' -f [int][Math]::Floor($seconds / 60), [int]($seconds % 60)
}

# How far a step is: its markers, or its own [n/m] counter, or the time it usually takes, whichever says more.
# Never quite done until it has exited.
function Update-Fraction($step) {
    switch ($step.State) {
        'done' { $step.Fraction = 1.0 }
        'running' {
            $byMarkers = if ($step.Markers.Count) { $step.Reached / $step.Markers.Count } else { 0.0 }
            $seconds = ((Get-Date) - $step.Started).TotalSeconds
            $byTime = 0.92 * [Math]::Min(1.0, $seconds / [Math]::Max(1, $step.Expected))
            $step.Fraction = [Math]::Min(0.98, [Math]::Max($step.Fraction, [Math]::Max($byMarkers, $byTime)))
        }
    }
}

function Read-NewOutput($step) {
    if (-not (Test-Path -LiteralPath $step.Log)) { return }
    $stream = [System.IO.File]::Open($step.Log, 'Open', 'Read', 'ReadWrite')
    try {
        if ($stream.Length -le $script:readPosition) { return }
        [void]$stream.Seek($script:readPosition, 'Begin')
        $buffer = New-Object byte[] ($stream.Length - $script:readPosition)
        $read = $stream.Read($buffer, 0, $buffer.Length)
    }
    finally {
        $stream.Dispose()
    }
    # Only whole lines: the rest is read again once it is complete
    $end = [Array]::LastIndexOf($buffer, [byte]10, $read - 1)
    if ($end -lt 0) { return }
    $script:readPosition += $end + 1
    # The steps' tools do not agree on an encoding: UTF-8 when it is valid, the system code page otherwise
    try { $text = $strictUtf8.GetString($buffer, 0, $end + 1) }
    catch { $text = [System.Text.Encoding]::Default.GetString($buffer, 0, $end + 1) }

    foreach ($line in ($text -split "`r?`n")) {
        $trimmed = $line.Trim()
        if (-not $trimmed) { continue }
        $script:lines.Add($trimmed)
        for ($index = $step.Reached; $index -lt $step.Markers.Count; ++$index) {
            if ($trimmed.Contains($step.Markers[$index].Match)) {
                $step.Reached = $index + 1
                break
            }
        }
        if ($trimmed -match '^\[(\d+)/(\d+)\]' -and [int]$Matches[2] -gt 0) {
            $step.Fraction = [Math]::Max($step.Fraction, [Math]::Min(0.98, [int]$Matches[1] / [int]$Matches[2]))
        }
        if ($trimmed -match '^[\w\-\.]+\.(cpp|c|cc)$') {
            $step.File = $trimmed
        }
    }
}

function Set-Icon($step, $which) {
    foreach ($name in 'IconWaiting', 'IconRunning', 'IconDone', 'IconFailed', 'IconBlocked', 'IconSkipped') {
        $step.Ui[$name].Visibility = if ($name -eq $which) { 'Visible' } else { 'Collapsed' }
    }
}

function Update-Row($step) {
    $parts = $step.Ui
    $active = $step.State -in 'running', 'blocked'
    $parts.Root.Background = [System.Windows.Media.Brushes]::Transparent
    if ($active) { $parts.Root.Background = $brushRowActive }
    elseif ($step.State -eq 'failed') { $parts.Root.Background = $brushRowFailed }
    $parts.Accent.Visibility = if ($active) { 'Visible' } else { 'Hidden' }
    $parts.MiniTrack.Visibility = if ($step.State -eq 'running') { 'Visible' } else { 'Collapsed' }

    switch ($step.State) {
        'waiting' {
            Set-Icon $step 'IconWaiting'
            $parts.Name.Foreground = $brushName.waiting
            $parts.Detail.Visibility = 'Collapsed'
            $parts.Time.Text = '~' + (Format-Duration $step.Expected)
        }
        'blocked' {
            Set-Icon $step 'IconBlocked'
            $parts.Name.Foreground = $brushName.active
            $parts.Detail.Visibility = 'Visible'
            $parts.Detail.Text = 'En attente : fermez World of Warcraft pour continuer'
            $parts.Detail.Foreground = $brushGold
            $parts.Time.Text = ''
        }
        'running' {
            Set-Icon $step 'IconRunning'
            $parts.Name.Foreground = $brushName.active
            $parts.Detail.Visibility = 'Visible'
            $parts.Detail.Foreground = $brushName.waiting
            $part = if ($step.Reached -gt 0) { $step.Markers[$step.Reached - 1].Label } else { $step.Initial }
            $text = $part
            if ($step.Markers.Count) {
                $position = [Math]::Min($step.Reached + 1, $step.Markers.Count)
                $text = '{0}/{1}  ·  {2}' -f $position, $step.Markers.Count, $part
            }
            if ($step.File) { $text += "  ·  $($step.File)" }
            $parts.Detail.Text = $text
            $parts.MiniFill.Width = [Math]::Max(0, 380 * $step.Fraction)
            $parts.Time.Text = Format-Duration ((Get-Date) - $step.Started).TotalSeconds
        }
        'done' {
            Set-Icon $step 'IconDone'
            $parts.Name.Foreground = $brushName.done
            $parts.Detail.Visibility = 'Collapsed'
            $parts.Time.Text = Format-Duration $step.Elapsed.TotalSeconds
        }
        'failed' {
            Set-Icon $step 'IconFailed'
            $parts.Name.Foreground = $brushName.failed
            $parts.Detail.Visibility = 'Visible'
            $parts.Detail.Foreground = $brushRust
            $parts.Detail.Text = if ($step.Summary) { $step.Summary } else { "Échec : voir $($step.Log)" }
            $parts.Time.Text = if ($step.Elapsed) { Format-Duration $step.Elapsed.TotalSeconds } else { '' }
        }
        'skipped' {
            Set-Icon $step 'IconSkipped'
            $parts.Name.Foreground = $brushName.skipped
            $parts.Detail.Visibility = 'Visible'
            $parts.Detail.Foreground = $brushName.skipped
            $parts.Detail.Text = 'Annulé'
            $parts.Time.Text = ''
        }
    }
}

function Update-View {
    $total = 0.0
    $done = 0.0
    $remaining = 0.0
    foreach ($step in $plan) {
        Update-Fraction $step
        $total += $step.Expected
        $done += $step.Expected * $step.Fraction
        if ($step.State -in 'waiting', 'blocked', 'running') { $remaining += $step.Expected * (1 - $step.Fraction) }
        Update-Row $step
    }

    $fraction = if ($total -gt 0) { $done / $total } else { 1.0 }
    if ($script:finishedAt -and -not $script:failed) { $fraction = 1.0 }
    $ui.PercentText.Text = '{0} %' -f [int][Math]::Floor($fraction * 100)
    $ui.BarFill.Width = [Math]::Max(0, ($ui.BarTrack.ActualWidth - 2) * $fraction)
    $elapsedAll = ((Get-Date) - $script:startedAll).TotalSeconds
    if ($script:finishedAt) { $elapsedAll = ($script:finishedAt - $script:startedAll).TotalSeconds }
    $ui.ElapsedText.Text = "Écoulé $(Format-Duration $elapsedAll)"

    if ($script:failed) {
        $ui.StatusText.Text = 'Échec'
        $ui.RemainingText.Text = "Échec : $($script:failed.Label)"
    }
    elseif ($script:finishedAt) {
        $ui.StatusText.Text = 'Terminé'
        $ui.RemainingText.Text = 'Tout est à jour'
    }
    elseif ($plan | Where-Object { $_.State -eq 'blocked' }) {
        $ui.StatusText.Text = 'En attente'
        $ui.RemainingText.Text = 'Fermez WoW pour continuer'
    }
    else {
        $ui.StatusText.Text = 'En cours'
        $ui.RemainingText.Text = "Environ $(Format-Duration $remaining) restantes"
    }
    $statusBrush = if ($script:failed) { $brushRust } else { $brushGold }
    $ui.StatusText.Foreground = $statusBrush
    $ui.StatusDot.Fill = $statusBrush
    $ui.StatusPill.Background = if ($script:failed) { $brushPillRust } else { $brushPillGold }

    $count = $script:lines.Count
    $ui.LogNow.Text = if ($count -ge 1) { '›  ' + $script:lines[$count - 1] } else { '›  …' }
    $ui.LogOld1.Text = if ($count -ge 2) { '   ' + $script:lines[$count - 2] } else { '' }
    $ui.LogOld2.Text = if ($count -ge 3) { '   ' + $script:lines[$count - 3] } else { '' }
}

function Invoke-Layout {
    $infinite = [System.Windows.Size]::new([double]::PositiveInfinity, [double]::PositiveInfinity)
    $root.Measure($infinite)
    $root.Arrange([System.Windows.Rect]::new(0, 0, $root.DesiredSize.Width, $root.DesiredSize.Height))
    $root.UpdateLayout()
}

# --- Preview: made-up progress, rendered to a PNG, nothing shown ------------------------------------------------
if ($preview) {
    $now = Get-Date
    $script:startedAll = $now.AddSeconds(-331)
    $states = @('done', 'done', 'running', 'waiting', 'waiting')
    for ($index = 0; $index -lt $plan.Count; ++$index) {
        $step = $plan[$index]
        $step.State = $states[[Math]::Min($index, $states.Count - 1)]
        if ($step.State -eq 'done') { $step.Elapsed = [TimeSpan]::FromSeconds($step.Expected * 1.04) }
        if ($step.State -eq 'running') {
            $step.Started = $now.AddSeconds(-74)
            $step.Reached = [int]($step.Markers.Count / 2)
        }
    }
    foreach ($line in 'Built 112 paragon node icons in ...\modules\mod-stat-growth\client-assets\compiled',
        'Compiling Paragon interface art...', 'Built Paragon-Node-Keystone: 256x256, DXT3') {
        $script:lines.Add($line)
    }
    Invoke-Layout
    Update-View
    Invoke-Layout
    Update-View
    Invoke-Layout

    # The margin around the card is where its shadow falls
    $scale = 1.5
    $width = [int](($root.ActualWidth + $root.Margin.Left + $root.Margin.Right) * $scale)
    $height = [int](($root.ActualHeight + $root.Margin.Top + $root.Margin.Bottom) * $scale)
    $bitmap = New-Object System.Windows.Media.Imaging.RenderTargetBitmap $width, $height, (96 * $scale), (96 * $scale),
        ([System.Windows.Media.PixelFormats]::Pbgra32)
    $backdrop = New-Object System.Windows.Media.DrawingVisual
    $context = $backdrop.RenderOpen()
    $context.DrawRectangle((New-Brush '#FF3C4450'), $null, [System.Windows.Rect]::new(0, 0, $width, $height))
    $context.Close()
    $bitmap.Render($backdrop)
    $bitmap.Render($root)
    $encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $file = [System.IO.File]::Create($preview)
    try { $encoder.Save($file) } finally { $file.Dispose() }
    Write-Output "Preview written to $preview"
    exit 0
}

# --- Running the steps: a state machine on the window's timer, so the window never freezes -----------------------
function Start-Step($step) {
    Remove-Item -LiteralPath $step.Log, "$($step.Log).err" -ErrorAction SilentlyContinue
    $script:readPosition = 0
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

function Save-History($step) {
    $seconds = $step.Elapsed.TotalSeconds
    if ($history.ContainsKey($step.Name)) { $seconds = 0.6 * $history[$step.Name] + 0.4 * $seconds }
    $history[$step.Name] = $seconds
    try {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $historyPath) | Out-Null
        $history | ConvertTo-Json | Set-Content -LiteralPath $historyPath -Encoding UTF8
    }
    catch { }
}

function Stop-Run($failedStep) {
    $script:failed = $failedStep
    foreach ($rest in $plan) { if ($rest.State -in 'waiting', 'blocked') { $rest.State = 'skipped' } }
    $script:finishedAt = Get-Date
    $ui.Shimmer.Visibility = 'Collapsed'
}

$window = [System.Windows.Markup.XamlReader]::Parse($windowXaml)
$window.Content = $root
$root.Add_MouseLeftButtonDown({ try { $window.DragMove() } catch { } })

# The spinners and the shimmer on the bar
$spinTime = [System.Windows.Duration]::new([TimeSpan]::FromSeconds(1.1))
$spin = New-Object System.Windows.Media.Animation.DoubleAnimation (0, 360, $spinTime)
$spin.RepeatBehavior = [System.Windows.Media.Animation.RepeatBehavior]::Forever
foreach ($step in $plan) { $step.Ui.Spin.BeginAnimation([System.Windows.Media.RotateTransform]::AngleProperty, $spin) }
$shimmerTime = [System.Windows.Duration]::new([TimeSpan]::FromSeconds(2.2))
$shimmer = New-Object System.Windows.Media.Animation.DoubleAnimation (-90, 600, $shimmerTime)
$shimmer.RepeatBehavior = [System.Windows.Media.Animation.RepeatBehavior]::Forever
$ui.ShimmerMove.BeginAnimation([System.Windows.Media.TranslateTransform]::XProperty, $shimmer)

$timer = New-Object System.Windows.Threading.DispatcherTimer
$timer.Interval = [TimeSpan]::FromMilliseconds(250)
$timer.Add_Tick({
        try {
            if ($script:finishedAt) {
                $hold = if ($script:failed) { 20 } else { 6 }
                $left = $hold - ((Get-Date) - $script:finishedAt).TotalSeconds
                if ($left -le 0) {
                    $timer.Stop()
                    $window.Close()
                    return
                }
                $ui.LogNow.Text = '›  Fermeture dans {0} s' -f [int][Math]::Ceiling($left)
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
                        Save-History $step
                    }
                    else {
                        $step.State = 'failed'
                        $errorLog = "$($step.Log).err"
                        if (Test-Path -LiteralPath $errorLog) {
                            $step.Summary = ((Get-Content -LiteralPath $errorLog -Tail 2) -join ' ').Trim()
                        }
                        Stop-Run $step
                        Update-View
                        return
                    }
                }
            }

            $next = $script:current + 1
            if ($next -ge $plan.Count) {
                $script:finishedAt = Get-Date
                $ui.Shimmer.Visibility = 'Collapsed'
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
        }
        catch {
            $script:lines.Add("Erreur de la fenêtre : $_")
        }
    })

$ui.CloseButton.Add_Click({ $window.Close() })
$window.Add_Closing({
        param($sender, $closing)
        # Closing the window by hand does not leave a step running unseen
        if ($script:finishedAt) { return }
        $running = $script:process -and -not $script:process.HasExited
        $question = 'Annuler la mise à jour ?'
        if ($running) { $question = 'Une étape est en cours. Arrêter la mise à jour ?' }
        $answer = [System.Windows.MessageBox]::Show($question, 'Evolutions', 'YesNo', 'Warning')
        if ($answer -ne 'Yes') {
            $closing.Cancel = $true
            return
        }
        if ($running) {
            & taskkill.exe /PID $script:process.Id /T /F | Out-Null
            $plan[$script:current].State = 'failed'
            $plan[$script:current].Summary = 'Arrêtée depuis la fenêtre'
            Stop-Run $plan[$script:current]
        }
        else {
            $blocked = $plan | Where-Object { $_.State -eq 'blocked' } | Select-Object -First 1
            if ($blocked) { $blocked.Summary = 'Annulée depuis la fenêtre' }
            Stop-Run $blocked
        }
    })

$window.Add_Loaded({
        Update-View
        $timer.Start()
    })
[void]$window.ShowDialog()

# --- The summary, for whoever started it -----------------------------------------------------------------------
foreach ($step in $plan) {
    $time = if ($step.Elapsed) { Format-Duration $step.Elapsed.TotalSeconds } else { '' }
    $line = '{0,-8} {1,-8} {2}' -f $step.Name, $step.State, $time
    if ($step.Summary) { $line += "  $($step.Summary)" }
    Write-Output $line
    if ($step.State -in 'done', 'failed') {
        $tail = 2
        if ($step.State -eq 'failed') { $tail = 25 }
        Get-Content -LiteralPath $step.Log -Tail $tail -ErrorAction SilentlyContinue |
            ForEach-Object { Write-Output "    $_" }
    }
}
if ($script:failed -or ($plan | Where-Object { $_.State -ne 'done' })) {
    exit 1
}
exit 0
