# The Hunter's spell data for its retail-style talent trees (localTools/hunter/talentTree.json): the abilities its
# nodes and specializations teach, the auras its scripted talents show, and the rank spells of its new talents.
# Behaviour lives in modules/mod-hunter; this file owns client/server spell data only. The stock Hunter spells it
# changes in place (Focus costs, Kill Command, Aimed Shot, Raptor Strike...) are in localTools/hunter/StockSpells.ps1.
# Ids 93100-93399: 93100-93199 talent ranks (generated from the tree), 93200-93379 abilities and auras, 93380-93389
# the specializations' passives.
#
# Clone field notes: 1 category, 3 mechanic, 12-13 stances, 16 target flags, 28 casting time index (1 instant, 5 2 s,
# 19 2.5 s), 29 cooldown, 30 category cooldown, 40 duration index (1 10 s, 3 1 min, 8 15 s, 18 20 s, 21 never, 27 3 s,
# 31 8 s, 32 6 s, 35 4 s, 39 2 s, 85 18 s, 305 14 s), 41 power type (2 Focus), 42 cost, 46 range index (1 self, 5 40 yd,
# 35 35 yd, 37 50 yd, 54 5-30 yd, 114 the ranged weapon's), 47 speed (float bits; 0 instant), 49 stack amount, 68-70
# equipped item, 80-82 base points, 84-85 effect mechanic, 86-88 target A, 89-91 target B (15 enemies around the
# source), 92-94 radius index (8 5 yd, 13 10 yd, 14 8 yd), 98-100 periodic interval, 116-118 triggered spell, 122-130
# the effects' class masks, 131 visual, 204 mana cost percentage (0: the flat cost is Focus), 205-206 global cooldown
# category and time (0 0: off the global cooldown), 208 family (9 Hunter), 209-211 family flags, 212 max targets, 213
# damage class (2 melee, 3 ranged), 225 school (1 physical, 4 fire, 8 nature), 226 rune cost (cleared on Death Knight
# clones).
#
# Targets: 1 the caster, 6 the enemy target, 22 around the caster (with target B 15), 28 an area at the chosen spot,
# 77 the channel's target, 104 a cone in front.
#
# Focus (Focalisation) is the Hunter's power: power type 2, 100 at most, regenerating in the core (Player::Regenerate)
# with ranged haste. Every family 9 spell counts as a ranged weapon spell in the core (SpellInfo::IsRangedWeaponSpell):
# its attack power coefficients read the ranged attack power unless its damage class is melee (the Survival strikes).
# Damage the module works out itself (pet hits, ricochets, cleaves) goes through spells with explicit zero
# coefficients in modules/mod-hunter's SQL, cloned from Death Coil (no weapon requirement, so the pet can cast them).

$classMask = 4
$beastMastery = 50
$marksmanship = 163
$survival = 51

# The new abilities' own family flags, word 2 (bits 0x200000-0x40000000 are free among the Hunter's spells; the core
# reads the aspects from 0x1010 there and the traps from 0x24000, both left out). Only the spells a talent modifies
# carry one.
$flagBarbedShot = 0x200000
$flagCobraShot = 0x400000
$flagRapidFire = 0x800000
$flagDefensives = 0x1000000
$flagWildfireBomb = 0x2000000
$flagHarpoon = 0x4000000
$flagCounterShot = 0x8000000
$flagVolley = 0x10000000
$flagButchery = 0x20000000
$flagFlanking = 0x40000000

# The stock spells the modifiers name: Aimed Shot word 0 0x20000, Raptor Strike and Mongoose Bite word 0 0x2, Kill
# Command word 1 0x800
$maskAimedShot = 0x20000
$maskKillCommand = 0x800
$maskRaptorStrike = 0x2

# Arcane Shot's layout for a ranged shot of the Hunter's own: a projectile (speed 40) on the ranged weapon. Its range
# (114, the ranged weapon's) cannot reach a target in melee: what Survival uses in melee (Counter Shot, the chakram,
# Spearhead) takes range 5 instead, 40 yd without that dead zone
$shot = 49045
# Death Coil's for damage the module sets (no weapon, so the pet can cast it too): speed and visual cleared
$computed = 47632
# Crusader Strike's for a weapon strike (121 normalized weapon damage plus the flat bonus, 31 the weapon percentage)
$strike = 35395
# Deterrence's for an instant self buff of the Hunter's (it asks for a melee weapon: NoEquipment)
$selfBuff = 19263

