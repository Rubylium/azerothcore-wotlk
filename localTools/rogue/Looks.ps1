# The Rogue's looks imported from the Ascension client (localTools/rogue/ascensionVisuals.json, through
# localTools/ascensionImport/importVisuals.py), for Spells.ps1 and StockSpells.ps1: Look gives the SpellVisual id the
# import gave a key, for field 131.
$rogueLooks = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\mod-rogue\client-assets\imported\visuals.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json
function Look([string]$key) {
    $id = $rogueLooks.ids.$key
    if (-not $id) { throw "No imported Rogue look '$key' (localTools\rogue\ascensionVisuals.json)." }
    return [uint32]$id
}
