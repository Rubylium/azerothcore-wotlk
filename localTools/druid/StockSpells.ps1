# The stock Druid spells localTools/patchSinisterStrike.ps1 changes in place for the retail-style Druid
# (localTools/druid/talentTree.json, modules/mod-druid). Each entry names its spells by family and English name (every
# rank: a trainer's spells keep their chain) or by id, and gives the fields to write (see Spells.ps1 for the field
# numbers), and optionally new Effects, a Description and an AuraDescription. A spell named here is only changed when
# it had a cost (Tiger's Fury and Frenzied Regeneration have none: they are named by id).
#
# The Druid keeps its forms, mana, energy, combo points and rage. What changes: Shred works from any side, as retail
# (its "behind the target" requirement is spell_custom_attr data, cleared in modules/mod-druid's SQL); Tiger's Fury
# gives 15% physical damage instead of a flat bonus (mod-druid adds its 50 energy for Combat farouche); Force of
# Nature, Tranquility and Frenzied Regeneration come back on retail-like cooldowns.

$druid = 7

# Tiger's Fury: 15% more physical damage for 6 s (aura 79, physical), every rank
$tigersFury = @{ Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 1 })
    Description = "Augmente de 15% les dégâts physiques que vous infligez pendant 6 s. Combat farouche : vous rend aussitôt 50 points d'énergie."
    AuraDescription = 'Dégâts physiques augmentés de 15%.' }

$edits = @(
    # Shred: from any side (mod-druid's SQL clears its behind-the-target requirement)
    @{ Family = $druid; Name = 'Shred'
       Description = 'Lacère la cible : $s3% des dégâts de l''arme, plus un bonus. Utilisable de n''importe quel côté. Vous rapporte 1 point de combo.' },
    # Swipe (Cat): a combo point for Combat farouche (mod-druid),
    # and the enemies all around the cat within 8 yd (it was a cone in front), as retail
    @{ Family = $druid; Name = 'Swipe (Cat)'
       Effects = @(@{ Index = 0; Effect = 31; TargetA = 22; Value = 250 }); Fields = @{ 89 = 15; 92 = 14; 212 = 0 }
       Description = 'Balaye les ennemis à 8 m autour de vous : $s1% des dégâts de l''arme. Combat farouche : vous rapporte 1 point de combo.' },
    # Tranquility: 3 min (category cooldown 8 min)
    @{ Family = $druid; Name = 'Tranquility'; Fields = @{ 30 = 180000 } },
    # Force of Nature: 1 min
    @{ Id = 33831; Fields = @{ 29 = 60000 } },
    # Frenzied Regeneration: 1 min 30 s (category cooldown 3 min)
    @{ Id = 22842; Fields = @{ 30 = 90000 } }
)
foreach ($id in @(5217, 6793, 9845, 9846, 50212, 50213)) {
    $edits += @{ Id = $id; Effects = $tigersFury.Effects; Description = $tigersFury.Description
                 AuraDescription = $tigersFury.AuraDescription }
}

return $edits
