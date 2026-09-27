# The Paladin's spell data for its retail-style talent trees (localTools/paladin/talentTree.json): the abilities its
# nodes and specializations teach, the auras its scripted talents show, and the rank spells of its new talents.
# Behaviour lives in modules/mod-paladin; this file owns client/server spell data only.
# Ids 92800-93099: 92800-92899 talent ranks (generated from the tree), 92900-92999 abilities and auras, 93000-93009
# the specializations' passives.
#
# Clone field notes: 1 category, 3 mechanic, 9 attributes ex 5, 17 target creature type, 28 casting time index
# (1 instant), 40 duration index (1 10 s, 3 1 min, 8 15 s, 9 30 s, 18 20 s, 21 never, 27 3 s, 31 8 s, 32 6 s, 35 4 s),
# 41 power type (0 mana), 42 cost, 46 range index (1 self, 4 30 yd, 5 40 yd), 49 stack amount, 80-82 base points, 86-88
# target A, 89-91 target B (15 enemies around the source), 92-94 radius index (13 10 yd, 14 8 yd, 32 12 yd), 98-100
# periodic interval, 122-130 the effects' class masks, 131 visual, 204 mana cost percentage, 205-206 global cooldown
# category and time (0 0: off the global cooldown), 208 family (10 Paladin), 209-211 family flags, 225 school (1
# physical, 2 holy), 226 rune cost (cleared on Death Knight clones).
#
# Targets: 1 the caster, 6 the enemy target, 21 a friendly target, 22 around the caster (with target B 15), 104 a cone
# in front. Weapon strikes keep Crusader Strike's layout (121 normalized weapon damage plus the flat bonus, 31 the
# weapon percentage applied to both).
#
# Holy Power (Puissance sacrée) is an aura of stacks (92900) that mod-paladin keeps: its builders add to it, and its
# finishers (Word of Glory, Light of Dawn, Shield of the Righteous, Templar's Verdict, Divine Storm) check and spend 3.
# The finishers cost no mana.

$classMask = 2
$holy = 594
$protection = 267
$retribution = 184

# The new abilities' own family flags, word 2 (bits 0x200-0x40000000 are free among the Paladin's spells, 0x4000 left
# out: spell_warrior.cpp reads it as Avenger's Shield). Words 0 and 1 stay clear: the core reads seals, hands and
# auras from them (SpellInfo::GetSpellSpecific), and no WotLK talent modifier reaches the new spells by accident.
$flagWordOfGlory = 0x200
$flagLightOfDawn = 0x400
$flagShieldOfTheRighteous = 0x800
$flagTemplarsVerdict = 0x1000
$flagBladeOfJustice = 0x2000
$flagWakeOfAshes = 0x8000
$flagDivineToll = 0x10000
$flagBlindingLight = 0x20000
$flagEyeOfTyr = 0x40000
$flagArdentDefender = 0x80000
$flagGuardian = 0x100000
$flagFinalReckoning = 0x200000
$flagExecutionSentence = 0x400000
$flagCrusade = 0x800000
$flagAvengingCrusader = 0x1000000
$flagBeaconOfVirtue = 0x2000000
$flagShieldOfVengeance = 0x4000000
$flagBastion = 0x8000000
$flagGlimmer = 0x10000000
$flagExpurgation = 0x20000000
$flagBeaconOfFaith = 0x40000000

