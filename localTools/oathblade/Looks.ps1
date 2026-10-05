# The Oathblade's looks imported from the Ascension client (localTools/oathblade/ascensionVisuals.json, through
# localTools/ascensionImport/importVisuals.py), for Spells.ps1: OathbladeLook gives the SpellVisual id the import gave
# a key, for field 131.
$oathbladeLooks = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\mod-oathblade\client-assets\imported\visuals.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json
function OathbladeLook([string]$key) {
    $id = $oathbladeLooks.ids.$key
    if (-not $id) { throw "No imported Oathblade look '$key' (localTools\oathblade\ascensionVisuals.json)." }
    return [uint32]$id
}
