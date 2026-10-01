# The Warlock's spell data for its retail-style talent trees (localTools/warlock/talentTree.json): the abilities its
# nodes and specializations teach, the auras its scripted talents show, the hits the module hands out, and the rank
# spells of its new talents. Behaviour lives in modules/mod-warlock; this file owns client/server spell data only. The
# stock Warlock spells it changes in place (Chaos Bolt on Soul Shards, Shadowburn and the summons without a shard
# item...) are in localTools/warlock/StockSpells.ps1.
# Ids 95600-95899: 95600-95699 talent ranks (generated from the tree), 95700-95879 abilities, auras and hits,
# 95880-95889 the specializations' passives.
#
# Clone field notes: 1 category, 7 attributes ex 3, 9 attributes ex 5 (0x8 usable while stunned), 12-13 stances, 16
# target flags (0x40 a ground target), 21 target aura state, 28 casting time index (1 instant, 16 1.5 s, 5 2 s, 14
# 3 s, 30 4.5 s), 29 cooldown, 30 category cooldown, 40 duration index (39 2 s, 27 3 s, 32 6 s, 31 8 s, 1 10 s, 29
# 12 s, 8 15 s, 387 16 s, 18 20 s, 9 30 s, 21 never), 41 power type (0 mana), 42 cost, 46 range index (1 self, 2
# melee, 4 30 yd, 5 40 yd, 6 100 yd), 47 speed (0: the hit lands with the cast), 72-73 effects 1-2, 83-85 effect
# mechanic (12 stun), 86-88 target A, 98-100 periodic interval, 122-130 the effects' class masks, 131 visual, 204 cost
# as a share of base mana, 205-206 global cooldown category and time (0 0: off the global cooldown), 208 family (5
# Warlock), 209-211 family flags, 212 maximum targets, 213 damage class (1 magic), 225 school (4 fire, 32 shadow).
#
# Targets: 1 the caster, 6 the enemy target, 87 the chosen spot.
#
# The Warlock keeps mana and its demons. Soul Shards (Fragments d'âme, 95700) are an aura of up to 5 stacks every
# specialization fills and spends; mod-warlock checks and spends them (the spenders cost no mana). The damage the
# module hands out goes through hit spells: those the Warlock casts carry a spell power coefficient in
# modules/mod-warlock's SQL, those its summoned demons cast (and the relays) an amount the module works out and an
# explicit zero coefficient.

$classMask = 256
$affliction = 355
$demonology = 354
$destruction = 593

# The new abilities' own family flags, word 2 (bits 0x10000-0x80000000 are free among the Warlock's spells; the core
# reads none of them). Only the spells a talent or the module's modifiers name carry one.
$flagMalefic = 0x10000
$flagDemonbolt = 0x20000
$flagGuldan = 0x40000
$flagDreadstalkers = 0x80000
$flagResolve = 0x100000
$flagTyrant = 0x200000
$flagDarkglare = 0x400000
$flagHavoc = 0x800000
$flagRainOfFire = 0x1000000
$flagRot = 0x2000000
$flagGrimoire = 0x4000000
$flagDemonfire = 0x8000000
$flagCataclysm = 0x10000000
$flagInfernal = 0x20000000
$flagDarkPact = 0x40000000

# The stock spells the modifiers name: word 0 Shadow Bolt 0x1, Corruption 0x2, Curse of Agony 0x400, Shadowburn 0x80;
# word 1 Incinerate 0x40, Unstable Affliction 0x100, Chaos Bolt 0x20000, Conflagrate 0x800000 (Conflagration carries
# it, so Backdraft, Fire and Brimstone, Pyroclasm and Soul Leech take it for Conflagrate)
$maskShadowBolt = 0x1
$maskShadowburn = 0x80
$maskAfflictionDots = 0x402
$maskUnstable = 0x100
$maskIncinerate = 0x40
$maskChaosBolt = 0x20000
$maskConflagrate = 0x800000

# Shadow Bolt's layout for a cast bolt; Searing Pain's for a cast spell landing with the cast; Corruption's for an
# instant spell or an aura on the enemy; Shadowfury's for a spell aimed at a spot on the ground; Hellfire's for a spell
# channelled on the Warlock; Conflagrate's for Conflagration; Death Coil's for the hits the module hands out (its speed
# and visual set per hit); Sprint's for an instant self buff
$bolt = 47809
$cast = 47815
$instant = 47813
$ground = 47847
$channel = 47823
$conflagrate = 17962
$computed = 47632
$selfBuff = 2983

