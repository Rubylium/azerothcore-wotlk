# The stock Rogue spells localTools/patchSinisterStrike.ps1 changes in place: Finesse's strikes and Shadow Dance take
# the retail looks of localTools/rogue/ascensionVisuals.json (every rank: by family and English name; Shadow Dance,
# which costs nothing, by id). Field 131 is the SpellVisual.

. (Join-Path $repoRoot 'localTools\rogue\Looks.ps1')
$rogue = 8

$edits = @(
    @{ Family = $rogue; Name = 'Backstab'; Fields = @{ 131 = (Look 'Backstab') } },
    @{ Family = $rogue; Name = 'Ambush'; Fields = @{ 131 = (Look 'Ambush') } },
    @{ Family = $rogue; Name = 'Eviscerate'; Fields = @{ 131 = (Look 'Eviscerate') } },
    @{ Family = $rogue; Name = 'Hemorrhage'; Fields = @{ 131 = (Look 'Hemorrhage') } },
    @{ Id = 51713; Fields = @{ 131 = (Look 'ShadowDance') } }
)

return $edits
