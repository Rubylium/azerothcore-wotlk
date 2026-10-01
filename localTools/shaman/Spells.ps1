# The Shaman's spell data for its retail-style talent trees (localTools/shaman/talentTree.json): the abilities its
# nodes and specializations teach, the auras its scripted talents show, and the rank spells of its new talents.
# Behaviour lives in modules/mod-shaman; this file owns client/server spell data only. The stock Shaman spells it
# changes in place (the shocks' shared cooldown gone, Chain Lightning without one...) are in
# localTools/shaman/StockSpells.ps1.
# Ids 95300-95599: 95300-95419 talent ranks (generated from the tree), 95400-95579 abilities and auras, 95580-95589
# the specializations' passives.
#
# Clone field notes: 1 category, 9 attributes ex 5 (0x8 usable while stunned), 12-13 stances, 16 target flags (0x40 a
# ground target), 28 casting time index (1 instant, 16 1.5 s, 5 2 s), 29 cooldown, 30 category cooldown, 40 duration
# index (39 2 s, 27 3 s, 28 5 s, 32 6 s, 31 8 s, 1 10 s, 29 12 s, 8 15 s, 21 never), 41 power type (0 mana), 42 cost,
# 46 range index (1 self, 2 melee, 4 30 yd, 5 40 yd, 6 100 yd), 47 speed (0: the hit lands with the cast), 72-73
# effects 1-2, 83-85 effect mechanic (7 root, 11 snare, 12 stun), 86-88 target A, 89-91 target B (15 enemies around
# the source, 55 the leap's point in front), 92-94 radius index (13 10 yd, 14 8 yd, 9 20 yd), 98-100 periodic interval,
# 122-130 the effects' class masks, 131 visual, 204 cost as a share of base mana, 205-206 global cooldown category and
# time (0 0: off the global cooldown), 208 family (11 Shaman), 209-211 family flags, 212 maximum targets (0 every one),
# 213 damage class (1 magic), 225 school (4 fire, 8 nature, 16 frost, 28 fire, nature and frost).
#
# Targets: 1 the caster, 6 the enemy target, 16 the enemies around the chosen spot, 21 the friendly target, 22 around
# the caster (with target B 15), 87 the chosen spot.
#
# The Shaman keeps mana. Elemental's Maelstrom (95400) and Enhancement's Maelstrom Weapon (95402) are auras of stacks
# that mod-shaman fills and spends; Earth Shock, Elemental Blast and Earthquake spend Maelstrom, not mana, for
# Elemental. Damage and heals the module works out (the relays, Cloudburst, Ascendance's copies) go through spells
# with explicit zero coefficients in modules/mod-shaman's SQL; the other new spells carry their coefficients there.

$classMask = 64
$elementalCombat = 375
$enhancement = 373
$restoration = 374

# The new abilities' own family flags, word 2 (bits 0x8000-0x40000000 are free among the Shaman's spells; the core
# reads none of them). Only the spells a talent or the module's modifiers name carry one.
$flagAstralShift = 0x8000
$flagTotem = 0x10000
$flagEarthquake = 0x20000
$flagElementalBlast = 0x40000
$flagStormkeeper = 0x80000
$flagIcefury = 0x100000
$flagAscendance = 0x200000
$flagCrashLightning = 0x400000
$flagIceStrike = 0x800000
$flagSundering = 0x1000000
$flagDoomWinds = 0x2000000
$flagHealingRain = 0x4000000
$flagUnleashLife = 0x8000000
$flagWellspring = 0x10000000

# The stock spells the modifiers name: word 0 Lightning Bolt 0x1, Chain Lightning 0x2, Healing Wave 0x40, Lesser
# Healing Wave 0x80, Chain Heal 0x100, Earth Shock 0x100000, Flame Shock 0x10000000, Frost Shock 0x80000000; word 1
# Stormstrike (and its two hits) 0x10, Lava Burst 0x1000; word 2 Lava Lash 0x4, Riptide 0x10
$maskBolts = 0x3
$maskHeals = 0x1c0
$maskEarthShock = 0x100000
$maskFrostShock = 0x80000000
$maskStormstrike = 0x10
$maskLavaBurst = 0x1000
$maskLavaLash = 0x4
$maskRiptide = 0x10

# Lightning Bolt's layout for a cast nature bolt; Frost Shock's for an instant ranged hit with a snare; Thunder Clap's
# for damage around the Shaman; Shadowfury's for a spell aimed at a spot on the ground; Flash Heal's for a heal the
# Shaman casts; Holy Shock's heal for one the module hands out (instant, triggered, 100 yd); Death Coil's for damage
# the module or a coefficient sets (its speed and visual cleared); Sprint's for an instant self buff
$bolt = 49238
$frostShock = 49236
$nova = 6343
$ground = 47847
$castHeal = 48071
$healHit = 25914
$computed = 47632
$selfBuff = 2983