# Every clone made a Warlock spell: its own category and flags, no stance, mana
function Own($flag, $extra = @{}) {
    $fields = @{ 1 = 0; 12 = 0; 13 = 0; 30 = 0; 41 = 0; 208 = 5; 209 = 0; 210 = 0; 211 = $flag }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A hit the module hands out: 100 yd, landing with its cast, magic. Never a travelling one: a guardian's delayed hit
# never landed (the Wild Imps' bolts, measured on the combat bench)
function Hit($flag, $school, $visual, $extra = @{}) {
    $fields = Own $flag @{ 46 = 6; 47 = 0; 131 = $visual; 213 = 1; 225 = $school }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A relay carries an amount already final: no damage class (no second roll, no second critical strike) and no caster
# modifiers (attributes ex 3 0x20000000)
function Relay($school) {
    return (Own 0 @{ 7 = 0x60000200; 21 = 0; 46 = 13; 47 = 0; 131 = 0; 213 = 0; 225 = $school })
}

$spells = @(
    # --- Resources, procs and charges (mod-warlock fills, reads and spends them) -----------------------------------
    # Fragments d'âme (Soul Shards): every specialization's resource, 5 at most; out of combat they settle at 3
    @{ Id = 95700; Clone = 2983; Name = "Fragments d'âme"; IconPath = 'Interface\Icons\INV_Misc_Gem_Amethyst_02'; FallbackIconSpell = 47884; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; CantCancel = $true; MaxStacks = 5; Spellbook = $false
       Description = "Fragments d'âme."; AuraDescription = "Fragments d'âme : Rapture maléfique, Main de Gul'dan, Appel des traqueffroi, Trait du chaos, Pluie de feu, Brûlure de l'ombre et d'autres sorts en consomment."; Fields = @{ 40 = 21; 208 = 5; 209 = 0 } },
    # Noyau démoniaque (Demonic Core): up to 4; the next Demonbolt (word 2 0x20000) instant (mod-warlock keeps the
    # amount at -100% whatever the stacks)
    @{ Id = 95701; Clone = 2983; Name = 'Noyau démoniaque'; IconPath = 'Interface\Icons\Spell_Fire_FelFlameRing'; FallbackIconSpell = 47245; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 4; Spellbook = $false
       Description = 'Noyau démoniaque.'; AuraDescription = 'Votre prochain Trait démoniaque est instantané.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 18; 122 = 0; 123 = 0; 124 = $flagDemonbolt; 208 = 5; 209 = 0 } },
    # Charges de Conflagration: shown as stacks
    @{ Id = 95702; Clone = 2983; Name = 'Charges de Conflagration'; IconPath = 'Interface\Icons\Spell_Fire_Fireball'; FallbackIconSpell = 17962; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Charges de Conflagration.'; AuraDescription = 'Charges de Conflagration disponibles.'; Fields = @{ 40 = 21; 208 = 5; 209 = 0 } },
    # Crescendo tourmenté: the next Malefic Rapture instant and free
    @{ Id = 95703; Clone = 2983; Name = 'Crescendo tourmenté'; IconPath = 'Interface\Icons\Spell_Shadow_UnstableAffliction_3'; FallbackIconSpell = 30108; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Crescendo tourmenté.'; AuraDescription = 'Votre prochaine Rapture maléfique est instantanée et ne coûte aucun Fragment d''âme.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 1; 122 = 0; 123 = 0; 124 = $flagMalefic; 208 = 5; 209 = 0 } },
    # Ponction d'âme (Soul Leech): the shield, its amount set by mod-warlock
    @{ Id = 95704; Clone = 2983; Name = "Ponction d'âme"; IconPath = 'Interface\Icons\Spell_Shadow_SoulLeech_3'; FallbackIconSpell = 30293; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_SchoolAbsorb; TargetA = 1; Value = 1; Misc = 127 }); Fields = @{ 40 = 9; 208 = 5; 209 = 0 } },
    # Grimoire de sacrifice: the demon's power, until a demon is summoned (mod-warlock)
    @{ Id = 95705; Clone = 2983; Name = 'Grimoire de sacrifice'; IconPath = 'Interface\Icons\Spell_Shadow_SacrificialShield'; FallbackIconSpell = 18788; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Grimoire de sacrifice.'; AuraDescription = "Vos sorts de dégâts ont 35% de chances d'infliger des dégâts d'Ombre supplémentaires. Prend fin si vous invoquez un démon."; Fields = @{ 40 = 21; 208 = 5; 209 = 0 } },
    # The enemy's marks: Éradication (Chaos Bolt), Brasier rugissant (Conflagration), Contact de l'effroi (Malefic
    # Rapture), Trépas (Demonbolt); mod-warlock reads them
    @{ Id = 95708; Clone = 2983; Name = 'Éradication'; IconPath = 'Interface\Icons\Ability_Warlock_Eradication'; FallbackIconSpell = 47195; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Éradication.'; AuraDescription = 'Subit davantage de dégâts du démoniste.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 31; 208 = 5; 209 = 0 } },
    @{ Id = 95709; Clone = 2983; Name = 'Brasier rugissant'; IconPath = 'Interface\Icons\Spell_Fire_SoulBurn'; FallbackIconSpell = 17962; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Brasier rugissant.'; AuraDescription = "L'Immolation du démoniste inflige 25% de dégâts en plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 31; 208 = 5; 209 = 0 } },
    @{ Id = 95710; Clone = 2983; Name = "Contact de l'effroi"; IconPath = 'Interface\Icons\Spell_Shadow_PainSpike'; FallbackIconSpell = 30108; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Contact de l'effroi."; AuraDescription = 'Subit davantage de dégâts périodiques du démoniste.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 32; 208 = 5; 209 = 0 } },
    @{ Id = 95711; Clone = 2983; Name = 'Trépas'; IconPath = 'Interface\Icons\Spell_Shadow_AuraOfDarkness'; FallbackIconSpell = 47867; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Trépas.'; AuraDescription = 'Explose quand l''effet prend fin.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 387; 208 = 5; 209 = 0 } },
    # Appel démoniaque: the next Call Dreadstalkers (word 2 0x80000) instant and a shard cheaper (mod-warlock)
    @{ Id = 95712; Clone = 2983; Name = 'Appel démoniaque'; IconPath = 'Interface\Icons\Spell_Shadow_DemonicEmpathy'; FallbackIconSpell = 30146; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Appel démoniaque.'; AuraDescription = "Votre prochain Appel des traqueffroi est instantané et coûte 1 Fragment d'âme de moins."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 18; 122 = 0; 123 = 0; 124 = $flagDreadstalkers; 208 = 5; 209 = 0 } },
    # Conflagration du chaos: the next Conflagration or Shadowburn a critical strike (critical chance +100, flat)
    @{ Id = 95713; Clone = 2983; Name = 'Conflagration du chaos'; IconPath = 'Interface\Icons\Ability_Warlock_ChaosBolt'; FallbackIconSpell = 50796; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Conflagration du chaos.'; AuraDescription = 'Votre prochaine Conflagration ou Brûlure de l''ombre est un coup critique.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 100; Misc = 7 })
       Fields = @{ 40 = 8; 122 = $maskShadowburn; 123 = $maskConflagrate; 124 = 0; 208 = 5; 209 = 0 } },
    # Pacte noir's shield, its amount set by mod-warlock
    @{ Id = 95714; Clone = 2983; Name = 'Pacte noir'; IconPath = 'Interface\Icons\Spell_Shadow_DarkRitual'; FallbackIconSpell = 18220; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_SchoolAbsorb; TargetA = 1; Value = 1; Misc = 127 }); Fields = @{ 40 = 18; 208 = 5; 209 = 0 } },
    # Puissance démoniaque: the Demonic Tyrant's empowerment, shown on the Warlock while it stands (mod-warlock)
    @{ Id = 95759; Clone = 2983; Name = 'Puissance démoniaque'; IconPath = 'Interface\Icons\Ability_Warlock_DemonicPower'; FallbackIconSpell = 47193; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Puissance démoniaque.'; AuraDescription = 'Vos démons infligent 15% de dégâts en plus.'; Fields = @{ 40 = 8; 208 = 5; 209 = 0 } },

    # --- Class tree -------------------------------------------------------------------------------------------------
    # Résolution inflexible (Unending Resolve): 40% less damage for 8 s, off the global cooldown, usable while stunned
    @{ Id = 95720; Clone = $selfBuff; Name = 'Résolution inflexible'; IconPath = 'Interface\Icons\Spell_Shadow_DemonicTactics'; FallbackIconSpell = 30242; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask; NoEquipment = $true
       Description = 'Durcit votre peau : les dégâts que vous subissez sont réduits de 40% pendant 8 s. Utilisable étourdi.'
       AuraDescription = 'Dégâts subis réduits de 40%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -40; Misc = 127 })
       Fields = (Own $flagResolve @{ 9 = 0x8; 40 = 31; 131 = 13422; 205 = 0; 206 = 0; 225 = 32 }) },
    # Pacte noir (Dark Pact): mod-warlock takes 20% of the current health and shields 200% of it (95714)
    @{ Id = 95721; Clone = $selfBuff; Name = 'Pacte noir'; IconPath = 'Interface\Icons\Spell_Shadow_DarkRitual'; FallbackIconSpell = 18220; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $affliction; ClassMask = $classMask; NoEquipment = $true
       Description = 'Sacrifie 20% de vos points de vie actuels : un bouclier vous protège pendant 20 s et absorbe 200% de la vie sacrifiée. Utilisable étourdi.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagDarkPact @{ 9 = 0x8; 40 = 0; 131 = 969; 205 = 0; 206 = 0; 225 = 32 }) },
    # Ruée ardente (Burning Rush): 50% speed until cancelled; mod-warlock takes 4% of the health a second, ends it under
    # 10% and on a second cast
    @{ Id = 95722; Clone = $selfBuff; Name = 'Ruée ardente'; IconPath = 'Interface\Icons\Spell_Fire_BurningSpeed'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous vous entourez de flammes : votre vitesse de déplacement augmente de 50%, mais vous perdez 4% de vos points de vie chaque seconde. Relancez-la pour l'éteindre ; elle s'éteint d'elle-même sous 10% de points de vie."
       AuraDescription = 'Vitesse de déplacement augmentée de 50% ; perd 4% de ses points de vie chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 1; Value = 50 })
       Fields = (Own 0 @{ 40 = 21; 131 = 5423; 205 = 133; 206 = 1000; 225 = 4 }) },
    # Grimoire de sacrifice: mod-warlock dismisses the demon and puts up its power (95705)
    @{ Id = 95723; Clone = $selfBuff; Name = 'Grimoire de sacrifice'; IconPath = 'Interface\Icons\Spell_Shadow_SacrificialShield'; FallbackIconSpell = 18788; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask; NoEquipment = $true
       Description = "Sacrifie votre démon : pendant 1 h, ou jusqu'à ce que vous invoquiez un démon, vos sorts de dégâts ont 35% de chances d'infliger des dégâts d'Ombre supplémentaires."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own 0 @{ 40 = 0; 131 = 969; 205 = 133; 206 = 1500; 225 = 32 }) },
    @{ Id = 95724; Clone = $computed; Name = 'Sacrifice démoniaque'; IconPath = 'Interface\Icons\Spell_Shadow_SacrificialShield'; FallbackIconSpell = 18788; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 100 })
       Fields = (Hit 0 32 7682) },

    # --- Affliction ---------------------------------------------------------------------------------------------------
    # Récolte sinistre's hit
    @{ Id = 95729; Clone = $computed; Name = 'Récolte sinistre'; IconPath = 'Interface\Icons\Spell_Shadow_SoulLeech_1'; FallbackIconSpell = 47855; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 120 })
       Fields = (Hit 0 32 7682) },
    # Rapture maléfique (Malefic Rapture): 1 shard, 1.5 s; mod-warlock strikes every enemy with the Warlock's damage over
    # time effects within 40 yd (95735), once for each of them
    @{ Id = 95730; Clone = $cast; Name = 'Rapture maléfique'; IconPath = 'Interface\Icons\Ability_Warlock_EverlastingAffliction'; FallbackIconSpell = 47201; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $affliction; ClassMask = $classMask
       Description = "Consomme 1 Fragment d'âme : la douleur de vos afflictions se déchaîne et frappe de dégâts d'Ombre chaque ennemi à 40 m affligé par vos effets de dégâts sur la durée, d'autant plus fort qu'il en porte."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own $flagMalefic @{ 28 = 16; 46 = 5; 131 = 64; 204 = 0; 225 = 32 }) },
    # Pourriture d'âme (Soul Rot): a 1.5 s damage over time; mod-warlock casts it on 3 enemies near the target too and
    # gives a shard
    @{ Id = 95731; Clone = $instant; Name = "Pourriture d'âme"; IconPath = 'Interface\Icons\Spell_Shadow_LifeDrain'; FallbackIconSpell = 47857; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $affliction; ClassMask = $classMask
       Description = "Flétrit la cible et jusqu'à 3 ennemis proches : des dégâts d'Ombre toutes les 2 s pendant 8 s. Vous rend 1 Fragment d'âme."
       AuraDescription = "Subit des dégâts d'Ombre toutes les 2 s."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 220 })
       Fields = (Own $flagRot @{ 28 = 16; 40 = 31; 98 = 2000; 131 = 8629; 204 = 6; 225 = 32 }) },
    # Singularité fantôme (Phantom Singularity): 16 s on the target; mod-warlock strikes the enemies within 8 yd of it
    # every 2 s (95736) and heals the Warlock with a share
    @{ Id = 95732; Clone = $instant; Name = 'Singularité fantôme'; IconPath = 'Interface\Icons\Spell_Shadow_Shadesofdarkness'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $affliction; ClassMask = $classMask
       Description = "Place sur la cible une singularité : toutes les 2 s pendant 16 s, des dégâts d'Ombre frappent les ennemis à 8 m d'elle et vous soignent d'une part des dégâts."
       AuraDescription = "Une singularité fantôme frappe les ennemis proches."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 })
       Fields = (Own 0 @{ 40 = 387; 131 = 10731; 204 = 5; 225 = 32 }) },
    # Souillure infâme (Vile Taint): 1 shard, 1.5 s, aimed at a spot; mod-warlock puts Curse of Agony and its taint
    # (95737) on the enemies within 8 yd
    @{ Id = 95733; Clone = $ground; Name = 'Souillure infâme'; IconPath = 'Interface\Icons\Spell_Shadow_PlagueCloud'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 25000; Level = 1; Spellbook = $true; SkillLine = $affliction; ClassMask = $classMask
       Description = "Consomme 1 Fragment d'âme : souille l'endroit visé, à 40 m ; les ennemis à 8 m subissent votre Malédiction d'agonie et des dégâts d'Ombre toutes les 2 s pendant 10 s."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own 0 @{ 28 = 16; 40 = 0; 46 = 5; 131 = 7732; 204 = 0; 205 = 133; 206 = 1500; 213 = 1; 225 = 32 }) },
    # Invocation d'un œil noir (Summon Darkglare): mod-warlock calls the eye for 20 s and lengthens the damage over time
    @{ Id = 95734; Clone = $selfBuff; Name = "Invocation d'un œil noir"; IconPath = 'Interface\Icons\Spell_Shadow_EvilEye'; FallbackIconSpell = 126; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $affliction; ClassMask = $classMask; NoEquipment = $true
       Description = "Invoque un Œil noir pendant 20 s : il prolonge de 8 s vos effets de dégâts sur la durée sur les ennemis proches, puis frappe votre cible d'un rayon d'Ombre toutes les 2 s, d'autant plus fort qu'elle porte de vos effets."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagDarkglare @{ 40 = 0; 131 = 7313; 204 = 0; 205 = 133; 206 = 1500; 225 = 32 }) },
    @{ Id = 95735; Clone = $computed; Name = 'Rapture maléfique'; IconPath = 'Interface\Icons\Ability_Warlock_EverlastingAffliction'; FallbackIconSpell = 47201; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 110 })
       Fields = (Hit $flagMalefic 32 7682) },
    @{ Id = 95736; Clone = $computed; Name = 'Singularité fantôme'; IconPath = 'Interface\Icons\Spell_Shadow_Shadesofdarkness'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 140 })
       Fields = (Hit 0 32 7682) },
    @{ Id = 95737; Clone = $instant; Name = 'Souillure infâme'; IconPath = 'Interface\Icons\Spell_Shadow_PlagueCloud'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre toutes les 2 s."; AuraDescription = "Subit des dégâts d'Ombre toutes les 2 s."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 120 })
       Fields = (Own $flagRot @{ 40 = 1; 46 = 6; 98 = 2000; 131 = 0; 204 = 0; 205 = 0; 206 = 0; 225 = 32 }) },
    # The Darkglare's Eye Beam: an amount the module works out
    @{ Id = 95738; Clone = $computed; Name = 'Rayon oculaire'; IconPath = 'Interface\Icons\Spell_Shadow_EvilEye'; FallbackIconSpell = 126; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 32 64) },
    @{ Id = 95739; Clone = $computed; Name = 'Floraison funeste'; IconPath = 'Interface\Icons\Spell_Shadow_CorpseExplode'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 160 })
       Fields = (Hit 0 32 7682) },

    # --- Démonologie --------------------------------------------------------------------------------------------------
    # Main de Gul'dan (Hand of Gul'dan): 1-3 shards, 1.5 s; mod-warlock strikes the target and the enemies within 8 yd
    # (95750, stronger a shard) and calls a Wild Imp a shard
    @{ Id = 95740; Clone = $cast; Name = "Main de Gul'dan"; IconPath = 'Interface\Icons\Spell_Fire_FelFlameStrike'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask
       Description = "Consomme jusqu'à 3 Fragments d'âme : un météore démoniaque frappe la cible et les ennemis à 8 m de dégâts d'Ombre, d'autant plus forts qu'il a consommé de fragments, et libère un Diablotin sauvage par fragment."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own $flagGuldan @{ 28 = 16; 46 = 5; 131 = 7732; 204 = 0; 225 = 32 }) },
    # Trait démoniaque (Demonbolt): 4.5 s, instant on a Demonic Core; mod-warlock gives 2 shards
    @{ Id = 95741; Clone = $bolt; Name = 'Trait démoniaque'; IconPath = 'Interface\Icons\Spell_Fire_FelPyroblast'; FallbackIconSpell = 47809; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask
       Description = "Envoie un trait démoniaque qui inflige de lourds dégâts d'Ombre à la cible et vous rend 2 Fragments d'âme. Instantané avec une charge de Noyau démoniaque."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = (Own $flagDemonbolt @{ 28 = 30; 46 = 5; 131 = 64; 204 = 4; 225 = 32 }) },
    # Appel des traqueffroi (Call Dreadstalkers): 2 shards, 2 s; mod-warlock calls the two hounds for 12 s
    @{ Id = 95742; Clone = $cast; Name = 'Appel des traqueffroi'; IconPath = 'Interface\Icons\Spell_Shadow_SummonFelHunter'; FallbackIconSpell = 691; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask
       Description = "Consomme 2 Fragments d'âme : deux traqueffroi bondissent sur la cible, la mordent et combattent à vos côtés pendant 12 s. Chacun vous confère une charge de Noyau démoniaque."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own $flagDreadstalkers @{ 28 = 5; 46 = 5; 131 = 7313; 204 = 0; 225 = 32 }) },
    # Implosion: mod-warlock makes every Wild Imp explode on the target (95753)
    @{ Id = 95743; Clone = $instant; Name = 'Implosion'; IconPath = 'Interface\Icons\Spell_Fire_FelFireNova'; FallbackIconSpell = 50589; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask
       Description = "Fait exploser vos Diablotins sauvages : chacun inflige des dégâts d'Ombre à la cible et aux ennemis à 8 m d'elle."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own 0 @{ 40 = 0; 46 = 5; 131 = 0; 204 = 2; 225 = 32 }) },
    # Siphon de puissance (Power Siphon): mod-warlock sacrifices up to 2 Wild Imps for as many Demonic Cores
    @{ Id = 95744; Clone = $selfBuff; Name = 'Siphon de puissance'; IconPath = 'Interface\Icons\Spell_Shadow_SiphonMana'; FallbackIconSpell = 5138; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask; NoEquipment = $true
       Description = "Sacrifie jusqu'à 2 Diablotins sauvages : chacun vous confère une charge de Noyau démoniaque."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own 0 @{ 40 = 0; 131 = 13422; 204 = 0; 205 = 133; 206 = 1500; 225 = 32 }) },
    # Bombardiers de bilefléau (Bilescourge Bombers): 2 shards, aimed at a spot; mod-warlock bombs it every second for
    # 6 s (95754)
    @{ Id = 95745; Clone = $ground; Name = 'Bombardiers de bilefléau'; IconPath = 'Interface\Icons\Spell_Fire_FelRainOfFire'; FallbackIconSpell = 47820; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask
       Description = "Consomme 2 Fragments d'âme : des bombardiers survolent l'endroit visé, à 40 m, pendant 6 s ; chaque seconde, des dégâts d'Ombre frappent les ennemis à 8 m."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own 0 @{ 28 = 1; 40 = 0; 46 = 5; 131 = 10379; 204 = 0; 205 = 133; 206 = 1500; 213 = 1; 225 = 32 }) },
    # Force démoniaque (Demonic Strength): mod-warlock has the Felguard whirl for 5 s (95755)
    @{ Id = 95746; Clone = $selfBuff; Name = 'Force démoniaque'; IconPath = 'Interface\Icons\Spell_Shadow_UnholyStrength'; FallbackIconSpell = 30146; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask; NoEquipment = $true
       Description = "Votre gangregarde déchaîne une Tempête gangrenée : pendant 5 s, il frappe chaque seconde les ennemis à 8 m de lui."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own 0 @{ 40 = 0; 131 = 13422; 204 = 0; 205 = 133; 206 = 1500; 225 = 32 }) },
    # Grimoire : gangregarde (Grimoire: Felguard): 1 shard; mod-warlock calls a second Felguard for 17 s
    @{ Id = 95747; Clone = $instant; Name = 'Grimoire : gangregarde'; IconPath = 'Interface\Icons\Spell_Shadow_SummonFelGuard'; FallbackIconSpell = 30146; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask
       Description = "Consomme 1 Fragment d'âme : invoque un second gangregarde pendant 17 s ; il attaque votre cible et la frappe de ses Frappes de la Légion."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own $flagGrimoire @{ 40 = 0; 46 = 5; 131 = 8360; 204 = 0; 225 = 32 }) },
    # Invocation d'un tyran démoniaque (Summon Demonic Tyrant): 2 s; mod-warlock calls the tyrant for 15 s
    @{ Id = 95748; Clone = $selfBuff; Name = "Invocation d'un tyran démoniaque"; IconPath = 'Interface\Icons\Ability_Warlock_DemonicPower'; FallbackIconSpell = 18540; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $demonology; ClassMask = $classMask; NoEquipment = $true
       Description = "Invoque un tyran démoniaque pendant 15 s : vos démons restent 15 s de plus et infligent 15% de dégâts en plus tant qu'il est là, et il frappe votre cible de son Feu démoniaque."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagTyrant @{ 28 = 5; 40 = 0; 131 = 8360; 204 = 0; 205 = 133; 206 = 1500; 225 = 32 }) },
    @{ Id = 95750; Clone = $computed; Name = "Main de Gul'dan"; IconPath = 'Interface\Icons\Spell_Fire_FelFlameStrike'; FallbackIconSpell = 47836; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 150 })
       Fields = (Hit $flagGuldan 32 7682) },
    # What the summoned demons cast: amounts the module works out
    @{ Id = 95751; Clone = $computed; Name = 'Éclair gangrené'; IconPath = 'Interface\Icons\Spell_Fire_FelFlameBolt'; FallbackIconSpell = 47964; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 4 67) },
    @{ Id = 95752; Clone = $computed; Name = "Morsure de l'effroi"; IconPath = 'Interface\Icons\Spell_Shadow_SummonFelHunter'; FallbackIconSpell = 54053; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 32 11837) },
    @{ Id = 95753; Clone = $computed; Name = 'Implosion'; IconPath = 'Interface\Icons\Spell_Fire_FelFireNova'; FallbackIconSpell = 50589; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 160 })
       Fields = (Hit 0 32 7682) },
    @{ Id = 95754; Clone = $computed; Name = 'Bombardiers de bilefléau'; IconPath = 'Interface\Icons\Spell_Fire_FelRainOfFire'; FallbackIconSpell = 47820; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 90 })
       Fields = (Hit 0 32 10045) },
    @{ Id = 95755; Clone = $computed; Name = 'Tempête gangrenée'; IconPath = 'Interface\Icons\Spell_Shadow_UnholyStrength'; FallbackIconSpell = 50581; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 32 7684) },
    @{ Id = 95756; Clone = $computed; Name = 'Feu démoniaque'; IconPath = 'Interface\Icons\Ability_Warlock_DemonicPower'; FallbackIconSpell = 47825; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 4 2253) },
    @{ Id = 95757; Clone = $computed; Name = 'Frappe de la Légion'; IconPath = 'Interface\Icons\Spell_Shadow_SummonFelGuard'; FallbackIconSpell = 50581; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 32 7684) },
    @{ Id = 95758; Clone = $computed; Name = 'Trépas'; IconPath = 'Interface\Icons\Spell_Shadow_AuraOfDarkness'; FallbackIconSpell = 47867; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 500 })
       Fields = (Hit 0 32 7682) },

    # --- Destruction --------------------------------------------------------------------------------------------------
    # Conflagration: Conflagrate's flags (word 1 0x800000) without its need of Immolate; 2 charges a 13 s recharge
    # (mod-warlock), half a shard
    @{ Id = 95760; Clone = $conflagrate; Name = 'Conflagration'; IconPath = 'Interface\Icons\Spell_Fire_Fireball'; FallbackIconSpell = 17962; Cost = 0; Cooldown = 13000; Level = 1; Spellbook = $true; SkillLine = $destruction; ClassMask = $classMask
       Description = "Embrase la cible : dégâts de Feu, et vous rend un demi-Fragment d'âme. 2 charges."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 750 })
       Fields = @{ 1 = 0; 12 = 0; 13 = 0; 21 = 0; 30 = 0; 41 = 0; 46 = 5; 204 = 8; 208 = 5; 209 = 0; 210 = $maskConflagrate; 211 = 0; 225 = 4 } },
    # Pluie de feu (Rain of Fire): 3 shards, aimed at a spot; mod-warlock burns it every second for 8 s (95766)
    @{ Id = 95761; Clone = $ground; Name = 'Pluie de feu'; IconPath = 'Interface\Icons\Spell_Shadow_RainOfFire'; FallbackIconSpell = 47820; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $destruction; ClassMask = $classMask
       Description = "Consomme 3 Fragments d'âme : une pluie de feu s'abat sur l'endroit visé, à 40 m ; pendant 8 s, les ennemis à 8 m subissent des dégâts de Feu chaque seconde."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagRainOfFire @{ 28 = 1; 40 = 0; 46 = 5; 131 = 10379; 204 = 0; 205 = 133; 206 = 1500; 213 = 1; 225 = 4 }) },
    # Ravage (Havoc): 12 s on an enemy; mod-warlock copies the single-target Destruction spells cast at another target
    # onto it (95768)
    @{ Id = 95762; Clone = $instant; Name = 'Ravage'; IconPath = 'Interface\Icons\Spell_Fire_FelFireward'; FallbackIconSpell = 47867; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $destruction; ClassMask = $classMask
       Description = 'Marque un ennemi pendant 12 s : vos sorts de Destruction à cible unique lancés sur une autre cible le frappent aussi.'
       AuraDescription = 'Subit aussi les sorts de Destruction du démoniste lancés sur une autre cible.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 })
       Fields = (Own $flagHavoc @{ 40 = 29; 46 = 5; 131 = 4859; 204 = 4; 225 = 4 }) },
    # Canalisation de feu démoniaque (Channel Demonfire): 3 s channelled; mod-warlock throws a bolt every 0.25 s at an
    # enemy with the Warlock's Immolate (95767)
    @{ Id = 95763; Clone = $channel; Name = 'Canalisation de feu démoniaque'; IconPath = 'Interface\Icons\Spell_Fire_FelImmolation'; FallbackIconSpell = 47823; Cost = 0; Cooldown = 25000; Level = 1; Spellbook = $true; SkillLine = $destruction; ClassMask = $classMask
       Description = "Canalisé 3 s : des éclairs de feu démoniaque frappent au hasard les ennemis à 40 m affligés par votre Immolation."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (Own $flagDemonfire @{ 40 = 27; 131 = 5423; 204 = 8; 225 = 4 }) },
    # Cataclysme (Cataclysm): 2 s, aimed at a spot; mod-warlock strikes the enemies within 8 yd (95769) and burns them
    # with Immolate
    @{ Id = 95764; Clone = $ground; Name = 'Cataclysme'; IconPath = 'Interface\Icons\Spell_Fire_Felcano'; FallbackIconSpell = 47820; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $destruction; ClassMask = $classMask
       Description = "Invoque un cataclysme à l'endroit visé, à 40 m : dégâts de Feu aux ennemis à 8 m, qui subissent votre Immolation."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagCataclysm @{ 28 = 5; 40 = 0; 46 = 5; 131 = 7732; 204 = 8; 205 = 133; 206 = 1500; 213 = 1; 225 = 4 }) },
    # Invocation d'infernal (Summon Infernal): aimed at a spot; mod-warlock drops the infernal there for 30 s (95771
    # on landing)
    @{ Id = 95765; Clone = $ground; Name = "Invocation d'infernal"; IconPath = 'Interface\Icons\Spell_Shadow_SummonInfernal'; FallbackIconSpell = 1122; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $destruction; ClassMask = $classMask
       Description = "Un infernal s'abat à l'endroit visé, à 30 m : dégâts de Feu et étourdissement de 2 s aux ennemis à 8 m, puis il combat à vos côtés pendant 30 s ; son Immolation vous rend des dixièmes de Fragment d'âme."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagInfernal @{ 28 = 1; 40 = 0; 46 = 4; 131 = 4859; 204 = 0; 205 = 133; 206 = 1500; 213 = 1; 225 = 4 }) },
    @{ Id = 95766; Clone = $computed; Name = 'Pluie de feu'; IconPath = 'Interface\Icons\Spell_Shadow_RainOfFire'; FallbackIconSpell = 47820; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 110 })
       Fields = (Hit $flagRainOfFire 4 10045) },
    @{ Id = 95767; Clone = $computed; Name = 'Feu démoniaque'; IconPath = 'Interface\Icons\Spell_Fire_FelImmolation'; FallbackIconSpell = 47823; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 100 })
       Fields = (Hit $flagDemonfire 4 7675) },
    @{ Id = 95768; Clone = $computed; Name = 'Ravage'; IconPath = 'Interface\Icons\Spell_Fire_FelFireward'; FallbackIconSpell = 47867; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Relay 4) },
    @{ Id = 95769; Clone = $computed; Name = 'Cataclysme'; IconPath = 'Interface\Icons\Spell_Fire_Felcano'; FallbackIconSpell = 47820; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 650 })
       Fields = (Hit $flagCataclysm 4 0) },
    @{ Id = 95770; Clone = $computed; Name = 'Immolation'; IconPath = 'Interface\Icons\Spell_Fire_Immolation'; FallbackIconSpell = 47823; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit 0 4 781) },
    @{ Id = 95771; Clone = $computed; Name = 'Impact infernal'; IconPath = 'Interface\Icons\Spell_Shadow_SummonInfernal'; FallbackIconSpell = 1122; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu ; étourdi.'; AuraDescription = 'Étourdi.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 400 },
           @{ Index = 1; Effect = 6; Aura = 12; TargetA = 6 })
       Fields = (Hit $flagInfernal 4 2816 @{ 40 = 39; 84 = 12 }) },
    @{ Id = 95772; Clone = $computed; Name = 'Combustion interne'; IconPath = 'Interface\Icons\Spell_Fire_Incinerate'; FallbackIconSpell = 47811; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Relay 4) },

    # --- Spec passives ----------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-warlock also reads them to know which one
    # is on. The modifiers are the bench's tuning knobs (README.md).
    # Affliction: Curse of Agony, Corruption, Unstable Affliction (words 0 and 1), Soul Rot and Vile Taint's taint
    # (word 2) deal more over time; Soul Shards from Curse of Agony (mod-warlock)
    @{ Id = 95880; Clone = 2983; Name = 'Affliction'; IconPath = 'Interface\Icons\Spell_Shadow_DeathCoil'; FallbackIconSpell = 172; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Les dégâts de votre Malédiction d'agonie peuvent vous rendre des Fragments d'âme, que Rapture maléfique et Graine de corruption consomment. Graine de corruption répand votre Corruption sur les ennemis qu'elle touche. Vos effets d'Affliction infligent 20% de dégâts périodiques en plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 20; Misc = 22 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 0; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = $maskAfflictionDots; 123 = $maskUnstable; 124 = $flagRot
                   125 = 0; 126 = 0; 127 = $flagMalefic; 208 = 5 } },
    # Démonologie: Shadow Bolt (word 0) gives a shard and Demonbolt two; Hand of Gul'dan, Demonbolt (word 2) tuned here
    @{ Id = 95881; Clone = 2983; Name = 'Démonologie'; IconPath = 'Interface\Icons\Spell_Shadow_Metamorphosis'; FallbackIconSpell = 30146; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Trait de l'ombre vous rend un Fragment d'âme et Trait démoniaque deux ; Main de Gul'dan, Appel des traqueffroi et Grimoire : gangregarde les consomment pour appeler vos démons. Vos Diablotins sauvages peuvent vous conférer des charges de Noyau démoniaque."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 0; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 0; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = $maskShadowBolt; 123 = 0; 124 = 0
                   125 = 0; 126 = 0; 127 = ($flagDemonbolt -bor $flagGuldan); 208 = 5 } },
    # Destruction: Chaos Bolt and Incinerate (word 1) deal 10% less (the shards' spenders and the infernal carry the
    # specialization; tuned on the combat bench), Conflagration (word 1 0x800000) tuned here too; Immolate, Incinerate,
    # Conflagration and the infernal give shards (mod-warlock)
    @{ Id = 95882; Clone = 2983; Name = 'Destruction'; IconPath = 'Interface\Icons\Spell_Shadow_RainOfFire'; FallbackIconSpell = 47811; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Les dégâts de votre Immolation, Incinérer et Conflagration vous rendent des fractions de Fragment d'âme, que Trait du chaos, Pluie de feu et Brûlure de l'ombre consomment. Trait du chaos est toujours un coup critique ; Trait du chaos et Incinérer infligent 10% de dégâts en moins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -10; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 0; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = 0; 123 = ($maskChaosBolt -bor $maskIncinerate); 124 = 0
                   125 = 0; 126 = $maskConflagrate; 127 = 0; 208 = 5 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-warlock reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\warlock\talentTree.json') -Family 5

return $spells
