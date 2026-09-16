$ErrorActionPreference = 'Stop'

$toolsRoot = $PSScriptRoot

& (Join-Path $toolsRoot 'stopAll.ps1')
& (Join-Path $toolsRoot 'startAll.ps1')