$spells = @(
    # --- Holy Power -----------------------------------------------------------------------------------------------
    @{ Id = 92900; Clone = 2983; Name = 'Puissance sacrée'; IconPath = 'Interface\Icons\Spell_Holy_Power'; FallbackIconSpell = 20216; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Puissance sacrée.'; AuraDescription = 'Charges de Puissance sacrée : Mot de gloire et vos techniques de finition en consomment 3.'
       Fields = @{ 40 = 21 } },
    # Mot de gloire (Word of Glory): every specialization's heal, 3 Holy Power, instant, 40 yd
    @{ Id = 92901; Clone = 48785; Name = 'Mot de gloire'; IconPath = 'Interface\Icons\Spell_Holy_Heal'; FallbackIconSpell = 48785; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = 'Consomme 3 charges de Puissance sacrée pour soigner instantanément la cible alliée.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 3500 })
       Fields = @{ 1 = 0; 28 = 1; 46 = 5; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagWordOfGlory } },
    # Volonté divine (Divine Purpose): the next finisher is free
    @{ Id = 92902; Clone = 2983; Name = 'Volonté divine'; IconPath = 'Interface\Icons\Spell_Holy_DivinePurpose'; FallbackIconSpell = 20216; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochaine technique de Puissance sacrée est gratuite.'; AuraDescription = 'Votre prochaine technique de Puissance sacrée ne coûte rien.'
       Fields = @{ 40 = 1 } },

    # --- Class tree -----------------------------------------------------------------------------------------------
    # Lumière aveuglante (Blinding Light): Repentance on every enemy within 10 yd, 6 s, broken by damage, every minute
    @{ Id = 92903; Clone = 20066; Name = 'Lumière aveuglante'; IconPath = 'Interface\Icons\Spell_Holy_Dizzy'; FallbackIconSpell = 20066; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Un éclat de lumière désoriente les ennemis à 10 m pendant 6 s. Tout dégât les en sort.'
       AuraDescription = 'Désorienté.'
       Fields = @{ 1 = 0; 17 = 0; 28 = 1; 40 = 32; 46 = 1; 86 = 22; 89 = 15; 92 = 13; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagBlindingLight } },
    # Glas divin (Divine Toll): a 30 yd dummy; mod-paladin strikes (92905) the target and up to 4 enemies near it, a
    # Holy Power each, and in Holy heals (92906) the 5 most injured allies
    @{ Id = 92904; Clone = 49909; Name = 'Glas divin'; IconPath = 'Interface\Icons\Spell_Holy_SearingLight'; FallbackIconSpell = 48827; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = "Lance un marteau de lumière sur la cible, qui rebondit sur jusqu'à 4 ennemis proches : dégâts du Sacré et 1 charge de Puissance sacrée par ennemi touché. En Sacré, soigne aussi les 5 alliés les plus blessés, 1 charge de Puissance sacrée par allié soigné, et peut viser un allié."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 25 })
       Fields = @{ 1 = 0; 41 = 0; 46 = 4; 131 = 7886; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagDivineToll; 225 = 2; 226 = 0 } },
    @{ Id = 92905; Clone = 47632; Name = 'Glas divin'; IconPath = 'Interface\Icons\Spell_Holy_SearingLight'; FallbackIconSpell = 48827; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts du Sacré.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1000 })
       Fields = @{ 41 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagDivineToll; 225 = 2; 226 = 0 } },
    @{ Id = 92906; Clone = 25914; Name = 'Glas divin'; IconPath = 'Interface\Icons\Spell_Holy_SearingLight'; FallbackIconSpell = 48827; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1500 })
       Fields = @{ 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagDivineToll } },
    # Ailes de la vengeance: held by mod-paladin while Avenging Wrath is up - 20% melee and spell haste
    @{ Id = 92907; Clone = 2983; Name = 'Ailes de la vengeance'; IconPath = 'Interface\Icons\Spell_Holy_ProclaimChampion_02'; FallbackIconSpell = 31884; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Hâte augmentée de 20%.'; AuraDescription = 'Hâte augmentée de 20%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 20 },
           @{ Index = 1; Effect = 6; Aura = 65; TargetA = 1; Value = 20 })
       Fields = @{ 40 = 21 } },
    # Courroux de la justice: held while Avenging Wrath is up - Hammer of Wrath (word 1 0x80) ignores its target's
    # health (SPELL_AURA_ABILITY_IGNORE_AURASTATE)
    @{ Id = 92908; Clone = 2983; Name = 'Courroux de la justice'; IconPath = 'Interface\Icons\Spell_Holy_SealOfWrath'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Marteau de courroux peut frapper toute cible.'; AuraDescription = 'Marteau de courroux peut frapper une cible quelle que soit sa santé.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 262; TargetA = 1 })
       Fields = @{ 40 = 21; 122 = 0; 123 = 0x80; 124 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = 0 } },

    # --- Sacré ----------------------------------------------------------------------------------------------------
    # Lumière de l'aube (Light of Dawn): 3 Holy Power, a self dummy; mod-paladin heals (92911) the most injured allies
    # within 15 yd, 5 of them (more with Aube radieuse)
    @{ Id = 92910; Clone = 49039; Name = "Lumière de l'aube"; IconPath = 'Interface\Icons\Spell_Holy_PrayerOfHealing02'; FallbackIconSpell = 48825; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = "Consomme 3 charges de Puissance sacrée : soigne jusqu'à 5 alliés blessés à 15 m autour de vous."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 0; 41 = 0; 131 = 126; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagLightOfDawn; 225 = 2; 226 = 0 } },
    @{ Id = 92911; Clone = 25914; Name = "Lumière de l'aube"; IconPath = 'Interface\Icons\Spell_Holy_PrayerOfHealing02'; FallbackIconSpell = 48825; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1800 })
       Fields = @{ 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagLightOfDawn } },
    # Infusion de lumière: Flash of Light and Holy Light (word 0 0x40000000, 0x80000000) instant; mod-paladin removes
    # it as one of them is cast, and gives the Holy Power
    @{ Id = 92912; Clone = 2983; Name = 'Infusion de lumière'; IconPath = 'Interface\Icons\Ability_Paladin_InfusionofLight'; FallbackIconSpell = 53569; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochain Éclair lumineux ou Lumière sacrée est instantané.'; AuraDescription = 'Votre prochain Éclair lumineux ou Lumière sacrée est instantané et vous confère 1 charge de Puissance sacrée.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 8; 122 = 0xC0000000; 123 = 0; 124 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = 0 } },
    # Holy Shock's charges, shown as stacks (mod-paladin keeps the count)
    @{ Id = 92913; Clone = 2983; Name = 'Charges de Horion sacré'; IconPath = 'Interface\Icons\Spell_Holy_SearingLight'; FallbackIconSpell = 48825; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Horion sacré.'; AuraDescription = 'Charges de Horion sacré disponibles.'; Fields = @{ 40 = 21 } },
    # Lueur de lumière (Glimmer of Light): the mark Holy Shock leaves; each Holy Shock heals (92915) the allies and
    # hurts (92916) the enemies that carry it
    @{ Id = 92914; Clone = 2983; Name = 'Lueur de lumière'; IconPath = 'Interface\Icons\Spell_Holy_SurgeOfLight'; FallbackIconSpell = 48825; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Une lueur de lumière.'; AuraDescription = 'Chaque Horion sacré du paladin vous atteint aussi.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 9 } },
    @{ Id = 92915; Clone = 25914; Name = 'Lueur de lumière'; IconPath = 'Interface\Icons\Spell_Holy_SurgeOfLight'; FallbackIconSpell = 48825; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 700 })
       Fields = @{ 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagGlimmer } },
    @{ Id = 92916; Clone = 47632; Name = 'Lueur de lumière'; IconPath = 'Interface\Icons\Spell_Holy_SurgeOfLight'; FallbackIconSpell = 48825; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts du Sacré.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 500 })
       Fields = @{ 41 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagGlimmer; 225 = 2; 226 = 0 } },
    # Guide de foi (Beacon of Faith): a second beacon, 1 min like Beacon of Light; mod-paladin copies half of the
    # Paladin's heals on others to it (92919)
    @{ Id = 92917; Clone = 1044; Name = 'Guide de foi'; IconPath = 'Interface\Icons\Spell_Holy_ChampionsBond'; FallbackIconSpell = 53563; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = 'La cible devient un second guide : vos soins sur les autres alliés la soignent aussi de 50% de leur montant.'
       AuraDescription = 'Les soins du paladin sur les autres alliés vous soignent aussi.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 21 })
       Fields = @{ 1 = 0; 3 = 0; 34 = 0; 35 = 0; 36 = 0; 40 = 3; 46 = 5; 204 = 15; 208 = 10; 209 = 0; 210 = 0; 211 = $flagBeaconOfFaith } },
    # Guide de vertu (Beacon of Virtue): the target and, by mod-paladin, the 3 most injured allies near it, 8 s
    @{ Id = 92918; Clone = 1044; Name = 'Guide de vertu'; IconPath = 'Interface\Icons\Spell_Holy_CircleOfRenewal'; FallbackIconSpell = 53563; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = "La cible et les 3 alliés les plus blessés près d'elle deviennent des guides pendant 8 s : chacun de vos soins sur l'un d'eux soigne les autres de 40%."
       AuraDescription = 'Les soins du paladin sur un autre guide vous soignent aussi.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 21 })
       Fields = @{ 1 = 0; 3 = 0; 34 = 0; 35 = 0; 36 = 0; 40 = 31; 46 = 5; 204 = 10; 208 = 10; 209 = 0; 210 = 0; 211 = $flagBeaconOfVirtue } },
    # The heals mod-paladin hands out at amounts it works out (beacons, Avenging Crusader, Word of Glory's splash): no
    # family flag, so no talent modifier counts them twice
    @{ Id = 92919; Clone = 25914; Name = 'Lumière du guide'; IconPath = 'Interface\Icons\Ability_Paladin_BeaconofLight'; FallbackIconSpell = 53563; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = 0 } },
    # Croisé vengeur (Avenging Crusader): 15% damage for 20 s, every minute; mod-paladin turns Crusader Strike and Holy
    # Shock damage into heals (92921) on the 3 most injured allies
    @{ Id = 92920; Clone = 49039; Name = 'Croisé vengeur'; IconPath = 'Interface\Icons\Spell_Holy_Crusade'; FallbackIconSpell = 31884; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = 'Pendant 20 s, vous infligez 15% de dégâts en plus, et les dégâts de Frappe du croisé et de Horion sacré soignent les 3 alliés les plus blessés proches de 200% de leur montant.'
       AuraDescription = 'Dégâts augmentés de 15%. Frappe du croisé et Horion sacré soignent les alliés proches.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 127 })
       Fields = @{ 9 = 0; 40 = 18; 208 = 10; 209 = 0; 210 = 0; 211 = $flagAvengingCrusader; 226 = 0 } },
    @{ Id = 92921; Clone = 25914; Name = 'Croisé vengeur'; IconPath = 'Interface\Icons\Spell_Holy_Crusade'; FallbackIconSpell = 31884; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92922; Clone = 25914; Name = 'Dispensateur de lumière'; IconPath = 'Interface\Icons\Spell_Holy_Rapture'; FallbackIconSpell = 48785; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = 0 } },

    # --- Protection -----------------------------------------------------------------------------------------------
    # Bouclier du vertueux (Shield of the Righteous): 3 Holy Power, off the global cooldown, a second between two; holy
    # damage to the target, and mod-paladin puts up (or lengthens) its armor (92931)
    @{ Id = 92930; Clone = 61411; Name = 'Bouclier du vertueux'; IconPath = 'Interface\Icons\Ability_Paladin_ShieldoftheTemplar'; FallbackIconSpell = 61411; Cost = 0; Cooldown = 1000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Consomme 3 charges de Puissance sacrée : frappe la cible avec votre bouclier (dégâts du Sacré) et augmente votre armure de 30% pendant 4,5 s, durée qui se cumule jusqu''à 13,5 s. Ne déclenche pas le temps de recharge global.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = @{ 1 = 0; 204 = 0; 205 = 0; 206 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagShieldOfTheRighteous } },
    @{ Id = 92931; Clone = 2983; Name = 'Bouclier du vertueux'; IconPath = 'Interface\Icons\Ability_Paladin_ShieldoftheTemplar'; FallbackIconSpell = 61411; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Armure augmentée.'; AuraDescription = 'Armure augmentée de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 101; TargetA = 1; Value = 30; Misc = 1 })
       Fields = @{ 40 = 32 } },
    # Défenseur ardent (Ardent Defender): 20% less damage for 8 s; mod-paladin turns the killing blow into a heal to
    # 20% health
    @{ Id = 92932; Clone = 49039; Name = 'Défenseur ardent'; IconPath = 'Interface\Icons\Spell_Holy_ArdentDefender'; FallbackIconSpell = 31850; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Pendant 8 s, les dégâts que vous subissez sont réduits de 20%, et un coup qui devrait vous tuer vous ramène à la place à 20% de vos points de vie.'
       AuraDescription = 'Dégâts subis réduits de 20%. Un coup fatal vous ramène à 20% de vos points de vie.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -20; Misc = 127 })
       Fields = @{ 9 = 0; 40 = 31; 208 = 10; 209 = 0; 210 = 0; 211 = $flagArdentDefender; 226 = 0 } },
    # Œil de Tyr (Eye of Tyr): holy damage within 10 yd, and the enemies hit deal 25% less damage for 10 s
    @{ Id = 92933; Clone = 49941; Name = 'Œil de Tyr'; IconPath = 'Interface\Icons\Spell_Holy_EmpowerChampion'; FallbackIconSpell = 48817; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Libère la lumière de Tyr : dégâts du Sacré aux ennemis à 10 m, qui infligent 25% de dégâts en moins pendant 10 s.'
       AuraDescription = 'Dégâts infligés réduits de 25%.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 900 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 22; Value = -25; Misc = 127 })
       Fields = @{ 1 = 0; 40 = 1; 41 = 0; 89 = 15; 90 = 15; 92 = 13; 93 = 13; 131 = 126; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagEyeOfTyr; 225 = 2; 226 = 0 } },
    # Gardien des anciens rois (Guardian of Ancient Kings): half the damage for 8 s
    @{ Id = 92934; Clone = 49039; Name = 'Gardien des anciens rois'; IconPath = 'Interface\Icons\Spell_Holy_Heroism'; FallbackIconSpell = 498; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Pendant 8 s, les dégâts que vous subissez sont réduits de 50%.'
       AuraDescription = 'Dégâts subis réduits de 50%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -50; Misc = 127 })
       Fields = @{ 9 = 0; 40 = 31; 208 = 10; 209 = 0; 210 = 0; 211 = $flagGuardian; 226 = 0 } },
    # Bastion de lumière (Bastion of Light): 3 stacks (mod-paladin sets them), each a free Shield of the Righteous
    @{ Id = 92935; Clone = 49039; Name = 'Bastion de lumière'; IconPath = 'Interface\Icons\Spell_Holy_SealOfProtection'; FallbackIconSpell = 53600; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Vos 3 prochains Boucliers du vertueux ne coûtent aucune Puissance sacrée.'
       AuraDescription = 'Bouclier du vertueux ne coûte aucune Puissance sacrée.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 8; 49 = 3; 208 = 10; 209 = 0; 210 = 0; 211 = $flagBastion; 226 = 0 } },
    # Sol consacré: held by mod-paladin while the Paladin stands in its own Consecration
    @{ Id = 92936; Clone = 2983; Name = 'Sol consacré'; IconPath = 'Interface\Icons\Spell_Holy_InnerFire'; FallbackIconSpell = 48819; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vous vous tenez dans votre Consécration.'; AuraDescription = 'Dégâts subis réduits de 10%, dégâts du Sacré infligés augmentés de 10%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -10; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 2 })
       Fields = @{ 40 = 21 } },
    # The improved Avenger's Shield's silence, 3 s
    @{ Id = 92937; Clone = 15487; Name = 'Bouclier du vengeur'; IconPath = 'Interface\Icons\Spell_Holy_AvengersShield'; FallbackIconSpell = 48827; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Réduit au silence.'; AuraDescription = 'Réduit au silence.'
       Fields = @{ 1 = 0; 12 = 0; 40 = 27; 41 = 0; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = 0 } },
    # Rempart de fureur vertueuse: 20% more damage on the next Shield of the Righteous per stack, 5 stacks
    @{ Id = 92938; Clone = 2983; Name = 'Rempart de fureur vertueuse'; IconPath = 'Interface\Icons\Spell_Holy_RighteousnessAura'; FallbackIconSpell = 48827; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Bouclier du vertueux renforcé.'; AuraDescription = 'Votre prochain Bouclier du vertueux inflige 20% de dégâts en plus par charge.'
       Fields = @{ 40 = 9 } },

    # --- Vindicte -------------------------------------------------------------------------------------------------
    # Verdict du templier (Templar's Verdict): 3 Holy Power, 280% weapon damage plus 400 as holy
    @{ Id = 92940; Clone = 35395; Name = 'Verdict du templier'; IconPath = 'Interface\Icons\Spell_Holy_RighteousFury'; FallbackIconSpell = 35395; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = "Consomme 3 charges de Puissance sacrée : frappe la cible avec une puissance divine (280% des dégâts de l'arme plus 400, sous forme de dégâts du Sacré)."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 6; Value = 400 },
           @{ Index = 1; Effect = 31; TargetA = 6; Value = 280 })
       Fields = @{ 1 = 0; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagTemplarsVerdict; 225 = 2 } },
    # Lame de justice (Blade of Justice): instant holy damage at 30 yd, 2 Holy Power, every 8 s
    @{ Id = 92941; Clone = 48801; Name = 'Lame de justice'; IconPath = 'Interface\Icons\Spell_Holy_Excorcism_02'; FallbackIconSpell = 48801; Cost = 0; Cooldown = 8000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = 'Transperce la cible d''une lame de lumière : dégâts du Sacré et 2 charges de Puissance sacrée.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1300 })
       Fields = @{ 1 = 0; 28 = 1; 46 = 4; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagBladeOfJustice; 225 = 2 } },
    # Tempête divine (Divine Storm): the WotLK talent's spell (its script heals and reaches 6 in a dungeon, bound in
    # mod-paladin's SQL) as a finisher - 3 Holy Power, no cooldown, 160% weapon damage (field 82: effect 2's raw base
    # points). Its family flags stay: Sanctity of Battle and the tree's modifiers reach it
    @{ Id = 92942; Clone = 53385; Name = 'Tempête divine'; IconPath = 'Interface\Icons\Ability_Paladin_DivineStorm'; FallbackIconSpell = 53385; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = "Consomme 3 charges de Puissance sacrée : frappe jusqu'à 5 ennemis à 8 m (160% des dégâts de l'arme) et soigne vos alliés de 25% des dégâts infligés."
       Fields = @{ 1 = 0; 82 = 159; 204 = 0 } },
    # Réveil des cendres (Wake of Ashes): a 12 yd cone of holy damage and a 50% slow for 4 s, 3 Holy Power, every 30 s
    @{ Id = 92943; Clone = 42931; Name = 'Réveil des cendres'; IconPath = 'Interface\Icons\Spell_Holy_AshesToAshes'; FallbackIconSpell = 48817; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = 'Un torrent de cendres ardentes devant vous : dégâts du Sacré sur 12 m, ennemis ralentis de 50% pendant 4 s, et 3 charges de Puissance sacrée.'
       AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 104; Value = 1500 },
           @{ Index = 1; Effect = 6; Aura = 33; TargetA = 104; Value = -50 })
       Fields = @{ 1 = 0; 40 = 35; 41 = 0; 92 = 32; 93 = 32; 131 = 126; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagWakeOfAshes; 225 = 2; 226 = 0 } },
    # Jugement final (Final Reckoning): a 30 yd dummy; mod-paladin strikes (92945) the target and every enemy within
    # 8 yd of it, and marks them (92946): 30% more damage from the Paladin's finishers for 8 s
    @{ Id = 92944; Clone = 49909; Name = 'Jugement final'; IconPath = 'Interface\Icons\Spell_Holy_Excorcism'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = "Un marteau de lumière s'abat sur la cible : dégâts du Sacré aux ennemis à 8 m, qui subissent 30% de dégâts en plus de vos techniques de Puissance sacrée pendant 8 s."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 1 = 0; 41 = 0; 46 = 4; 131 = 7250; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagFinalReckoning; 225 = 2; 226 = 0 } },
    @{ Id = 92945; Clone = 47632; Name = 'Jugement final'; IconPath = 'Interface\Icons\Spell_Holy_Excorcism'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts du Sacré.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 2000 })
       Fields = @{ 41 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagFinalReckoning; 225 = 2; 226 = 0 } },
    @{ Id = 92946; Clone = 2983; Name = 'Jugement final'; IconPath = 'Interface\Icons\Spell_Holy_Excorcism'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Jugé.'; AuraDescription = 'Subit 30% de dégâts en plus des techniques de Puissance sacrée du paladin.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 31 } },
    # Sentence d'exécution (Execution Sentence): an 8 s mark; as it ends, mod-paladin brings the sentence down (92948)
    # with a fifth of the damage the Paladin dealt the target meanwhile
    @{ Id = 92947; Clone = 49909; Name = "Sentence d'exécution"; IconPath = 'Interface\Icons\Spell_Holy_Persecution'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = 'Condamne la cible : au bout de 8 s, elle subit de lourds dégâts du Sacré, plus 20% des dégâts que vous lui avez infligés entre-temps.'
       AuraDescription = "Subira de lourds dégâts du Sacré à la fin de l'effet."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 })
       Fields = @{ 1 = 0; 40 = 31; 41 = 0; 46 = 4; 131 = 7250; 204 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagExecutionSentence; 225 = 2; 226 = 0 } },
    @{ Id = 92948; Clone = 47632; Name = "Sentence d'exécution"; IconPath = 'Interface\Icons\Spell_Holy_Persecution'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts du Sacré.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 2500 })
       Fields = @{ 41 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagExecutionSentence; 225 = 2; 226 = 0 } },
    # Croisade (Crusade): 10% damage for 20 s, every 1 min 30 s; mod-paladin adds a stack (92950) per Holy Power spent
    @{ Id = 92949; Clone = 49039; Name = 'Croisade'; IconPath = 'Interface\Icons\Spell_Holy_Crusade'; FallbackIconSpell = 31884; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = "Pendant 20 s, vous infligez 10% de dégâts en plus, et chaque charge de Puissance sacrée dépensée augmente encore vos dégâts de 2% et votre hâte de 2%, jusqu'à 10 fois."
       AuraDescription = 'Dégâts augmentés de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 127 })
       Fields = @{ 9 = 0; 40 = 18; 208 = 10; 209 = 0; 210 = 0; 211 = $flagCrusade; 226 = 0 } },
    @{ Id = 92950; Clone = 2983; Name = 'Croisade'; IconPath = 'Interface\Icons\Spell_Holy_Crusade'; FallbackIconSpell = 31884; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Dégâts et hâte augmentés.'; AuraDescription = 'Dégâts et hâte augmentés de 2% par charge.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 2; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 2 })
       Fields = @{ 40 = 18 } },
    # Bouclier de vengeance (Shield of Vengeance): a self dummy; mod-paladin casts the absorb (92955) at 30% health
    @{ Id = 92951; Clone = 49039; Name = 'Bouclier de vengeance'; IconPath = 'Interface\Icons\Ability_Paladin_ShieldofVengeance'; FallbackIconSpell = 53601; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $retribution; ClassMask = $classMask
       Description = 'Un bouclier de lumière absorbe des dégâts égaux à 30% de vos points de vie maximum pendant 10 s.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 0; 208 = 10; 209 = 0; 210 = 0; 211 = $flagShieldOfVengeance; 226 = 0 } },
    # Puissance empyréenne (Empyrean Power): the next Divine Storm is free
    @{ Id = 92952; Clone = 2983; Name = 'Puissance empyréenne'; IconPath = 'Interface\Icons\Spell_Holy_Crusade'; FallbackIconSpell = 53385; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochaine Tempête divine est gratuite.'; AuraDescription = 'Votre prochaine Tempête divine ne coûte aucune Puissance sacrée.'
       Fields = @{ 40 = 8 } },
    # Expurgation: holy damage every 2 s for 6 s
    @{ Id = 92953; Clone = 2983; Name = 'Expurgation'; IconPath = 'Interface\Icons\Spell_Holy_SealOfBlood'; FallbackIconSpell = 48801; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts du Sacré.'; AuraDescription = 'Dégâts du Sacré toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 150 })
       Fields = @{ 40 = 32; 98 = 2000; 208 = 10; 209 = 0; 210 = 0; 211 = $flagExpurgation; 225 = 2 } },
    # L'art de la guerre: Blade of Justice's cooldown was just reset (shown for 10 s)
    @{ Id = 92954; Clone = 2983; Name = "L'art de la guerre"; IconPath = 'Interface\Icons\Ability_Paladin_ArtofWar'; FallbackIconSpell = 53486; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Lame de justice est prête.'; AuraDescription = 'Lame de justice est prête.'
       Fields = @{ 40 = 1 } },
    @{ Id = 92955; Clone = 2983; Name = 'Bouclier de vengeance'; IconPath = 'Interface\Icons\Ability_Paladin_ShieldofVengeance'; FallbackIconSpell = 53601; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_SchoolAbsorb; TargetA = 1; Value = 1; Misc = 127 }); Fields = @{ 40 = 1 } },

    # --- Spec passives --------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-paladin also reads them to know which one
    # is on. Spell modifiers on the stock spells' family flags: Flash of Light word 0 0x40000000, Holy Light word 0
    # 0x80000000, Holy Shock's heal word 1 0x10000, Avenger's Shield word 0 0x4000, Crusader Strike word 1 0x8000,
    # Judgement of Light and of Wisdom word 0 0x800000, Judgement of Justice word 2 0x8.
    # Grâce sacrée (Holy): Holy Shock gives Holy Power (mod-paladin), Flash of Light and Holy Light cost 10% less mana,
    # Holy Shock heals 10% more
    @{ Id = 93000; Clone = 2983; Name = 'Grâce sacrée'; IconPath = 'Interface\Icons\Spell_Holy_HolyBolt'; FallbackIconSpell = 20473; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Horion sacré vous confère 1 charge de Puissance sacrée et soigne 10% de plus ; Éclair lumineux et Lumière sacrée coûtent 10% de mana en moins. Mot de gloire et Lumière de l'aube dépensent votre Puissance sacrée."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -10; Misc = $SPELLMOD_COST },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 10; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = 0xC0000000; 123 = 0; 124 = 0; 125 = 0; 126 = 0x10000; 127 = 0; 208 = 10 } },
    # Bastion sacré (Protection): 10% stamina and armor, Avenger's Shield every 15 s
    @{ Id = 93001; Clone = 2983; Name = 'Bastion sacré'; IconPath = 'Interface\Icons\Spell_Holy_DevotionAura'; FallbackIconSpell = 31935; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Votre Endurance et votre armure augmentent de 10%, et Bouclier du vengeur se recharge en 15 s. Marteau du vertueux et vos Jugements vous confèrent de la Puissance sacrée, que Bouclier du vertueux et Mot de gloire dépensent."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModTotalStatPercentage; TargetA = 1; Value = 10; Misc = 2 },
           @{ Index = 1; Effect = 6; Aura = 101; TargetA = 1; Value = 10; Misc = 1 },
           @{ Index = 2; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -15000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 128 = 0x4000; 129 = 0; 130 = 0; 208 = 10 } },
    # Zèle vindicatif (Retribution): Crusader Strike deals 20% more, the Judgements come back 2 s sooner
    @{ Id = 93002; Clone = 2983; Name = 'Zèle vindicatif'; IconPath = 'Interface\Icons\Spell_Holy_AuraOfLight'; FallbackIconSpell = 35395; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Frappe du croisé inflige 20% de dégâts en plus et vos Jugements se rechargent 2 s plus vite. Frappe du croisé, Jugement et Lame de justice vous confèrent de la Puissance sacrée, que Verdict du templier et Tempête divine dépensent."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 20; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -2000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0; 123 = 0x8000; 124 = 0; 125 = 0x800000; 126 = 0; 127 = 0x8; 208 = 10 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-paladin reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\paladin\talentTree.json') -Family 10

return $spells
