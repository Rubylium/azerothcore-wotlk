# The stock Shaman spells localTools/patchSinisterStrike.ps1 changes in place for the retail-style Shaman
# (localTools/shaman/talentTree.json, modules/mod-shaman). Each entry names its spells by family and English name
# (every rank: a trainer's spells keep their chain) or by id, and gives the fields to write (see Spells.ps1 for the
# field numbers), and optionally new Effects, a Description and an AuraDescription. A spell named here is only changed
# when it had a cost.
#
# The Shaman keeps mana and its totems. What changes: the shocks no longer share a cooldown (category 19, 6 s): Earth
# Shock and Frost Shock have none, as retail plays them (Elemental spends Maelstrom on Earth Shock and empowers Frost
# Shock with Icefury), Flame Shock keeps its own 6 s; Chain Lightning has no cooldown and reaches 5 enemies, as retail
# (Enhancement casts it instantly on Maelstrom Weapon, Elemental fills packs with it).

$shaman = 11

$edits = @(
    # Earth Shock: no shared cooldown; Elemental spends 60 Maelstrom instead of mana (mod-shaman)
    @{ Family = $shaman; Name = 'Earth Shock'; Fields = @{ 1 = 0; 30 = 0 }
       Description = "Choque la cible : dégâts de Nature et vitesse d'attaque réduite de 10% pendant 8 s. Élémentaire : consomme 60 points de Maelström au lieu de mana et inflige 150% de dégâts en plus." },
    # Flame Shock: its own 6 s cooldown
    @{ Family = $shaman; Name = 'Flame Shock'; Fields = @{ 1 = 0; 29 = 6000; 30 = 0 } },
    # Frost Shock: no cooldown
    @{ Family = $shaman; Name = 'Frost Shock'; Fields = @{ 1 = 0; 30 = 0 } },
    # Chain Lightning: no cooldown (category 85, 6 s), 5 targets instead of 3 (field 104, the effect's chain targets)
    @{ Family = $shaman; Name = 'Chain Lightning'; Fields = @{ 1 = 0; 30 = 0; 104 = 5 }
       Description = "Projette un éclair sur l'ennemi : dégâts de Nature, puis l'éclair rebondit sur d'autres ennemis proches, 5 cibles en tout. Chaque rebond réduit les dégâts de 30%." }
)

return $edits
