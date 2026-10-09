# The stock Rogue spells localTools/patchSinisterStrike.ps1 changes in place: Finesse's, Assassinat's and Hors-la-loi's
# strikes, Shadow Dance, Cold Blood and the poisons' procs take the retail looks of
# localTools/rogue/ascensionVisuals.json (every rank: by family and English name, or by id where a name is shared).
# Field 131 is the SpellVisual.

. (Join-Path $repoRoot 'localTools\rogue\Looks.ps1')
$rogue = 8

$edits = @(
    @{ Family = $rogue; Name = 'Backstab'; Fields = @{ 131 = (Look 'Backstab') } },
    @{ Family = $rogue; Name = 'Ambush'; Fields = @{ 131 = (Look 'Ambush') } },
    @{ Family = $rogue; Name = 'Eviscerate'; Fields = @{ 131 = (Look 'Eviscerate') } },
    @{ Family = $rogue; Name = 'Hemorrhage'; Fields = @{ 131 = (Look 'Hemorrhage') } },
    # Hors-la-loi's builder: retail's Saber Slash on every rank (its cost stays the patcher's 0, 92191 puts 45 back)
    @{ Family = $rogue; Name = 'Sinister Strike'; Fields = @{ 131 = (Look 'OutlawSinisterStrike') } },
    @{ Id = 51713; Fields = @{ 131 = (Look 'ShadowDance') } },
    # Assassinat. Mutilate by its casting ranks: the two hits it triggers share its name and carry no visual, and would
    # swing again for each dagger. Fan of Knives by the cast (52874, its thrown knives, keeps its missile).
    @{ Family = $rogue; Name = 'Envenom'; Fields = @{ 131 = (Look 'Envenom') } },
    @{ Family = $rogue; Name = 'Garrote'; Fields = @{ 131 = (Look 'Garrote') } },
    @{ Family = $rogue; Name = 'Rupture'; Fields = @{ 131 = (Look 'Rupture') } },
    @{ Id = 51723; Fields = @{ 131 = (Look 'FanOfKnives') } },
    @{ Id = 14177; Fields = @{ 131 = (Look 'ColdBlood') } }
)
foreach ($id in 1329, 34411, 34412, 34413, 48663, 48666) {
    $edits += @{ Id = $id; Fields = @{ 131 = (Look 'Mutilate') } }
}
# The poisons' sting on the enemy: Deadly and Instant Poison's procs, every rank (their coating spells keep theirs)
foreach ($id in 2818, 2819, 11353, 11354, 25349, 26968, 27187, 57969, 57970,
        8680, 8685, 8689, 11335, 11336, 11337, 26890, 57964, 57965) {
    $edits += @{ Id = $id; Fields = @{ 131 = (Look 'PoisonSting') } }
}

return $edits
