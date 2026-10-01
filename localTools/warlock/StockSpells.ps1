# The stock Warlock spells localTools/patchSinisterStrike.ps1 changes in place for the retail-style Warlock
# (localTools/warlock/talentTree.json, modules/mod-warlock). Each entry names its spells by family and English name
# (every rank: a trainer's spells keep their chain) or by id, and gives the fields to write (see Spells.ps1 for the
# field numbers), and optionally new Effects, a Description and an AuraDescription. A spell named here is only changed
# when it had a cost.
#
# The Warlock keeps mana and its demons. What changes: Soul Shards are an aura of stacks now (mod-warlock), so no spell
# asks for a Soul Shard item any more (fields 52 and 60, the first reagent and its count): the demons' summons,
# Shadowburn and Soul Fire. Chaos Bolt spends 2 shards instead of a cooldown and takes 3 s, as retail; Shadowburn
# spends 1 on a 12 s cooldown (neither costs mana any more); Incinerate takes 2 s.

$warlock = 5

$edits = @(
    # The demons, summoned without a shard
    @{ Family = $warlock; Name = 'Summon Voidwalker'; Fields = @{ 52 = 0; 60 = 0 } },
    @{ Family = $warlock; Name = 'Summon Succubus'; Fields = @{ 52 = 0; 60 = 0 } },
    @{ Family = $warlock; Name = 'Summon Felhunter'; Fields = @{ 52 = 0; 60 = 0 } },
    @{ Family = $warlock; Name = 'Summon Felguard'; Fields = @{ 52 = 0; 60 = 0 } },
    # Soul Fire: no shard item either
    @{ Family = $warlock; Name = 'Soul Fire'; Fields = @{ 52 = 0; 60 = 0 } },
    # Shadowburn: 1 Soul Shard (mod-warlock), its own 12 s cooldown (category 651 kept)
    @{ Family = $warlock; Name = 'Shadowburn'; Fields = @{ 30 = 12000; 52 = 0; 60 = 0; 204 = 0 }
       Description = "Consomme 1 Fragment d'âme : inflige des dégâts d'Ombre à la cible. Ses chances de coup critique augmentent de 50% contre une cible sous 20% de points de vie, et le fragment vous est rendu si la cible meurt dans les 5 s." },
    # Chaos Bolt: 2 Soul Shards (mod-warlock), no cooldown (category 1225, 12 s), 3 s (index 14), always a critical strike
    @{ Family = $warlock; Name = 'Chaos Bolt'; Fields = @{ 1 = 0; 28 = 14; 30 = 0; 204 = 0 }
       Description = "Consomme 2 Fragments d'âme : envoie un trait de chaos qui inflige d'énormes dégâts de Feu à la cible. Toujours un coup critique ; ne peut être ni résisté ni absorbé." },
    # Incinerate: 2 s (index 5)
    @{ Family = $warlock; Name = 'Incinerate'; Fields = @{ 28 = 5 } },
    # Seed of Corruption: 1 Soul Shard for Affliction (mod-warlock), and its blast spreads Corruption
    @{ Family = $warlock; Name = 'Seed of Corruption'
       Description = "Plante une graine démoniaque dans l'ennemi : des dégâts d'Ombre toutes les 3 s pendant 18 s ; quand la cible a subi assez de dégâts, ou meurt, la graine explose et frappe les ennemis à 15 m, qui subissent votre Corruption. Affliction : consomme 1 Fragment d'âme." }
)

return $edits
