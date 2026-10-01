# The stock Warrior spells localTools/patchSinisterStrike.ps1 changes in place for the retail-style Warrior
# (localTools/warrior/talentTree.json, modules/mod-warrior). Each entry names its spells by family and English name
# (every rank: a trainer's spells keep their chain) or by id, and gives the fields to write (see Spells.ps1 for the
# field numbers), and optionally new Effects, a Description and an AuraDescription. A spell named here is only changed
# when it had a cost; the ones that cost nothing (Taunt, Shield Wall, Recklessness...) are named by id.
#
# The Warrior keeps rage and its stances, but no ability asks for one any more (field 12): every specialization plays
# its whole kit in any stance, as retail does. What else changes: Overpower needs no dodge (12 s, 2 charges for Arms in
# mod-warrior), Revenge needs no block, dodge or parry and strikes a cone in front (a dodge, a parry or a block makes
# it free, mod-warrior), Execute's health check moves to mod-warrior (below 20%, 35% with Massacre, or Sudden Death),
# Slam is instant, Whirlwind has no cooldown and, like Bladestorm's whirls, no target cap (mod-warrior's falloff past
# five), Shield Slam, Thunder Clap and Bloodthirst give rage instead of costing it (mod-warrior), Shield Block lasts
# 6 s on a 16 s cooldown (2 charges for Protection), Recklessness is 20% critical strike chance for 12 s every 1 min
# 30 s, Shield Wall 3 min, Spell Reflection any weapon every 25 s.

$warrior = 4
$noStance = @{ 12 = 0; 13 = 0 }

function Fields($extra = @{}) {
    $fields = @{}
    foreach ($key in $noStance.Keys) { $fields[$key] = $noStance[$key] }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$edits = @(
    # --- Any stance ---------------------------------------------------------------------------------------------------
    @{ Family = $warrior; Name = 'Rend'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Hamstring'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Mocking Blow'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Disarm'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Shield Bash'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Pummel'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Intercept'; Fields = (Fields) },
    @{ Family = $warrior; Name = 'Intervene'; Fields = (Fields) },
    # Charge, Taunt, Retaliation and Sweeping Strikes cost nothing: by id (Charge stays out of combat without
    # Warbringer or Juggernaut)
    @{ Id = 100; Fields = (Fields) },
    @{ Id = 6178; Fields = (Fields) },
    @{ Id = 11578; Fields = (Fields) },
    @{ Id = 355; Fields = (Fields) },
    @{ Id = 20230; Fields = (Fields @{ 1 = 0; 30 = 0 }) },
    @{ Id = 12328; Fields = (Fields) },

    # --- Arms ---------------------------------------------------------------------------------------------------------
    # Overpower: no dodge needed (attributes ex lose 0x00100000, the combo point it read), its own 12 s cooldown (its
    # category was shared with Revenge), free
    @{ Family = $warrior; Name = 'Overpower'; Fields = (Fields @{ 1 = 0; 5 = 0x58000200; 29 = 12000; 30 = 0; 42 = 0 })
       Description = "Domine l'ennemi : dégâts de l'arme. Ne peut être bloquée, esquivée ni parée. 12 s de recharge ; 2 charges en Armes." },
    # Execute: mod-warrior checks the target's health (below 20%, 35% with Massacre, any with Sudden Death)
    @{ Family = $warrior; Name = 'Execute'; Fields = (Fields @{ 21 = 0 })
       Description = "Tente d'achever un ennemi blessé : dégâts selon votre puissance d'attaque, et chaque point de rage en plus (jusqu'à 30) les augmente. Utilisable sur un ennemi sous 20% de vie." },
    # Slam: instant (it cast in 1.5 s)
    @{ Family = $warrior; Name = 'Slam'; Fields = (Fields @{ 28 = 1 }) },
    # Whirlwind: no cooldown, no target cap (the off-hand hit, 44949, and Bladestorm's whirls, 50622, by id)
    @{ Family = $warrior; Name = 'Whirlwind'; Fields = (Fields @{ 1 = 0; 30 = 0; 212 = 0 }) },
    @{ Id = 44949; Fields = @{ 212 = 0 } },
    @{ Id = 50622; Fields = @{ 212 = 0 } },

    # --- Fury ---------------------------------------------------------------------------------------------------------
    # Bloodthirst gives 8 rage (mod-warrior) instead of costing 20
    @{ Family = $warrior; Name = 'Bloodthirst'; Fields = (Fields @{ 42 = 0 }) },
    # Recklessness: 20% critical strike chance (aura 290) for 12 s, every 1 min 30 s; no charges, no damage taken
    @{ Id = 1719; Fields = (Fields @{ 1 = 0; 29 = 90000; 30 = 0; 34 = 0; 35 = 0; 36 = 0; 40 = 29 })
       Effects = @(@{ Index = 0; Effect = 6; Aura = 290; TargetA = 1; Value = 20 })
       Description = 'Vos chances de coup critique augmentent de 20% pendant 12 s.'
       AuraDescription = 'Chances de coup critique augmentées de 20%.' },

    # --- Protection ---------------------------------------------------------------------------------------------------
    # Shield Slam and Thunder Clap give rage (mod-warrior: 15 and 5) instead of costing it
    @{ Family = $warrior; Name = 'Shield Slam'; Fields = (Fields @{ 42 = 0 }) },
    @{ Family = $warrior; Name = 'Thunder Clap'; Fields = (Fields @{ 42 = 0 }) },
    # Revenge: no block, dodge or parry needed (caster aura state), no cooldown, 20 rage (free after a dodge, a parry or
    # a block: mod-warrior), every enemy in an 8 yd cone in front (target 104)
    @{ Family = $warrior; Name = 'Revenge'; Fields = (Fields @{ 1 = 0; 20 = 0; 30 = 0; 42 = 200; 86 = 104; 92 = 14 })
       Description = "Contre-attaque les ennemis devant vous, à 8 m : dégâts physiques selon votre puissance d'attaque. Gratuite après une esquive, une parade ou un blocage." },
    # Shield Block: 6 s on a 16 s cooldown (2 charges in Protection, mod-warrior)
    @{ Id = 2565; Fields = (Fields @{ 29 = 16000; 40 = 32 }) },
    # Shield Wall: 3 min, its own category (it shared one with Recklessness and Retaliation)
    @{ Id = 871; Fields = (Fields @{ 1 = 0; 29 = 180000; 30 = 0 }) },
    # Spell Reflection: any weapon, every 25 s
    @{ Family = $warrior; Name = 'Spell Reflection'; Fields = (Fields @{ 29 = 25000; 68 = -1; 69 = 0; 70 = 0 }) }
)

return $edits
