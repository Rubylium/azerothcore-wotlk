# The Pestiféré's looks imported from the Ascension client (localTools/pestifere/ascensionVisuals.json, through
# localTools/ascensionImport/importVisuals.py), for its rows in patchSinisterStrike.ps1: PestifereLook gives the
# SpellVisual id the import gave a key, for field 131.
$pestifereLooks = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\mod-pestifere\client-assets\imported\visuals.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json
function PestifereLook([string]$key) {
    $id = $pestifereLooks.ids.$key
    if (-not $id) { throw "No imported Pestiféré look '$key' (localTools\pestifere\ascensionVisuals.json)." }
    return [uint32]$id
}