# Every clone made a Shaman spell: its own category and flags, no stance, mana
function Own($flag, $extra = @{}) {
    $fields = @{ 1 = 0; 12 = 0; 13 = 0; 30 = 0; 41 = 0; 208 = 11; 209 = 0; 210 = 0; 211 = $flag }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$spells = @(
    # --- Resources, procs and charges (mod-shaman fills, reads and spends them) ------------------------------------
    # Maelström (Maelstrom): Elemental's resource, 100 stacks (150 with Maelström gonflé); fades 15 s after combat
    @{ Id = 95400; Clone = 2983; Name = 'Maelström'; IconPath = 'Interface\Icons\Spell_Shaman_StaticShock'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; CantCancel = $true; MaxStacks = 150; Spellbook = $false
       Description = 'Maelström.'; AuraDescription = 'Points de Maelström : Horion de terre et Séisme en consomment 60, Explosion élémentaire 90.'; Fields = @{ 40 = 21 } },
    # Déferlante de lave (Lava Surge): the next Lava Burst instant (word 1 0x1000), its charge given back
    @{ Id = 95401; Clone = 2983; Name = 'Déferlante de lave'; IconPath = 'Interface\Icons\Spell_Shaman_LavaFlow'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Explosion de lave instantanée.'; AuraDescription = 'Votre prochaine Explosion de lave est instantanée.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 1; 122 = 0; 123 = $maskLavaBurst; 124 = 0; 208 = 11 } },
    # Arme du Maelström (Maelstrom Weapon): up to 10 stacks; each of up to 5 takes 20% off the cast time of Lightning
    # Bolt, Chain Lightning and the heals (word 0 0x1c3: mod-shaman keeps the amount at 5 stacks' at most)
    @{ Id = 95402; Clone = 2983; Name = 'Arme du Maelström'; IconPath = 'Interface\Icons\Spell_Shaman_MaelstromWeapon'; FallbackIconSpell = 51528; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Arme du Maelström.'; AuraDescription = "Chaque charge réduit de 20% le temps d'incantation de votre prochain Éclair, Chaîne d'éclairs ou sort de soins et augmente ses effets de 12%, 5 charges au plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -20; Misc = 10 })
       Fields = @{ 40 = 9; 122 = ($maskBolts -bor $maskHeals); 123 = 0; 124 = 0; 208 = 11 } },
    # Maître des éléments (Master of the Elements): the next Nature or Frost spell 20% stronger (mod-shaman)
    @{ Id = 95403; Clone = 2983; Name = 'Maître des éléments'; IconPath = 'Interface\Icons\Spell_Nature_ElementalAbsorption'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Maître des éléments.'; AuraDescription = "Votre prochain Éclair, Chaîne d'éclairs, Horion de terre, Horion de givre, Séisme ou Explosion élémentaire inflige 20% de dégâts en plus."; Fields = @{ 40 = 8 } },
    # Gardien des tempêtes (Stormkeeper): its stacks make Lightning Bolt and Chain Lightning instant (mod-shaman keeps the
    # amount at -100% whatever the stacks) and 150% stronger
    @{ Id = 95404; Clone = 2983; Name = 'Gardien des tempêtes'; IconPath = 'Interface\Icons\Spell_Nature_EyeOfTheStorm'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Gardien des tempêtes.'; AuraDescription = "Vos prochains Éclairs ou Chaînes d'éclairs sont instantanés et infligent 150% de dégâts en plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 8; 122 = $maskBolts; 123 = 0; 124 = 0; 208 = 11 } },
    # Glace furieuse (Icefury): the next 4 Frost Shocks twice as strong, 8 Maelstrom each (mod-shaman)
    @{ Id = 95405; Clone = 2983; Name = 'Glace furieuse'; IconPath = 'Interface\Icons\Spell_Frost_IceShard'; FallbackIconSpell = 49236; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 4; Spellbook = $false
       Description = 'Glace furieuse.'; AuraDescription = 'Vos prochains Horions de givre infligent 100% de dégâts en plus et vous rendent 8 points de Maelström.'; Fields = @{ 40 = 8 } },
    # Échos de la Grande fracture (Echoes of Great Sundering): the next Earthquake twice as strong (mod-shaman)
    @{ Id = 95406; Clone = 2983; Name = 'Échos de la Grande fracture'; IconPath = 'Interface\Icons\Spell_Nature_Earthquake'; FallbackIconSpell = 49231; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Échos de la Grande fracture.'; AuraDescription = 'Votre prochain Séisme inflige 100% de dégâts en plus.'; Fields = @{ 40 = 8 } },
    # Explosion élémentaire's two boons, one at random a cast: 6% spell haste or 6% spell critical strike, 10 s
    @{ Id = 95407; Clone = 2983; Name = 'Explosion élémentaire : hâte'; IconPath = 'Interface\Icons\Spell_Fire_MasterOfElements'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Hâte des sorts augmentée.'; AuraDescription = "Vitesse d'incantation augmentée de 6%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 65; TargetA = 1; Value = 6 }); Fields = @{ 40 = 1 } },
    @{ Id = 95408; Clone = 2983; Name = 'Explosion élémentaire : coup critique'; IconPath = 'Interface\Icons\Spell_Fire_MasterOfElements'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Coups critiques des sorts augmentés.'; AuraDescription = 'Chances de coup critique des sorts augmentées de 6%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 57; TargetA = 1; Value = 6 }); Fields = @{ 40 = 1 } },
    # Charges, shown as stacks: Lava Burst (Écho des éléments), Riptide (Écho de la marée)
    @{ Id = 95409; Clone = 2983; Name = "Charges d'Explosion de lave"; IconPath = 'Interface\Icons\Spell_Shaman_LavaBurst'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = "Charges d'Explosion de lave."; AuraDescription = "Charges d'Explosion de lave disponibles."; Fields = @{ 40 = 21 } },
    @{ Id = 95410; Clone = 2983; Name = 'Charges de Remous'; IconPath = 'Interface\Icons\spell_nature_riptide'; FallbackIconSpell = 61301; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Charges de Remous.'; AuraDescription = 'Charges de Remous disponibles.'; Fields = @{ 40 = 21 } },
    # Main brûlante (Hot Hand): Lava Lash (word 2 0x4) 100% stronger and 75% quicker to come back for 8 s
    @{ Id = 95411; Clone = 2983; Name = 'Main brûlante'; IconPath = 'Interface\Icons\Spell_Fire_Incinerate'; FallbackIconSpell = 60103; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Main brûlante.'; AuraDescription = 'Fouet de lave inflige 100% de dégâts en plus et se recharge 75% plus vite.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 100; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -75; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 40 = 31; 122 = 0; 123 = 0; 124 = $maskLavaLash; 125 = 0; 126 = 0; 127 = $maskLavaLash; 208 = 11 } },
    # Foudre écrasante (Crash Lightning): Stormstrike, Lava Lash and Ice Strike also strike the enemies near their target
    @{ Id = 95412; Clone = 2983; Name = 'Foudre écrasante'; IconPath = 'Interface\Icons\Spell_Shaman_ThunderStorm'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Foudre écrasante.'; AuraDescription = 'Frappe-tempête, Fouet de lave et Frappe de glace frappent aussi les ennemis proches de leur cible.'; Fields = @{ 40 = 29 } },
    # Frappe de glace's boon: the next Frost Shock twice as strong (mod-shaman)
    @{ Id = 95413; Clone = 2983; Name = 'Frappe de glace'; IconPath = 'Interface\Icons\Spell_Frost_IceShard'; FallbackIconSpell = 49236; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Horion de givre renforcé.'; AuraDescription = 'Votre prochain Horion de givre inflige 100% de dégâts en plus.'; Fields = @{ 40 = 8 } },
    # Héritage de la sorcière du givre (Legacy of the Frost Witch): 5% more physical damage for 5 s
    @{ Id = 95414; Clone = 2983; Name = 'Héritage de la sorcière du givre'; IconPath = 'Interface\Icons\Spell_Frost_FrostBrand'; FallbackIconSpell = 58796; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts physiques augmentés.'; AuraDescription = 'Dégâts physiques augmentés de 5%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 5; Misc = 1 }); Fields = @{ 40 = 28 } },
    # Libération de vie's boon: the next Healing Wave, Lesser Healing Wave or Chain Heal 35% stronger (mod-shaman)
    @{ Id = 95415; Clone = 2983; Name = 'Libération de vie'; IconPath = 'Interface\Icons\Spell_Shaman_SpectralTransformation'; FallbackIconSpell = 61301; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Soin renforcé.'; AuraDescription = 'Votre prochaine Vague de soins, Vague de soins inférieure ou Salve de guérison soigne 35% de plus.'; Fields = @{ 40 = 1 } },

    # --- Class tree -------------------------------------------------------------------------------------------------
    # Transfert astral (Astral Shift): 40% less damage for 12 s, off the global cooldown, usable while stunned
    @{ Id = 95420; Clone = $selfBuff; Name = 'Transfert astral'; IconPath = 'Interface\Icons\Spell_Shaman_AstralShift'; FallbackIconSpell = 51474; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous passez dans le plan astral : les dégâts que vous subissez sont réduits de 40% pendant 12 s. Utilisable étourdi.'
       AuraDescription = 'Dégâts subis réduits de 40%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -40; Misc = 127 })
       Fields = (Own $flagAstralShift @{ 9 = 0x8; 40 = 29; 131 = 11564; 205 = 0; 206 = 0; 225 = 8 }) },
    # Marche spirituelle (Spirit Walk): roots and snares broken and kept off, 60% speed for 8 s
    @{ Id = 95421; Clone = $selfBuff; Name = 'Marche spirituelle'; IconPath = 'Interface\Icons\Spell_Nature_SpiritWolf'; FallbackIconSpell = 2645; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask; NoEquipment = $true
       Description = 'Libère des immobilisations et des ralentissements, qui ne vous atteignent plus, et augmente votre vitesse de 60% pendant 8 s.'
       AuraDescription = 'Vitesse augmentée de 60%, insensible aux immobilisations et aux ralentissements.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 1; Value = 60 },
           @{ Index = 1; Effect = 6; Aura = 77; TargetA = 1; Misc = 7 },
           @{ Index = 2; Effect = 6; Aura = 77; TargetA = 1; Misc = 11 })
       Fields = (Own 0 @{ 40 = 31; 131 = 311; 205 = 0; 206 = 0; 225 = 8 }) },
    # Rafale de vent (Gust of Wind): Blink's leap, 20 yd forward
    @{ Id = 95422; Clone = 1953; Name = 'Rafale de vent'; IconPath = 'Interface\Icons\Spell_Nature_Cyclone'; FallbackIconSpell = 1953; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask
       Description = "Une bourrasque vous projette de 20 m vers l'avant."
       Effects = @(@{ Index = 0; Effect = 29; TargetA = 1 })
       Fields = (Own 0 @{ 89 = 55; 92 = 9; 131 = 8723; 204 = 0; 225 = 8 }) },
    # Totem de condensateur (Capacitor Totem): a dummy where the Shaman stands; mod-shaman stuns the enemies within 8 yd
    # of the spot 2 s later (95429)
    @{ Id = 95423; Clone = $selfBuff; Name = 'Totem de condensateur'; IconPath = 'Interface\Icons\Spell_Nature_Brilliance'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pose à vos pieds un totem qui se charge 2 s, puis étourdit 3 s les ennemis à 8 m.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagTotem @{ 40 = 0; 131 = 221; 205 = 133; 206 = 1000; 225 = 8 }) },
    # Lasso de foudre (Lightning Lasso): a 5 s stun and nature damage every second
    @{ Id = 95424; Clone = $frostShock; Name = 'Lasso de foudre'; IconPath = 'Interface\Icons\Spell_Lightning_LightningBolt01'; FallbackIconSpell = 49238; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask
       Description = "Enlace l'ennemi à 30 m dans un lasso de foudre : il est étourdi 5 s et subit des dégâts de Nature chaque seconde."
       AuraDescription = 'Étourdi ; subit des dégâts de Nature chaque seconde.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 12; TargetA = 6 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 150 })
       Fields = (Own 0 @{ 40 = 28; 46 = 4; 83 = 12; 84 = 0; 99 = 1000; 131 = 173; 204 = 0; 225 = 8 }) },
    # Totem de poigne de terre (Earthgrab Totem): roots on the chosen spot, 8 s within 8 yd
    @{ Id = 95425; Clone = $ground; Name = 'Totem de poigne de terre'; IconPath = 'Interface\Icons\Spell_Nature_NatureTouchDecay'; FallbackIconSpell = 2484; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask
       Description = "Fait jaillir des racines à l'endroit visé : les ennemis à 8 m sont immobilisés 8 s."
       AuraDescription = 'Immobilisé.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 26; TargetA = 16 })
       Fields = (Own $flagTotem @{ 28 = 1; 40 = 31; 83 = 7; 92 = 14; 131 = 907; 204 = 0; 205 = 133; 206 = 1000; 225 = 8 }) },
    # Guidance ancestrale (Ancestral Guidance): 10 s; mod-shaman heals the injured with a share of the Shaman's damage
    # and heals (95427)
    @{ Id = 95426; Clone = $selfBuff; Name = 'Guidance ancestrale'; IconPath = 'Interface\Icons\Spell_Nature_HealingTouch'; FallbackIconSpell = 51886; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pendant 10 s, 25% des dégâts et des soins que vous infligez soignent jusqu''à 3 alliés blessés à 40 m.'
       AuraDescription = 'Une part de vos dégâts et de vos soins soigne les alliés blessés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (Own 0 @{ 40 = 1; 131 = 3659; 205 = 0; 206 = 0; 225 = 8 }) },
    @{ Id = 95427; Clone = $healHit; Name = 'Guidance ancestrale'; IconPath = 'Interface\Icons\Spell_Nature_HealingTouch'; FallbackIconSpell = 51886; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = (Own 0 @{ 46 = 6; 131 = 58; 205 = 0; 206 = 0; 225 = 8 }) },
    # Bouclier de terre for Orbite élémentaire: the Shaman's own copy, not an elemental shield (no family flags) and not
    # single target (attributes ex 5 cleared), so it sits beside Lightning Shield and the ally's; spell_sha_earth_shield
    # heals with it (mod-shaman's SQL)
    @{ Id = 95428; Clone = 49284; Name = 'Bouclier de terre'; IconPath = 'Interface\Icons\Spell_Nature_SkinofEarth'; FallbackIconSpell = 49284; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Protège le chaman d''un bouclier de terre : chaque coup reçu le soigne, au plus une fois toutes les quelques secondes.'
       AuraDescription = 'Les coups reçus soignent, au plus une fois toutes les quelques secondes.'
       Fields = (Own 0 @{ 9 = 0; 46 = 1; 204 = 0 }) },
    @{ Id = 95429; Clone = $ground; Name = 'Totem de condensateur'; IconPath = 'Interface\Icons\Spell_Nature_Brilliance'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Étourdi.'; AuraDescription = 'Étourdi.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 12; TargetA = 16 })
       Fields = (Own $flagTotem @{ 16 = 0; 28 = 1; 40 = 27; 46 = 6; 83 = 12; 92 = 14; 131 = 11302; 204 = 0; 205 = 0; 206 = 0; 225 = 8 }) },

    # --- Élémentaire --------------------------------------------------------------------------------------------------
    # Séisme (Earthquake): 60 Maelstrom, aimed at a spot; mod-shaman shakes the ground there every second for 6 s (95435)
    @{ Id = 95430; Clone = $ground; Name = 'Séisme'; IconPath = 'Interface\Icons\Spell_Nature_Earthquake'; FallbackIconSpell = 33919; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask
       Description = "Consomme 60 points de Maelström : la terre tremble à l'endroit visé, à 40 m. Pendant 6 s, les ennemis à 8 m subissent des dégâts de Nature chaque seconde."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagEarthquake @{ 28 = 1; 40 = 0; 46 = 5; 131 = 5424; 204 = 0; 205 = 133; 206 = 1500; 213 = 1; 225 = 8 }) },
    @{ Id = 95435; Clone = $computed; Name = 'Séisme'; IconPath = 'Interface\Icons\Spell_Nature_Earthquake'; FallbackIconSpell = 33919; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Nature.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 400 })
       Fields = (Own $flagEarthquake @{ 46 = 6; 47 = 0; 131 = 0; 213 = 1; 225 = 8 }) },
    # Explosion élémentaire (Elemental Blast): 90 Maelstrom, a 2 s bolt of every element; mod-shaman grants one of its
    # boons (95407, 95408)
    @{ Id = 95431; Clone = $bolt; Name = 'Explosion élémentaire'; IconPath = 'Interface\Icons\Spell_Fire_MasterOfElements'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask
       Description = "Consomme 90 points de Maelström : une explosion des éléments inflige de lourds dégâts à la cible et vous confère 6% de hâte ou 6% de coup critique avec les sorts pendant 10 s."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1600 })
       Fields = (Own $flagElementalBlast @{ 28 = 5; 131 = 11565; 204 = 0; 225 = 28 }) },
    # Gardien des tempêtes (Stormkeeper): mod-shaman puts up its stacks (95404), 2 or 3
    @{ Id = 95432; Clone = $selfBuff; Name = 'Gardien des tempêtes'; IconPath = 'Interface\Icons\Spell_Nature_EyeOfTheStorm'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask; NoEquipment = $true
       Description = "Charge l'air autour de vous : vos 2 prochains Éclairs ou Chaînes d'éclairs sont instantanés et infligent 150% de dégâts en plus."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagStormkeeper @{ 40 = 0; 131 = 37; 205 = 133; 206 = 1500; 225 = 8 }) },
    # Glace furieuse (Icefury): a 2 s frost bolt; mod-shaman empowers the next 4 Frost Shocks (95405)
    @{ Id = 95433; Clone = $bolt; Name = 'Glace furieuse'; IconPath = 'Interface\Icons\Spell_Frost_IceShard'; FallbackIconSpell = 49236; Cost = 0; Cooldown = 25000; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask
       Description = "Projette une bourrasque de glace qui inflige des dégâts de Givre et vous rend 25 points de Maelström. Vos 4 prochains Horions de givre infligent 100% de dégâts en plus et vous rendent 8 points de Maelström."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = (Own $flagIcefury @{ 28 = 5; 131 = 13; 204 = 4; 225 = 16 }) },
    # Ascension (Ascendance, Elemental): 15 s, Lava Burst without a cooldown; mod-shaman casts one at every enemy with the
    # Shaman's Flame Shock
    @{ Id = 95434; Clone = $selfBuff; Name = 'Ascension'; IconPath = 'Interface\Icons\Spell_Fire_ElementalDevastation'; FallbackIconSpell = 60043; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $elementalCombat; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous devenez un ascendant de flammes pendant 15 s : Explosion de lave n'a plus de temps de recharge, et en vous transformant vous lancez une Explosion de lave sur chaque ennemi touché par votre Horion de flammes."
       AuraDescription = "Explosion de lave n'a plus de temps de recharge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (Own $flagAscendance @{ 40 = 8; 131 = 290; 205 = 0; 206 = 0; 225 = 4 }) },

    # --- Amélioration -------------------------------------------------------------------------------------------------
    # Foudre écrasante (Crash Lightning): nature damage within 8 yd; two enemies or more, mod-shaman puts up its boon
    # (95412) and relays the strikes (95445)
    @{ Id = 95441; Clone = $nova; Name = 'Foudre écrasante'; IconPath = 'Interface\Icons\Spell_Shaman_ThunderStorm'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 12000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask
       Description = "Électrocute les ennemis à 8 m. S'il en touche au moins deux, vos Frappe-tempête, Fouet de lave et Frappe de glace frappent aussi les ennemis proches de leur cible pendant 12 s."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 22; Value = 200 })
       Fields = (Own $flagCrashLightning @{ 40 = 0; 89 = 15; 92 = 14; 131 = 11302; 204 = 0; 212 = 0; 213 = 1; 225 = 8 }) },
    # Frappe de glace (Ice Strike): frost damage and a 6 s snare in melee; mod-shaman empowers the next Frost Shock
    @{ Id = 95442; Clone = $frostShock; Name = 'Frappe de glace'; IconPath = 'Interface\Icons\Spell_Frost_IceShard'; FallbackIconSpell = 49236; Cost = 0; Cooldown = 15000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask
       Description = "Frappe la cible d'une lame de glace : dégâts de Givre et vitesse de déplacement réduite de 50% pendant 6 s. Votre prochain Horion de givre inflige 100% de dégâts en plus."
       AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 500 },
           @{ Index = 1; Effect = 6; Aura = 33; TargetA = 6; Value = -50 })
       Fields = (Own $flagIceStrike @{ 40 = 32; 46 = 2; 83 = 0; 84 = 11; 131 = 144; 204 = 0; 225 = 16 }) },
    # Fracture (Sundering): fire damage and a 2 s stun within 8 yd
    @{ Id = 95443; Clone = $nova; Name = 'Fracture'; IconPath = 'Interface\Icons\Spell_Nature_EarthElemental_Totem'; FallbackIconSpell = 2062; Cost = 0; Cooldown = 40000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask
       Description = 'Fend la terre autour de vous : dégâts de Feu aux ennemis à 8 m, étourdis 2 s.'
       AuraDescription = 'Étourdi.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 400 },
           @{ Index = 1; Effect = 6; Aura = 12; TargetA = 22 })
       Fields = (Own $flagSundering @{ 40 = 39; 83 = 0; 84 = 12; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 131 = 5424; 204 = 0; 212 = 0; 213 = 1; 225 = 4 }) },
    # Vents funestes (Doom Winds): 20% melee haste for 8 s, off the global cooldown; mod-shaman gives a Maelstrom Weapon
    # stack on every swing while it lasts
    @{ Id = 95444; Clone = $selfBuff; Name = 'Vents funestes'; IconPath = 'Interface\Icons\Spell_Nature_Windfury'; FallbackIconSpell = 58804; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask; NoEquipment = $true
       Description = "Pendant 8 s, chacun de vos coups en mêlée vous confère une charge d'Arme du Maelström et votre vitesse d'attaque augmente de 20%."
       AuraDescription = "Vitesse d'attaque augmentée de 20% ; chaque coup confère une charge d'Arme du Maelström."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 20 })
       Fields = (Own $flagDoomWinds @{ 40 = 31; 131 = 8723; 205 = 0; 206 = 0; 225 = 8 }) },
    # The relay carries an amount already final: no damage class (no second roll, no second critical strike) and no
    # caster modifiers (attributes ex 3 0x20000000)
    @{ Id = 95445; Clone = $computed; Name = 'Foudre écrasante'; IconPath = 'Interface\Icons\Spell_Shaman_ThunderStorm'; FallbackIconSpell = 51490; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Nature.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Own 0 @{ 7 = 0x60000200; 21 = 0; 46 = 13; 47 = 0; 131 = 0; 213 = 0; 225 = 8 }) },
    # Ascension (Ascendance, Enhancement): 15 s, Stormstrike (word 1 0x10) 60% quicker to come back; mod-shaman strikes
    # the target and the enemies near it with the winds (95455)
    @{ Id = 95454; Clone = $selfBuff; Name = 'Ascension'; IconPath = 'Interface\Icons\Spell_Nature_CallStorm'; FallbackIconSpell = 17364; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $enhancement; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous devenez un ascendant de l'air pendant 15 s : des vents frappent la cible et les ennemis proches, et le temps de recharge de Frappe-tempête est réduit de 60%."
       AuraDescription = 'Temps de recharge de Frappe-tempête réduit de 60%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -60; Misc = $SPELLMOD_COOLDOWN })
       Fields = (Own $flagAscendance @{ 40 = 8; 122 = 0; 123 = $maskStormstrike; 124 = 0; 131 = 7922; 205 = 0; 206 = 0; 225 = 8 }) },
    @{ Id = 95455; Clone = $computed; Name = 'Ascension'; IconPath = 'Interface\Icons\Spell_Nature_CallStorm'; FallbackIconSpell = 17364; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Nature.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 600 })
       Fields = (Own $flagAscendance @{ 46 = 6; 47 = 0; 131 = 11302; 213 = 1; 225 = 8 }) },

    # --- Restauration -------------------------------------------------------------------------------------------------
    # Pluie guérisseuse (Healing Rain): aimed at a spot, 1.5 s; mod-shaman heals up to 6 allies within 10 yd of it every
    # 2 s for 10 s (95480)
    @{ Id = 95470; Clone = $ground; Name = 'Pluie guérisseuse'; IconPath = 'Interface\Icons\Spell_Nature_GiftoftheWaterSpirit'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 10000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask
       Description = "Fait tomber une pluie guérisseuse à l'endroit visé, à 40 m : pendant 10 s, elle soigne toutes les 2 s jusqu'à 6 alliés à 10 m."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagHealingRain @{ 28 = 16; 40 = 0; 46 = 5; 131 = 3659; 204 = 18; 205 = 133; 206 = 1500; 213 = 1; 225 = 8 }) },
    @{ Id = 95480; Clone = $healHit; Name = 'Pluie guérisseuse'; IconPath = 'Interface\Icons\Spell_Nature_GiftoftheWaterSpirit'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 250 })
       Fields = (Own $flagHealingRain @{ 46 = 6; 131 = 0; 205 = 0; 206 = 0; 225 = 8 }) },
    # Libération de vie (Unleash Life): an instant heal; mod-shaman strengthens the next heal (95415)
    @{ Id = 95471; Clone = $castHeal; Name = 'Libération de vie'; IconPath = 'Interface\Icons\Spell_Shaman_SpectralTransformation'; FallbackIconSpell = 61301; Cost = 0; Cooldown = 15000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask
       Description = 'Soigne aussitôt un allié, et votre prochaine Vague de soins, Vague de soins inférieure ou Salve de guérison soigne 35% de plus.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1100 })
       Fields = (Own $flagUnleashLife @{ 28 = 1; 131 = 58; 204 = 8; 225 = 8 }) },
    # Totem de nuage éclaté (Cloudburst Totem): 15 s; mod-shaman gathers 30% of the healing and hands it back (95481)
    @{ Id = 95472; Clone = $selfBuff; Name = 'Totem de nuage éclaté'; IconPath = 'Interface\Icons\Spell_Frost_SummonWaterElemental'; FallbackIconSpell = 58757; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = "Pendant 15 s, 30% de vos soins sont recueillis ; ils sont ensuite rendus, répartis entre jusqu'à 6 alliés blessés à 40 m."
       AuraDescription = 'Recueille 30% de vos soins.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (Own $flagTotem @{ 40 = 8; 131 = 319; 204 = 6; 205 = 133; 206 = 1000; 225 = 16 }) },
    # Totem de lien d'esprit (Spirit Link Totem): 6 s where the Shaman stands; mod-shaman evens out the health of the group
    # within 12 yd every second and puts its reduction on them (95482)
    @{ Id = 95473; Clone = $selfBuff; Name = "Totem de lien d'esprit"; IconPath = 'Interface\Icons\Spell_Shaman_SpiritLink'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pose un totem pendant 6 s : les membres du groupe à 12 m subissent 10% de dégâts en moins et leurs points de vie sont répartis également chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagTotem @{ 40 = 0; 131 = 319; 204 = 11; 205 = 133; 206 = 1000; 225 = 8 }) },
    # Totem de marée de soins (Healing Tide Totem): 10 s; mod-shaman heals the group within 40 yd every 2 s (95483)
    @{ Id = 95474; Clone = $selfBuff; Name = 'Totem de marée de soins'; IconPath = 'Interface\Icons\Spell_Nature_HealingTouch'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pose un totem qui soigne toutes les 2 s, pendant 10 s, chaque membre du groupe à 40 m.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagTotem @{ 40 = 0; 131 = 319; 204 = 18; 205 = 133; 206 = 1000; 225 = 8 }) },
    # Ascension (Ascendance, Restoration): 15 s; mod-shaman shares half of every heal among the injured (95479)
    @{ Id = 95475; Clone = $selfBuff; Name = 'Ascension'; IconPath = 'Interface\Icons\Spell_Frost_SummonWaterElemental'; FallbackIconSpell = 61301; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous devenez un ascendant de l'eau pendant 15 s : la moitié de chacun de vos soins est dupliquée et répartie entre jusqu'à 5 alliés blessés à 40 m."
       AuraDescription = 'La moitié de vos soins est dupliquée sur les alliés blessés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (Own $flagAscendance @{ 40 = 8; 131 = 3659; 205 = 0; 206 = 0; 225 = 16 }) },
    # Source de vie (Wellspring): 1.5 s; mod-shaman heals up to 6 injured allies within 30 yd (95484)
    @{ Id = 95476; Clone = $castHeal; Name = 'Source de vie'; IconPath = 'Interface\Icons\Spell_Nature_HealingWaveGreater'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask
       Description = "Une vague d'eau soigne jusqu'à 6 alliés blessés à 30 m."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagWellspring @{ 28 = 16; 46 = 1; 131 = 3659; 204 = 16; 225 = 8 }) },
    # Totem de mur de terre (Earthen Wall Totem): mod-shaman shields the group within 20 yd (95478)
    @{ Id = 95477; Clone = $selfBuff; Name = 'Totem de mur de terre'; IconPath = 'Interface\Icons\Spell_Nature_StoneSkinTotem'; FallbackIconSpell = 58753; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pose un totem qui protège les membres du groupe à 20 m : chacun reçoit un bouclier qui absorbe des dégâts selon votre puissance des sorts, pendant 15 s.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagTotem @{ 40 = 0; 131 = 319; 204 = 10; 205 = 133; 206 = 1000; 225 = 8 }) },
    @{ Id = 95478; Clone = 2983; Name = 'Mur de terre'; IconPath = 'Interface\Icons\Spell_Nature_StoneSkinTotem'; FallbackIconSpell = 58753; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_SchoolAbsorb; TargetA = 1; Value = 1; Misc = 127 }); Fields = @{ 40 = 8 } },
    @{ Id = 95479; Clone = $healHit; Name = 'Ascension'; IconPath = 'Interface\Icons\Spell_Frost_SummonWaterElemental'; FallbackIconSpell = 61301; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = (Own 0 @{ 46 = 6; 131 = 0; 205 = 0; 206 = 0; 225 = 16 }) },
    @{ Id = 95481; Clone = $healHit; Name = 'Totem de nuage éclaté'; IconPath = 'Interface\Icons\Spell_Frost_SummonWaterElemental'; FallbackIconSpell = 58757; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = (Own 0 @{ 46 = 6; 131 = 58; 205 = 0; 206 = 0; 225 = 16 }) },
    # Lien d'esprit: 10% less damage taken, put back every second while the totem stands
    @{ Id = 95482; Clone = 2983; Name = "Totem de lien d'esprit"; IconPath = 'Interface\Icons\Spell_Shaman_SpiritLink'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits.'; AuraDescription = 'Dégâts subis réduits de 10% ; points de vie partagés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -10; Misc = 127 }); Fields = @{ 40 = 39 } },
    @{ Id = 95483; Clone = $healHit; Name = 'Totem de marée de soins'; IconPath = 'Interface\Icons\Spell_Nature_HealingTouch'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 500 })
       Fields = (Own $flagTotem @{ 46 = 6; 131 = 0; 205 = 0; 206 = 0; 225 = 8 }) },
    @{ Id = 95484; Clone = $healHit; Name = 'Source de vie'; IconPath = 'Interface\Icons\Spell_Nature_HealingWaveGreater'; FallbackIconSpell = 55459; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 900 })
       Fields = (Own $flagWellspring @{ 46 = 6; 131 = 58; 205 = 0; 206 = 0; 225 = 8 }) },

    # --- Spec passives ----------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-shaman also reads them to know which one
    # is on.
    # Élémentaire: Lightning Bolt, Chain Lightning (word 0), Lava Burst (word 1) and Frost Shock deal 15% less (their
    # procs and Maelstrom's spenders carry the specialization; tuned on the combat bench), Earth Shock 150% more (it
    # spends Maelstrom, mod-shaman); Maelstrom, Lava Surge (mod-shaman)
    @{ Id = 95580; Clone = 2983; Name = 'Élémentaire'; IconPath = 'Interface\Icons\Spell_Nature_Lightning'; FallbackIconSpell = 403; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Éclair, Chaîne d'éclairs, Explosion de lave, Glace furieuse et Horion de givre vous rendent du Maelström, qu'Horion de terre, Séisme et Explosion élémentaire consomment. Les dégâts périodiques d'Horion de flammes peuvent rendre votre prochaine Explosion de lave instantanée. Horion de terre inflige 150% de dégâts en plus, et Éclair, Chaîne d'éclairs, Explosion de lave et Horion de givre 15% de plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 15; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 150; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = ($maskBolts -bor $maskFrostShock); 123 = $maskLavaBurst; 124 = 0
                   125 = $maskEarthShock; 126 = 0; 127 = 0; 208 = 11 } },
    # Amélioration: Stormstrike (word 1), Lava Lash, Crash Lightning, Ice Strike and Sundering (word 2) deal 20% less
    # (Maelstrom Weapon and the strikes' talents carry the specialization; tuned on the combat bench);
    # Chain Lightning (word 0 0x2, its pack spender) deals 100% more; Lightning Bolt, Chain Lightning and the heals cost half
    # as much mana; Maelstrom Weapon (mod-shaman)
    @{ Id = 95581; Clone = 2983; Name = 'Amélioration'; IconPath = 'Interface\Icons\Spell_Nature_LightningShield'; FallbackIconSpell = 17364; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Vos coups en mêlée chargent l'Arme du Maelström, qui rend plus rapides et plus puissants vos Éclairs, Chaînes d'éclairs et sorts de soins. Frappe-tempête, Fouet de lave, Foudre écrasante, Frappe de glace et Fracture infligent 5% de dégâts en plus, Chaîne d'éclairs inflige 100% de dégâts en plus, et Éclair, Chaîne d'éclairs et vos soins coûtent 50% de mana en moins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 5; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -50; Misc = $SPELLMOD_COST },
           @{ Index = 2; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 100; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = 0; 123 = $maskStormstrike; 124 = ($maskLavaLash -bor $flagCrashLightning -bor $flagIceStrike -bor $flagSundering)
                   125 = ($maskBolts -bor $maskHeals); 126 = 0; 127 = 0; 128 = 0x2; 129 = 0; 130 = 0; 208 = 11 } },
    # Amélioration's spells scale with its attack power (as retail's): half of it as spell power and healing power
    # (auras 237, 238). Its lightning, shocks and imbues are spells, and at 650 paragon - whose power an agility
    # fighter takes as agility and attack power - it did half of what a fire mage did. Esprit vif (Mental Quickness)
    # adds its 10-30% on top.
    @{ Id = 95583; Clone = 2983; Name = 'Puissance de la tempête'; IconPath = 'Interface\Icons\Spell_Nature_MentalQuickness'; FallbackIconSpell = 30812; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "50% de votre puissance d'attaque s'ajoute à votre puissance des sorts et à vos soins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 237; TargetA = 1; Value = 50; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 238; TargetA = 1; Value = 50 }) },
    # Restauration: healing done 15% more (aura 136), Chain Heal (word 0) and Riptide (word 2) 10% more again
    @{ Id = 95582; Clone = 2983; Name = 'Restauration'; IconPath = 'Interface\Icons\Spell_Nature_MagicImmunity'; FallbackIconSpell = 61301; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Vos soins augmentent de 15%, et Salve de guérison et Remous de 10% de plus. Remous peut avoir des charges, et vos totems de soins sauvent le groupe des pires moments."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 136; TargetA = 1; Value = 15 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 10; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 125 = 0x100; 126 = 0; 127 = $maskRiptide; 208 = 11 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-shaman reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\shaman\talentTree.json') -Family 11

return $spells