$spells = @(
    # --- Class tree -------------------------------------------------------------------------------------------------
    # Tir de contre (Counter Shot): interrupts, 3 s lockout (the spell's duration), off the global cooldown
    @{ Id = 93200; Clone = 34490; Name = 'Tir de contre'; IconPath = 'Interface\Icons\Ability_Hunter_SilentHunter'; FallbackIconSpell = 34490; Cost = 0; Cooldown = 24000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask
       Description = "Un tir qui interrompt l'incantation de la cible et l'empêche de lancer un sort de cette école pendant 3 s."
       Effects = @(@{ Index = 0; Effect = 68; TargetA = 6 })
       Fields = @{ 46 = 5; 1 = 0; 2 = 0; 40 = 27; 41 = 2; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagCounterShot; 225 = 1 } },
    # Enthousiasme (Exhilaration): 30% of the Hunter's health; mod-hunter heals the pet in full
    @{ Id = 93201; Clone = $selfBuff; Name = 'Enthousiasme'; IconPath = 'Interface\Icons\Ability_Hunter_RapidRegeneration'; FallbackIconSpell = 136; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous rend instantanément 30% de vos points de vie et 100% de ceux de votre familier.'
       Effects = @(@{ Index = 0; Effect = 136; TargetA = 1; Value = 30 })
       Fields = @{ 1 = 0; 40 = 0; 41 = 2; 131 = 240; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagDefensives } },
    # Piège goudronné (Tar Trap): a patch of tar at the chosen spot, 8 yd, 50% slower, 20 s
    @{ Id = 93202; Clone = 13810; Name = 'Piège goudronné'; IconPath = 'Interface\Icons\Spell_Nature_StrangleVines'; FallbackIconSpell = 13809; Cost = 0; Cooldown = 25000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = "Lance un piège qui répand une nappe de goudron de 8 m de rayon à l'endroit visé : les ennemis qui s'y trouvent sont ralentis de 50%. Dure 20 s."
       AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(@{ Index = 0; Effect = 27; Aura = 33; TargetA = 28; Value = -50 })
       Fields = @{ 40 = 18; 41 = 2; 46 = 5; 92 = 14; 204 = 0; 205 = 133; 206 = 1500; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 8 } },
    # Tir de lien (Binding Shot): a 5 yd tether at the chosen spot for 10 s; mod-hunter stuns (93204) whoever breaks it
    @{ Id = 93203; Clone = 13810; Name = 'Tir de lien'; IconPath = 'Interface\Icons\Spell_Nature_Web'; FallbackIconSpell = 1543; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = "Tire une flèche magique à l'endroit visé : pendant 10 s, les ennemis à moins de 5 m y sont liés, et ceux qui s'en éloignent sont étourdis 3 s."
       AuraDescription = "Lié par le Tir de lien : s'éloigner vous étourdit."
       Effects = @(@{ Index = 0; Effect = 27; Aura = 4; TargetA = 28 })
       Fields = @{ 3 = 0; 40 = 1; 41 = 2; 46 = 5; 92 = 8; 131 = 10387; 204 = 0; 205 = 133; 206 = 1500; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 64 } },
    @{ Id = 93204; Clone = 24394; Name = 'Tir de lien'; IconPath = 'Interface\Icons\Spell_Nature_Web'; FallbackIconSpell = 1543; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Étourdi.'; AuraDescription = 'Étourdi.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 12; TargetA = 6 })
       Fields = @{ 40 = 27; 208 = 9 } },
    # Camouflage: stealth for 1 min, 2% health a second; mod-hunter camouflages the pet too
    @{ Id = 93205; Clone = 1784; Name = 'Camouflage'; IconPath = 'Interface\Icons\Ability_Racial_ShadowMeld'; FallbackIconSpell = 1784; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = 'Vous et votre familier vous fondez dans le décor : camouflés pendant 1 min, vous récupérez 2% de vos points de vie chaque seconde. Attaquer ou subir des dégâts y met fin. Hors combat.'
       AuraDescription = 'Camouflé. Récupère 2% de ses points de vie chaque seconde.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 16; TargetA = 1; Value = 5 },
           @{ Index = 1; Effect = 6; Aura = 20; TargetA = 1; Value = 2 })
       Fields = @{ 1 = 0; 12 = 0; 13 = 0; 30 = 0; 40 = 3; 41 = 2; 99 = 1000; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Aspect de la tortue: held by mod-hunter while Deterrence is up - 30% less damage
    @{ Id = 93206; Clone = 2983; Name = 'Aspect de la tortue'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Turtle'; FallbackIconSpell = 19263; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits de 30%.'; AuraDescription = 'Dégâts subis réduits de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -30; Misc = 127 })
       Fields = @{ 40 = 21 } },
    # Survie du plus fort (Survival of the Fittest): 30% less damage for 6 s, off the global cooldown
    @{ Id = 93207; Clone = $selfBuff; Name = 'Survie du plus fort'; IconPath = 'Interface\Icons\Ability_Hunter_SurvivalInstincts'; FallbackIconSpell = 19263; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask; NoEquipment = $true
       Description = 'Réduit de 30% les dégâts que vous subissez pendant 6 s.'
       AuraDescription = 'Dégâts subis réduits de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -30; Misc = 127 })
       Fields = @{ 1 = 0; 40 = 32; 41 = 2; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagDefensives } },
    # Coup fatal (Deathblow): the next Kill Shot (word 1 0x800000) ignores its target's health
    @{ Id = 93208; Clone = 2983; Name = 'Coup fatal'; IconPath = 'Interface\Icons\Ability_Hunter_Assassinate'; FallbackIconSpell = 53351; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochain Tir mortel est utilisable sur toute cible.'; AuraDescription = 'Votre prochain Tir mortel est utilisable quelle que soit la santé de la cible.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 262; TargetA = 1 })
       Fields = @{ 40 = 1; 122 = 0; 123 = 0x800000; 124 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Chakram de la mort (Death Chakram): a dummy at the target; mod-hunter bounces it (93210) 7 times between the
    # target and the enemies near it, 3 Focus a hit
    @{ Id = 93209; Clone = $shot; Name = 'Chakram de la mort'; IconPath = 'Interface\Icons\Ability_UpgradeMoonGlaive'; FallbackIconSpell = 57755; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask
       Description = "Lance un chakram qui rebondit jusqu'à 7 fois entre la cible et les ennemis proches, infligeant des dégâts physiques et vous rendant 3 points de focalisation par coup."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 2; 131 = 13222; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    @{ Id = 93210; Clone = $shot; Name = 'Chakram de la mort'; IconPath = 'Interface\Icons\Ability_UpgradeMoonGlaive'; FallbackIconSpell = 57755; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 700 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 0; 131 = 13222; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    # Débandade (Stampede): an 8 s aura; mod-hunter sends beasts (93212) at up to 3 enemies within 30 yd each second
    @{ Id = 93211; Clone = $selfBuff; Name = 'Débandade'; IconPath = 'Interface\Icons\Ability_Hunter_BeastCall'; FallbackIconSpell = 53434; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask; NoEquipment = $true
       Description = "Pendant 8 s, des bêtes sauvages chargent chaque seconde jusqu'à 3 ennemis à moins de 30 m, infligeant des dégâts physiques."
       AuraDescription = 'Des bêtes sauvages chargent vos ennemis.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 31; 41 = 2; 131 = 246; 204 = 0; 205 = 133; 206 = 1500; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 93212; Clone = $computed; Name = 'Débandade'; IconPath = 'Interface\Icons\Ability_Hunter_BeastCall'; FallbackIconSpell = 53434; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 3; 225 = 1; 226 = 0 } },

    # --- Maîtrise des bêtes -----------------------------------------------------------------------------------------
    # Tir barbelé (Barbed Shot): a bleed for 8 s; mod-hunter keeps its charges (93235), stacks Frenzy (93221) on the
    # pet, gives 20 Focus over 8 s (93222) and brings Bestial Wrath 12 s closer
    @{ Id = 93220; Clone = 49001; Name = 'Tir barbelé'; IconPath = 'Interface\Icons\Ability_Hunter_SwiftStrike'; FallbackIconSpell = 1978; Cost = 0; Cooldown = 12000; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask
       Description = "Un tir barbelé qui fait saigner la cible pendant 8 s, attise la Frénésie de votre familier (10% de vitesse d'attaque par charge, 3 au plus), vous rend 20 points de focalisation en 8 s et réduit de 12 s le temps de recharge de Courroux bestial. 2 charges."
       AuraDescription = 'Saigne.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 250 })
       Fields = @{ 1 = 0; 2 = 0; 3 = 15; 30 = 0; 40 = 31; 41 = 2; 83 = 15; 98 = 2000; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagBarbedShot; 225 = 1 } },
    @{ Id = 93221; Clone = 2983; Name = 'Frénésie'; IconPath = 'Interface\Icons\Ability_Druid_Mangle2'; FallbackIconSpell = 19574; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = "Vitesse d'attaque augmentée."; AuraDescription = "Vitesse d'attaque augmentée de 10% par charge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 10 })
       Fields = @{ 40 = 31 } },
    @{ Id = 93222; Clone = 2983; Name = 'Tir barbelé'; IconPath = 'Interface\Icons\Ability_Hunter_SwiftStrike'; FallbackIconSpell = 1978; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Focalisation rendue.'; AuraDescription = 'Rend 5 points de focalisation toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 24; TargetA = 1; Value = 5; Misc = 2 })
       Fields = @{ 40 = 31; 98 = 2000 } },
    # Tir du cobra (Cobra Shot): Beast Mastery's filler, 35 Focus; mod-hunter brings Kill Command a second closer
    @{ Id = 93223; Clone = $shot; Name = 'Tir du cobra'; IconPath = 'Interface\Icons\Ability_Hunter_CobraStrikes'; FallbackIconSpell = 1978; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask
       Description = "Un tir rapide qui inflige des dégâts physiques et réduit de 1 s le temps de recharge d'Ordre de tuer."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 600 })
       Fields = @{ 1 = 0; 30 = 0; 41 = 2; 131 = 3179; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagCobraShot; 225 = 1 } },
    # Ordre de tuer's hit, cast by the pet at an amount mod-hunter works out from the Hunter's ranged attack power
    @{ Id = 93224; Clone = $computed; Name = 'Ordre de tuer'; IconPath = 'Interface\Icons\Ability_Hunter_KillCommand'; FallbackIconSpell = 34026; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },
    # Bête sauvage (Dire Beast): a dummy at the target; mod-hunter calls a beast on it for 8 s, and 20 Focus (93226)
    @{ Id = 93225; Clone = $shot; Name = 'Bête sauvage'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Wolf'; FallbackIconSpell = 883; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask
       Description = "Appelle une bête sauvage qui attaque la cible pendant 8 s et vous rend 20 points de focalisation au fil de ce temps."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 2; 47 = 0; 131 = 246; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    @{ Id = 93226; Clone = 2983; Name = 'Bête sauvage'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Wolf'; FallbackIconSpell = 883; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Focalisation rendue.'; AuraDescription = 'Rend 5 points de focalisation toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 24; TargetA = 1; Value = 5; Misc = 2 })
       Fields = @{ 40 = 31; 98 = 2000 } },
    # Fendoir de la bête (Beast Cleave): on the pet after Multi-Shot; its hits then reach (93228) the enemies near
    @{ Id = 93227; Clone = 2983; Name = 'Fendoir de la bête'; IconPath = 'Interface\Icons\Ability_Warrior_Cleave'; FallbackIconSpell = 2643; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Les attaques du familier frappent les ennemis proches.'; AuraDescription = 'Vos attaques frappent aussi les ennemis à moins de 8 m de votre cible.'
       Fields = @{ 40 = 35 } },
    @{ Id = 93228; Clone = $computed; Name = 'Fendoir de la bête'; IconPath = 'Interface\Icons\Ability_Warrior_Cleave'; FallbackIconSpell = 2643; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },
    # Aspect de la bête féroce (an Aspect of the Wild of retail's): 10% critical strike and 30% Focus regeneration for
    # 20 s; mod-hunter puts it on the pet too. Not an aspect to the core (none of the aspects' family flags)
    @{ Id = 93229; Clone = $selfBuff; Name = 'Aspect de la bête féroce'; IconPath = 'Interface\Icons\Ability_Hunter_BeastWithin'; FallbackIconSpell = 19574; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pendant 20 s, vous et votre familier gagnez 10% de chances de coup critique, et votre régénération de focalisation augmente de 30%.'
       AuraDescription = 'Chances de coup critique augmentées de 10%. Régénération de focalisation augmentée de 30%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModCritPct; TargetA = 1; Value = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_ModPowerRegenPercent; TargetA = 1; Value = 30; Misc = 2 })
       Fields = @{ 1 = 0; 40 = 18; 41 = 2; 131 = 5522; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Appel de la nature sauvage (Call of the Wild): a 20 s aura; mod-hunter calls a beast every 4 s and speeds up Kill
    # Command and Barbed Shot
    @{ Id = 93230; Clone = $selfBuff; Name = 'Appel de la nature sauvage'; IconPath = 'Interface\Icons\Ability_Hunter_BeastCall'; FallbackIconSpell = 53434; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask; NoEquipment = $true
       Description = "Pendant 20 s, une bête sauvage vient combattre à vos côtés toutes les 4 s, et Ordre de tuer et Tir barbelé se rechargent 50% plus vite."
       AuraDescription = "L'appel de la nature sauvage résonne."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 18; 41 = 2; 131 = 246; 204 = 0; 205 = 133; 206 = 1500; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Effusion de sang (Bloodshed): a dummy at the target; the pet makes it bleed (93232), and it takes 15% more from
    # the pet (mod-hunter)
    @{ Id = 93231; Clone = $shot; Name = 'Effusion de sang'; IconPath = 'Interface\Icons\Ability_Druid_Rake'; FallbackIconSpell = 1822; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $beastMastery; ClassMask = $classMask
       Description = 'Ordonne à votre familier de déchirer la cible : elle saigne pendant 18 s et subit 15% de dégâts en plus de votre familier.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 2; 47 = 0; 131 = 0; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    @{ Id = 93232; Clone = 2983; Name = 'Effusion de sang'; IconPath = 'Interface\Icons\Ability_Druid_Rake'; FallbackIconSpell = 1822; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Saigne.'; AuraDescription = 'Saigne et subit 15% de dégâts en plus du familier.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 1 })
       Fields = @{ 3 = 15; 40 = 85; 83 = 15; 98 = 2000; 208 = 9; 213 = 2; 225 = 1 } },
    # Piétinement (Stomp) and Compagnon brutal's extra attack: the pet's hits at amounts mod-hunter works out
    @{ Id = 93233; Clone = $computed; Name = 'Piétinement'; IconPath = 'Interface\Icons\ability_hunter_pet_rhino'; FallbackIconSpell = 19574; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },
    # The charges mod-hunter keeps, shown as stacks: Kill Command (Prédateur alpha), Barbed Shot
    @{ Id = 93234; Clone = 2983; Name = "Charges d'Ordre de tuer"; IconPath = 'Interface\Icons\Ability_Hunter_KillCommand'; FallbackIconSpell = 34026; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = "Charges d'Ordre de tuer."; AuraDescription = "Charges d'Ordre de tuer disponibles."; Fields = @{ 40 = 21 } },
    @{ Id = 93235; Clone = 2983; Name = 'Charges de Tir barbelé'; IconPath = 'Interface\Icons\Ability_Hunter_SwiftStrike'; FallbackIconSpell = 1978; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Tir barbelé.'; AuraDescription = 'Charges de Tir barbelé disponibles.'; Fields = @{ 40 = 21 } },
    @{ Id = 93236; Clone = $computed; Name = 'Compagnon brutal'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Bear'; FallbackIconSpell = 19574; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },
    # The wild beasts' own hits (Bête sauvage, Appel de la nature sauvage), cast by the beast
    @{ Id = 93237; Clone = $computed; Name = 'Bête sauvage'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Wolf'; FallbackIconSpell = 883; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },

    # --- Précision --------------------------------------------------------------------------------------------------
    # Tir en rafale (Rapid Fire): a 2 s channel on the target, a shot (93251) every third of a second, 1 Focus each
    @{ Id = 93250; Clone = 5143; Name = 'Tir en rafale'; IconPath = 'Interface\Icons\Ability_Hunter_RunningShot'; FallbackIconSpell = 3045; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask
       Description = 'Tire une rafale de 6 flèches sur la cible en 2 s, chacune infligeant des dégâts physiques et vous rendant 1 point de focalisation.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 },
           @{ Index = 1; Effect = 6; Aura = 23; TargetA = 1 })
       Fields = @{ 40 = 39; 41 = 2; 46 = 114; 99 = 333; 117 = 93251; 131 = 0; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagRapidFire; 225 = 1 } },
    @{ Id = 93251; Clone = $shot; Name = 'Tir en rafale'; IconPath = 'Interface\Icons\Ability_Hunter_RunningShot'; FallbackIconSpell = 3045; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 77; Value = 180 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 0; 131 = 8155; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagRapidFire; 225 = 1 } },
    # Tirs précis (Precise Shots): 2 stacks after Aimed Shot, each Arcane Shot or Multi-Shot spends one (75% more)
    @{ Id = 93252; Clone = 2983; Name = 'Tirs précis'; IconPath = 'Interface\Icons\Ability_Hunter_ZenArchery'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Tir des arcanes et Flèches multiples renforcés.'; AuraDescription = 'Votre prochain Tir des arcanes ou Flèches multiples inflige 75% de dégâts en plus.'
       Fields = @{ 40 = 8 } },
    # Tirs de ricochet (Trick Shots): the next Aimed Shot or Rapid Fire ricochets (93254)
    @{ Id = 93253; Clone = 2983; Name = 'Tirs de ricochet'; IconPath = 'Interface\Icons\Ability_Hunter_RunningShot'; FallbackIconSpell = 2643; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Visée et Tir en rafale ricochent.'; AuraDescription = "Votre prochain Visée ou Tir en rafale ricoche sur jusqu'à 4 autres ennemis."
       Fields = @{ 40 = 18 } },
    @{ Id = 93254; Clone = $shot; Name = 'Tirs de ricochet'; IconPath = 'Interface\Icons\Ability_Hunter_RunningShot'; FallbackIconSpell = 2643; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 0; 131 = 8155; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    # Tir assuré (Steady Focus): ranged haste for 15 s, its amount set by mod-hunter from the talent's rank
    @{ Id = 93255; Clone = 2983; Name = 'Tir assuré'; IconPath = 'Interface\Icons\Ability_Hunter_SteadyShot'; FallbackIconSpell = 56641; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Vitesse d'attaque à distance augmentée."; AuraDescription = "Vitesse d'attaque à distance augmentée."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 140; TargetA = 1; Value = 15 })
       Fields = @{ 40 = 8 } },
    # Loup solitaire (Lone Wolf): held by mod-hunter while no pet is out
    @{ Id = 93256; Clone = 2983; Name = 'Loup solitaire'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Wolf'; FallbackIconSpell = 883; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts augmentés de 15%.'; AuraDescription = 'Dégâts infligés augmentés de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 127 })
       Fields = @{ 40 = 21 } },
    # Salve (Volley): arrows rain on the chosen spot for 6 s, 8 yd, every second; mod-hunter gives Trick Shots
    @{ Id = 93257; Clone = 43265; Name = 'Salve'; IconPath = 'Interface\Icons\Ability_Marksmanship'; FallbackIconSpell = 1510; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask
       Description = "Une pluie de flèches s'abat pendant 6 s à l'endroit visé, infligeant des dégâts physiques chaque seconde aux ennemis dans un rayon de 8 m."
       Effects = @(@{ Index = 0; Effect = 27; Aura = $A_PeriodicDamage; TargetA = 28; Value = 150 })
       Fields = @{ 1 = 0; 30 = 0; 40 = 32; 41 = 2; 46 = 35; 92 = 14; 98 = 1000; 131 = 10384; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagVolley; 213 = 3; 225 = 1; 226 = 0 } },
    # Visée parfaite (Trueshot): 20% ranged haste, 50% Focus regeneration and Aimed Shot 50% faster to cast for 15 s;
    # mod-hunter doubles Aimed Shot's recharge
    @{ Id = 93258; Clone = $selfBuff; Name = 'Visée parfaite'; IconPath = 'Interface\Icons\Ability_TrueShot'; FallbackIconSpell = 19506; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask; NoEquipment = $true
       Description = "Pendant 15 s, votre vitesse d'attaque à distance augmente de 20%, votre régénération de focalisation de 50%, et Visée se lance 50% plus vite et se recharge deux fois plus vite."
       AuraDescription = "Vitesse d'attaque à distance augmentée de 20%, régénération de focalisation de 50%. Visée se lance 50% plus vite."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 140; TargetA = 1; Value = 20 },
           @{ Index = 1; Effect = 6; Aura = $A_ModPowerRegenPercent; TargetA = 1; Value = 50; Misc = 2 },
           @{ Index = 2; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -50; Misc = 10 })
       Fields = @{ 1 = 0; 40 = 8; 41 = 2; 128 = $maskAimedShot; 129 = 0; 130 = 0; 131 = 13245; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Chargement éclair (Lock and Load): the next Aimed Shot instant and free
    @{ Id = 93259; Clone = 2983; Name = 'Chargement éclair'; IconPath = 'Interface\Icons\Ability_Hunter_LockAndLoad'; FallbackIconSpell = 56342; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochain Visée est instantané et gratuit.'; AuraDescription = 'Votre prochain Visée est instantané et ne coûte rien.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 40 = 8; 122 = $maskAimedShot; 123 = 0; 124 = 0; 125 = $maskAimedShot; 126 = 0; 127 = 0; 208 = 9 } },
    # Rafale rationalisée (Streamline): the next Aimed Shot faster to cast, its share set by mod-hunter from the rank
    @{ Id = 93260; Clone = 2983; Name = 'Rafale rationalisée'; IconPath = 'Interface\Icons\Ability_Hunter_Quickshot'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Votre prochain Visée se lance plus vite."; AuraDescription = "Le temps d'incantation de votre prochain Visée est réduit."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -30; Misc = 10 })
       Fields = @{ 40 = 8; 122 = $maskAimedShot; 123 = 0; 124 = 0; 208 = 9 } },
    # Tir double (Double Tap): the next Aimed Shot or Rapid Fire fires twice (mod-hunter)
    @{ Id = 93261; Clone = $selfBuff; Name = 'Tir double'; IconPath = 'Interface\Icons\Ability_Hunter_Displacement'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask; NoEquipment = $true
       Description = 'Votre prochain Visée ou Tir en rafale tire deux fois.'
       AuraDescription = 'Votre prochain Visée ou Tir en rafale tire deux fois.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 8; 41 = 2; 131 = 0; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Flèche gémissante (Wailing Arrow): a 2 s cast; mod-hunter silences (93263) the target and the enemies within 8 yd,
    # and hits the others (93264) for 40%
    @{ Id = 93262; Clone = $shot; Name = 'Flèche gémissante'; IconPath = 'Interface\Icons\Ability_Hunter_EagleEye'; FallbackIconSpell = 19434; Cost = 15; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $marksmanship; ClassMask = $classMask
       Description = 'Une flèche hurlante qui inflige de lourds dégâts à la cible, 40% aux ennemis à moins de 8 m, et les réduit tous au silence pendant 3 s.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1500 })
       Fields = @{ 1 = 0; 28 = 5; 30 = 0; 41 = 2; 131 = 7955; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 32 } },
    @{ Id = 93263; Clone = 15487; Name = 'Flèche gémissante'; IconPath = 'Interface\Icons\Ability_Hunter_EagleEye'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Réduit au silence.'; AuraDescription = 'Réduit au silence.'
       Fields = @{ 1 = 0; 12 = 0; 29 = 0; 40 = 27; 41 = 0; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 93264; Clone = $computed; Name = 'Flèche gémissante'; IconPath = 'Interface\Icons\Ability_Hunter_EagleEye'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 3; 225 = 32; 226 = 0 } },
    # Héritage des Coursevent (Legacy of the Windrunners): the wind arrows
    @{ Id = 93265; Clone = $shot; Name = 'Flèche du vent'; IconPath = 'Interface\Icons\Ability_Hunter_WildQuiver'; FallbackIconSpell = 53215; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Nature.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 0; 131 = 3179; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 8 } },
    @{ Id = 93266; Clone = 2983; Name = 'Charges de Visée'; IconPath = 'Interface\Icons\INV_Spear_07'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Charges de Visée.'; AuraDescription = 'Charges de Visée disponibles.'; Fields = @{ 40 = 21 } },

    # --- Survie -----------------------------------------------------------------------------------------------------
    # Bombe de feu sauvage (Wildfire Bomb): fire at the target; mod-hunter bursts it (93281) on the enemies within 8 yd
    # and sets them all burning (93282) for 6 s, and keeps its charges (93296)
    @{ Id = 93280; Clone = 60053; Name = 'Bombe de feu sauvage'; IconPath = 'Interface\Icons\INV_Misc_Bomb_05'; FallbackIconSpell = 13813; Cost = 0; Cooldown = 18000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = 'Lance une bombe incendiaire sur la cible : dégâts de Feu à elle et aux ennemis à moins de 8 m, qui brûlent ensuite pendant 6 s.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = @{ 1 = 0; 30 = 0; 40 = 0; 41 = 2; 46 = 5; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagWildfireBomb; 225 = 4 } },
    @{ Id = 93281; Clone = $computed; Name = 'Bombe de feu sauvage'; IconPath = 'Interface\Icons\INV_Misc_Bomb_05'; FallbackIconSpell = 13813; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Feu.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagWildfireBomb; 213 = 3; 225 = 4; 226 = 0 } },
    @{ Id = 93282; Clone = 2983; Name = 'Bombe de feu sauvage'; IconPath = 'Interface\Icons\INV_Misc_Bomb_05'; FallbackIconSpell = 13813; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Brûle.'; AuraDescription = 'Subit des dégâts de Feu chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 120 })
       Fields = @{ 40 = 32; 98 = 1000; 208 = 9; 209 = 0; 210 = 0; 211 = $flagWildfireBomb; 213 = 3; 225 = 4 } },
    # Harpon (Harpoon): Intercept's charge from 5 to 30 yd, and the target rooted 3 s
    @{ Id = 93283; Clone = 20252; Name = 'Harpon'; IconPath = 'Interface\Icons\Thrown_1H_Harpoon_D_01'; FallbackIconSpell = 20252; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = 'Lance un harpon sur la cible et vous tire jusqu''à elle, puis l''immobilise pendant 3 s.'
       AuraDescription = 'Immobilisé.'
       Effects = @(
           @{ Index = 0; Effect = 96; TargetA = 6 },
           @{ Index = 1; Effect = 6; Aura = 26; TargetA = 6 })
       Fields = @{ 1 = 0; 12 = 0; 13 = 0; 30 = 0; 40 = 27; 41 = 2; 46 = 54; 84 = 7; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagHarpoon; 225 = 1; 226 = 0 } },
    # Boucherie (Butchery): every enemy within 8 yd, weapon damage; 3 charges (93297) kept by mod-hunter
    @{ Id = 93284; Clone = $strike; Name = 'Boucherie'; IconPath = 'Interface\Icons\Ability_Whirlwind'; FallbackIconSpell = 1680; Cost = 30; Cooldown = 9000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = "Frappe tous les ennemis à moins de 8 m autour de vous (80% des dégâts de l'arme plus un bonus). 3 charges."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 22; Value = 150 },
           @{ Index = 1; Effect = 31; TargetA = 22; Value = 80 })
       Fields = @{ 1 = 0; 29 = 9000; 40 = 0; 41 = 2; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 131 = 223; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagButchery; 212 = 8 } },
    # Découpe (Carve): up to 5 enemies in front, weapon damage; mod-hunter brings Wildfire Bomb a second closer each
    @{ Id = 93285; Clone = $strike; Name = 'Découpe'; IconPath = 'Interface\Icons\Ability_Warrior_Cleave'; FallbackIconSpell = 845; Cost = 35; Cooldown = 6000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = "Frappe jusqu'à 5 ennemis devant vous (100% des dégâts de l'arme plus un bonus), et réduit de 1 s le temps de recharge de Bombe de feu sauvage par ennemi touché."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 104; Value = 200 },
           @{ Index = 1; Effect = 31; TargetA = 104; Value = 100 })
       Fields = @{ 1 = 0; 40 = 0; 41 = 2; 92 = 14; 93 = 14; 131 = 219; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagButchery; 212 = 5 } },
    # Assaut coordonné (Coordinated Assault): 20% damage for 20 s; mod-hunter puts it on the pet too
    @{ Id = 93286; Clone = $selfBuff; Name = 'Assaut coordonné'; IconPath = 'Interface\Icons\Ability_Hunter_Harass'; FallbackIconSpell = 19574; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous et votre familier attaquez de concert : vous infligez tous deux 20% de dégâts en plus pendant 20 s, et Ordre de tuer se recharge 50% plus vite."
       AuraDescription = 'Dégâts infligés augmentés de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 20; Misc = 127 })
       Fields = @{ 1 = 0; 40 = 18; 41 = 2; 131 = 7278; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0 } },
    # Pointe de la lance (Tip of the Spear), Furie de la mangouste (Mongoose Fury): stacks read by mod-hunter
    @{ Id = 93287; Clone = 2983; Name = 'Pointe de la lance'; IconPath = 'Interface\Icons\INV_Spear_07'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Frappes renforcées.'; AuraDescription = 'Votre prochaine Attaque du raptor, Découpe ou Boucherie inflige 25% de dégâts en plus par charge.'
       Fields = @{ 40 = 1 } },
    @{ Id = 93288; Clone = 2983; Name = 'Furie de la mangouste'; IconPath = 'Interface\Icons\Ability_Hunter_SwiftStrike'; FallbackIconSpell = 1495; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Attaque du raptor renforcée.'; AuraDescription = "Les dégâts d'Attaque du raptor augmentent de 15% par charge."
       Fields = @{ 40 = 305 } },
    # Frappe de flanc (Flanking Strike): the Hunter's strike, the pet's (93290), and 30 Focus (mod-hunter)
    @{ Id = 93289; Clone = $strike; Name = 'Frappe de flanc'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Raptor'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = "Vous et votre familier frappez la cible ensemble (vous : 150% des dégâts de l'arme plus un bonus), et vous récupérez 30 points de focalisation."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 6; Value = 400 },
           @{ Index = 1; Effect = 31; TargetA = 6; Value = 150 })
       Fields = @{ 1 = 0; 40 = 0; 41 = 2; 131 = 39; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = $flagFlanking } },
    @{ Id = 93290; Clone = $computed; Name = 'Frappe de flanc'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Raptor'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },
    # Fer de lance (Spearhead): a dummy at the target; the pet bleeds it (93292), the Hunter crits 20% more (93293)
    @{ Id = 93291; Clone = $shot; Name = 'Fer de lance'; IconPath = 'Interface\Icons\INV_Spear_08'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = 'Votre familier charge la cible et la fait saigner pendant 10 s, et vos chances de coup critique augmentent de 20% pendant ce temps.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 46 = 5; 1 = 0; 30 = 0; 41 = 2; 47 = 0; 131 = 0; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    @{ Id = 93292; Clone = 2983; Name = 'Fer de lance'; IconPath = 'Interface\Icons\INV_Spear_08'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Saigne.'; AuraDescription = 'Saigne.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 1 })
       Fields = @{ 3 = 15; 40 = 1; 83 = 15; 98 = 2000; 208 = 9; 213 = 2; 225 = 1 } },
    @{ Id = 93293; Clone = 2983; Name = 'Fer de lance'; IconPath = 'Interface\Icons\INV_Spear_08'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Chances de coup critique augmentées.'; AuraDescription = 'Chances de coup critique augmentées de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModCritPct; TargetA = 1; Value = 20 })
       Fields = @{ 40 = 1 } },
    # Fureur de l'aigle (Fury of the Eagle): a 3 s channel on the Hunter, a cone of strikes (93295) every half second
    @{ Id = 93294; Clone = 5143; Name = "Fureur de l'aigle"; IconPath = 'Interface\Icons\Ability_Hunter_EagleEye'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $survival; ClassMask = $classMask
       Description = 'Pendant 3 s, vous frappez sans relâche tous les ennemis devant vous.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 },
           @{ Index = 1; Effect = 6; Aura = 23; TargetA = 1 })
       Fields = @{ 16 = 0; 40 = 27; 41 = 2; 46 = 1; 99 = 500; 117 = 93295; 131 = 0; 204 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    @{ Id = 93295; Clone = $strike; Name = "Fureur de l'aigle"; IconPath = 'Interface\Icons\Ability_Hunter_EagleEye'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 104; Value = 60 },
           @{ Index = 1; Effect = 31; TargetA = 104; Value = 45 })
       Fields = @{ 1 = 0; 29 = 0; 40 = 0; 41 = 0; 92 = 14; 93 = 14; 131 = 223; 204 = 0; 205 = 0; 206 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 212 = 8 } },
    @{ Id = 93296; Clone = 2983; Name = 'Charges de Bombe de feu sauvage'; IconPath = 'Interface\Icons\INV_Misc_Bomb_05'; FallbackIconSpell = 13813; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Bombe de feu sauvage.'; AuraDescription = 'Charges de Bombe de feu sauvage disponibles.'; Fields = @{ 40 = 21 } },
    @{ Id = 93297; Clone = 2983; Name = 'Charges de Boucherie'; IconPath = 'Interface\Icons\Ability_Whirlwind'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Boucherie.'; AuraDescription = 'Charges de Boucherie disponibles.'; Fields = @{ 40 = 21 } },
    # Lance sanglante (Bloodseeker): Kill Command's bleed, 8 s
    @{ Id = 93298; Clone = 2983; Name = 'Lance sanglante'; IconPath = 'Interface\Icons\Ability_Druid_Rake'; FallbackIconSpell = 1822; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Saigne.'; AuraDescription = 'Saigne.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 100 })
       Fields = @{ 3 = 15; 40 = 31; 83 = 15; 98 = 2000; 208 = 9; 213 = 2; 225 = 1 } },
    # Bombes infusées: Wildfire Bomb's slow, 6 s
    @{ Id = 93299; Clone = 2983; Name = 'Bombe de feu sauvage'; IconPath = 'Interface\Icons\INV_Misc_Bomb_08'; FallbackIconSpell = 13813; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Ralenti.'; AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 33; TargetA = 6; Value = -50 })
       Fields = @{ 40 = 32; 208 = 9 } },

    # Termes de l'engagement: Harpoon's hit, at an amount mod-hunter works out
    @{ Id = 93300; Clone = $computed; Name = "Termes de l'engagement"; IconPath = 'Interface\Icons\Thrown_1H_Harpoon_D_01'; FallbackIconSpell = 20252; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 9; 209 = 0; 210 = 0; 211 = 0; 213 = 2; 225 = 1; 226 = 0 } },

    # Morsure de serpent as Morsure de vipère and Venin de vipère apply it from melee: Serpent Sting's highest rank (its
    # family flags, so Improved Stings, Noxious Stings and Chimera Shot know it) at range 5 - a triggered spell still
    # checks its range, and the ranged weapon's (114) cannot reach a target in melee
    @{ Id = 93301; Clone = 49001; Name = 'Morsure de serpent'; IconPath = 'Interface\Icons\Ability_Hunter_Quickshot'; FallbackIconSpell = 1978; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Nature toutes les 3 s pendant 15 s.'
       AuraDescription = 'Subit des dégâts de Nature toutes les 3 s.'
       Fields = @{ 41 = 2; 46 = 5; 204 = 0 } },

    # --- Spec passives ----------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-hunter also reads them to know which one
    # is on.
    # Maître des bêtes (Beast Mastery): the pet deals 15% more (mod-hunter), Kill Command (word 1 0x800) 20% more
    @{ Id = 93380; Clone = 2983; Name = 'Maître des bêtes'; IconPath = 'Interface\Icons\Ability_Hunter_BeastTaming'; FallbackIconSpell = 1515; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Votre familier inflige 15% de dégâts en plus et Ordre de tuer 20%. Tir barbelé attise la Frénésie de votre familier, Tir du cobra rapproche Ordre de tuer, et Flèches multiples ne coûte que 25 points de focalisation et touche 2 ennemis de plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 20; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -5; Misc = $SPELLMOD_COST },
           @{ Index = 2; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 2; Misc = 17 })
       Fields = @{ 122 = 0; 123 = $maskKillCommand; 124 = 0; 125 = 0x1000; 126 = 0; 127 = 0; 128 = 0x1000; 129 = 0; 130 = 0; 208 = 9 } },
    # Tireur d'élite (Marksmanship): Aimed Shot has 2 charges and Steady Shot gives 10 Focus (mod-hunter), Aimed Shot
    # deals 15% more
    @{ Id = 93381; Clone = 2983; Name = "Tireur d'élite"; IconPath = 'Interface\Icons\Ability_Hunter_FocusedAim'; FallbackIconSpell = 19434; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Visée a 2 charges et inflige 15% de dégâts en plus, et Tir assuré vous rend 10 points de focalisation. Vos dégâts physiques augmentent de 10%, et Flèches multiples touche 2 ennemis de plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 15; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 1 },
           @{ Index = 2; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 2; Misc = 17 })
       Fields = @{ 122 = $maskAimedShot; 123 = 0; 124 = 0; 128 = 0x1000; 129 = 0; 130 = 0; 208 = 9 } },
    # Survivant (Survival): Kill Command costs nothing and gives 15 Focus (mod-hunter), Raptor Strike and Mongoose Bite
    # (word 0 0x2) deal 20% more
    @{ Id = 93382; Clone = 2983; Name = 'Survivant'; IconPath = 'Interface\Icons\Ability_Hunter_SwiftStrike'; FallbackIconSpell = 2973; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Ordre de tuer ne coûte rien, se recharge en 6 s et vous rend 15 points de focalisation ; Attaque du raptor inflige 20% de dégâts en plus. Vous combattez au corps à corps : Attaque du raptor dépense la focalisation qu'Ordre de tuer vous rend."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 20; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 2; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -1500; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0; 123 = $maskKillCommand; 124 = 0; 125 = $maskRaptorStrike; 126 = 0; 127 = 0; 128 = 0; 129 = $maskKillCommand; 130 = 0; 208 = 9 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-hunter reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\hunter\talentTree.json') -Family 9

return $spells
