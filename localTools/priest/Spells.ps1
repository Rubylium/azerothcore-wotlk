# The Priest's spell data for its retail-style talent trees (localTools/priest/talentTree.json): the abilities its
# nodes and specializations teach, the auras its scripted talents show, and the rank spells of its new talents.
# Behaviour lives in modules/mod-priest; this file owns client/server spell data only. The stock Priest spells it
# changes in place (snappier casts, Shadow's mana costs, Devouring Plague on Insanity...) are in
# localTools/priest/StockSpells.ps1.
# Ids 93400-93699: 93400-93499 talent ranks (generated from the tree), 93500-93679 abilities and auras, 93680-93689
# the specializations' passives.
#
# Clone field notes: 1 category, 2 dispel, 3 mechanic, 16 target flags, 27 excluded target aura, 28 casting time index
# (1 instant, 16 1.5 s, 5 2 s, 19 2.5 s), 29 cooldown, 30 category cooldown, 40 duration index (1 10 s, 8 15 s, 18
# 20 s, 21 never, 27 3 s, 29 12 s, 31 8 s, 32 6 s, 35 4 s, 581 9 s), 41 power type (0 mana), 42 cost, 46 range index
# (1 self, 4 30 yd, 5 40 yd, 6 100 yd, 13 anywhere), 47 speed (float bits; 0 instant), 49 stack amount, 80-82 base
# points, 84-85 effect mechanic, 86-88 target A, 89-91 target B, 92-94 radius index (13 10 yd, 14 8 yd), 98-100
# periodic interval, 116-118 triggered spell, 122-130 the effects' class masks, 131 visual, 204 mana cost percentage,
# 205-206 global cooldown category and time, 208 family (6 Priest), 209-211 family flags, 225 school (2 holy, 4 fire,
# 32 shadow).
#
# Targets: 1 the caster, 6 the enemy target, 16 the enemies around the chosen spot, 21 a friendly target, 77 the
# channel's target.
#
# The Priest keeps mana. Shadow fights with Insanity (Démence, 93500): an aura of up to 100 stacks that mod-priest fills
# from Mind Blast, Vampiric Touch, Shadow Word: Pain, Mind Flay, Void Bolt, Void Torrent, the fiend and the
# apparitions, and that Devouring Plague spends (50). Damage and heals the module works out itself (Atonement, the
# relays, the splashes) go through spells with explicit zero coefficients in modules/mod-priest's SQL; the others carry
# their spell power coefficients there.

$classMask = 16
$discipline = 613
$holy = 56
$shadowMagic = 78

# The new abilities' own family flags, word 2 (bits 0x10000-0x40000000 are free among the Priest's spells; the core
# reads none of them). Only the spells a talent or the module's modifiers name carry one.
$flagRadiance = 0x10000
$flagSerenity = 0x20000
$flagSanctify = 0x40000
$flagChastise = 0x80000
$flagApotheosis = 0x100000
$flagDevouring = 0x200000
$flagSchism = 0x400000
$flagPurge = 0x800000
$flagVoidBolt = 0x1000000
$flagEruption = 0x2000000
$flagShadowCrash = 0x4000000
$flagVoidTorrent = 0x8000000
$flagStar = 0x10000000
$flagDisciplineCooldowns = 0x20000000
$flagMindgames = 0x40000000

# The stock spells the modifiers name: word 0 Power Word: Shield 0x1, Greater Heal 0x1000, Mind Blast 0x2000, Shadow
# Word: Pain 0x8000; word 1 Shadow Word: Death 0x2, Vampiric Touch 0x400; word 2 Mind Flay 0x40
$maskShield = 0x1
$maskGreaterHeal = 0x1000
$maskMindBlast = 0x2000

# Flash Heal's layout for a heal the Priest casts; Holy Shock's heal for one the module hands out (instant, triggered,
# 100 yd); Death Coil's for damage the module or a coefficient sets (no weapon, speed and visual cleared); Mind
# Blast's for a shadow spell on the target; Sprint's for an instant self buff (its category and cooldown cleared)
$castHeal = 48071
$healHit = 25914
$computed = 47632
$shadowBolt = 48127
$selfBuff = 2983

$spells = @(
    # --- Resources, shared auras ----------------------------------------------------------------------------------
    # Démence (Insanity): Shadow's resource, 100 stacks at most; fades 10 s after combat (mod-priest)
    @{ Id = 93500; Clone = 2983; Name = 'Démence'; IconPath = 'Interface\Icons\Spell_Shadow_Twilight'; FallbackIconSpell = 15473; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 100; Spellbook = $false
       Description = 'Démence.'; AuraDescription = 'Points de Démence : Peste dévorante en consomme 50.'
       Fields = @{ 40 = 21 } },
    # Expiation (Atonement): on an ally; the Priest's damage heals it (93502), 15 s
    @{ Id = 93501; Clone = 2983; Name = 'Expiation'; IconPath = 'Interface\Icons\Spell_Holy_Absolution'; FallbackIconSpell = 17; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Les dégâts du prêtre vous soignent.'; AuraDescription = 'Les dégâts infligés par le prêtre Discipline vous soignent.'
       Fields = @{ 40 = 8 } },
    @{ Id = 93502; Clone = $healHit; Name = 'Expiation'; IconPath = 'Interface\Icons\Spell_Holy_Absolution'; FallbackIconSpell = 17; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 131 = 0; 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },
    # Lumière protectrice: 10% less damage for 10 s after a Flash Heal on oneself
    @{ Id = 93503; Clone = 2983; Name = 'Lumière protectrice'; IconPath = 'Interface\Icons\Spell_Holy_HolyProtection'; FallbackIconSpell = 17; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits de 10%.'; AuraDescription = 'Dégâts subis réduits de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -10; Misc = 127 })
       Fields = @{ 40 = 1 } },
    # Image translucide: 10% less damage while Fade lasts
    @{ Id = 93504; Clone = 2983; Name = 'Image translucide'; IconPath = 'Interface\Icons\Spell_Magic_LesserInvisibilty'; FallbackIconSpell = 586; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits de 10%.'; AuraDescription = 'Dégâts subis réduits de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -10; Misc = 127 })
       Fields = @{ 40 = 1 } },
    # Alliance ténébreuse (Shadow Covenant): 15% more damage and healing for 12 s after calling the fiend
    @{ Id = 93505; Clone = 2983; Name = 'Alliance ténébreuse'; IconPath = 'Interface\Icons\Spell_Shadow_ShadesOfDarkness'; FallbackIconSpell = 34433; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts et soins augmentés de 15%.'; AuraDescription = 'Dégâts et soins augmentés de 15%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 136; TargetA = 1; Value = 15 })
       Fields = @{ 40 = 29 } },
    # Pouvoir du côté obscur (Power of the Dark Side): the next Penance 50% stronger (mod-priest)
    @{ Id = 93506; Clone = 2983; Name = 'Pouvoir du côté obscur'; IconPath = 'Interface\Icons\Spell_Shadow_Twilight'; FallbackIconSpell = 47540; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Pénitence renforcée.'; AuraDescription = 'Votre prochaine Pénitence inflige et soigne 50% de plus.'
       Fields = @{ 40 = 18 } },
    # The charges mod-priest keeps, shown as stacks: Power Word: Radiance, Mind Blast (Pensées du Vide), Holy Word:
    # Serenity (Faiseur de miracles)
    @{ Id = 93507; Clone = 2983; Name = 'Charges de Radiance'; IconPath = 'Interface\Icons\Spell_Holy_HolyNova'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Charges de Mot de pouvoir : Radiance.'; AuraDescription = 'Charges de Mot de pouvoir : Radiance disponibles.'; Fields = @{ 40 = 21 } },
    @{ Id = 93508; Clone = 2983; Name = "Charges d'Attaque mentale"; IconPath = 'Interface\Icons\Spell_Shadow_UnholyFrenzy'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = "Charges d'Attaque mentale."; AuraDescription = "Charges d'Attaque mentale disponibles."; Fields = @{ 40 = 21 } },
    @{ Id = 93509; Clone = 2983; Name = 'Charges de Sérénité'; IconPath = 'Interface\Icons\Spell_Holy_Heal02'; FallbackIconSpell = 2061; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Charges de Mot sacré : Sérénité.'; AuraDescription = 'Charges de Mot sacré : Sérénité disponibles.'; Fields = @{ 40 = 21 } },

    # --- Class tree -------------------------------------------------------------------------------------------------
    # Mot de pouvoir : Vie (Power Word: Life): instant, only on an ally below 35% (mod-priest), 20 s
    @{ Id = 93510; Clone = $castHeal; Name = 'Mot de pouvoir : Vie'; IconPath = 'Interface\Icons\Spell_Holy_Heal02'; FallbackIconSpell = 2061; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = "Soigne instantanément un allié sous 35% de vie d'un montant considérable."
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 4000 })
       Fields = @{ 28 = 1; 204 = 4; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },
    # Saut de foi (Leap of Faith): a dummy on an ally; mod-priest pulls it to the Priest
    @{ Id = 93511; Clone = $castHeal; Name = 'Saut de foi'; IconPath = 'Interface\Icons\Spell_Holy_PersuitofJustice'; FallbackIconSpell = 1706; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask
       Description = "Attire un allié jusqu'à vous par la foi."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 21 })
       Fields = @{ 28 = 1; 131 = 0; 204 = 3; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },
    # Étoile divine (Divine Star): a self dummy; mod-priest sends the star out and back through the enemies (93515) and
    # the allies (93516) within 24 yd in front
    @{ Id = 93513; Clone = 48078; Name = 'Étoile divine'; IconPath = 'Interface\Icons\Spell_Holy_HolyGuidance'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 15000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = "Lance une étoile de lumière qui parcourt 24 m devant vous puis revient : elle inflige des dégâts du Sacré aux ennemis et soigne les alliés qu'elle traverse, à l'aller comme au retour."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 1 = 0; 204 = 2; 208 = 6; 209 = 0; 210 = 0; 211 = $flagStar } },
    # Halo: a 1.5 s cast around the Priest; mod-priest strikes (93517) every enemy and heals (93518) the 6 most
    # injured allies within 30 yd
    @{ Id = 93514; Clone = 48078; Name = 'Halo'; IconPath = 'Interface\Icons\Spell_Holy_Nullifydisease'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = "Crée un anneau de lumière qui s'étend à 30 m : il inflige des dégâts du Sacré aux ennemis et soigne les 6 alliés les plus blessés."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 1 = 0; 28 = 16; 204 = 5; 208 = 6; 209 = 0; 210 = 0; 211 = $flagStar } },
    @{ Id = 93515; Clone = $computed; Name = 'Étoile divine'; IconPath = 'Interface\Icons\Spell_Holy_HolyGuidance'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts du Sacré.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 500 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagStar; 225 = 2; 226 = 0 } },
    @{ Id = 93516; Clone = $healHit; Name = 'Étoile divine'; IconPath = 'Interface\Icons\Spell_Holy_HolyGuidance'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 500 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagStar } },
    @{ Id = 93517; Clone = $computed; Name = 'Halo'; IconPath = 'Interface\Icons\Spell_Holy_Nullifydisease'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts du Sacré.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 900 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagStar; 225 = 2; 226 = 0 } },
    @{ Id = 93518; Clone = $healHit; Name = 'Halo'; IconPath = 'Interface\Icons\Spell_Holy_Nullifydisease'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 900 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagStar } },
    # Jeux d'esprit (Mindgames): a 1.5 s shadow bolt; mod-priest heals (93520) the most injured ally for what it dealt
    @{ Id = 93519; Clone = $shadowBolt; Name = "Jeux d'esprit"; IconPath = 'Interface\Icons\Spell_Shadow_Possession'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $shadowMagic; ClassMask = $classMask
       Description = "Assaille l'esprit de la cible : de lourds dégâts d'Ombre, et votre allié le plus blessé est soigné d'autant."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1200 })
       Fields = @{ 1 = 0; 204 = 3; 208 = 6; 209 = 0; 210 = 0; 211 = $flagMindgames } },
    @{ Id = 93520; Clone = $healHit; Name = "Jeux d'esprit"; IconPath = 'Interface\Icons\Spell_Shadow_Possession'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagMindgames } },

    # --- Discipline -------------------------------------------------------------------------------------------------
    # Mot de pouvoir : Radiance (Power Word: Radiance): a 2 s heal on an ally; mod-priest heals (93531) the 4 most
    # injured allies within 30 yd of it too, gives them all Atonement, and keeps its 2 charges (93507)
    @{ Id = 93530; Clone = $castHeal; Name = 'Mot de pouvoir : Radiance'; IconPath = 'Interface\Icons\Spell_Holy_HolyNova'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 18000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask
       Description = "Un éclat de lumière soigne la cible et les 4 alliés les plus blessés à moins de 30 m d'elle, et leur accorde l'Expiation pour 60% de sa durée. 2 charges."
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1500 })
       Fields = @{ 28 = 5; 131 = 3643; 204 = 10; 208 = 6; 209 = 0; 210 = 0; 211 = $flagRadiance } },
    @{ Id = 93531; Clone = $healHit; Name = 'Mot de pouvoir : Radiance'; IconPath = 'Interface\Icons\Spell_Holy_HolyNova'; FallbackIconSpell = 48078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1500 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagRadiance } },
    # Schisme (Schism): a 1.5 s shadow bolt; its mark (93533) makes the target take 15% more from the Priest for 9 s
    @{ Id = 93532; Clone = $shadowBolt; Name = 'Schisme'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowWordDominate'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 24000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask
       Description = "Fend l'esprit de la cible : dégâts d'Ombre, et elle subit 15% de dégâts en plus de votre part pendant 9 s."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1000 })
       Fields = @{ 1 = 0; 204 = 4; 208 = 6; 209 = 0; 210 = 0; 211 = $flagSchism } },
    @{ Id = 93533; Clone = 2983; Name = 'Schisme'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowWordDominate'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Subit davantage de dégâts du prêtre.'; AuraDescription = 'Subit 15% de dégâts en plus du prêtre.'
       Fields = @{ 40 = 581 } },
    # Purifier le mal (Purge the Wicked): fire at the target, then a burn every 2 s for 20 s; Penance spreads it
    @{ Id = 93534; Clone = 48125; Name = 'Purifier le mal'; IconPath = 'Interface\Icons\Spell_Fire_Immolation'; FallbackIconSpell = 589; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask
       Description = 'Brûle la cible de flammes sacrées : dégâts du Feu, puis toutes les 2 s pendant 20 s.'
       AuraDescription = 'Subit des dégâts du Feu toutes les 2 s.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 300 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 120 })
       Fields = @{ 40 = 18; 99 = 2000; 204 = 4; 208 = 6; 209 = 0; 210 = 0; 211 = $flagPurge; 225 = 4 } },
    # Mot de pouvoir : Barrière (Power Word: Barrier): allies within 10 yd of the Priest take 25% less for 10 s
    @{ Id = 93535; Clone = 48066; Name = 'Mot de pouvoir : Barrière'; IconPath = 'Interface\Icons\Spell_Holy_PowerWordBarrier'; FallbackIconSpell = 17; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask
       Description = 'Vos alliés à moins de 10 m de vous subissent 25% de dégâts en moins pendant 10 s.'
       AuraDescription = 'Dégâts subis réduits de 25%.'
       Effects = @(@{ Index = 0; Effect = 35; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -25; Misc = 127 })
       Fields = @{ 1 = 0; 2 = 0; 3 = 0; 27 = 0; 40 = 1; 46 = 1; 92 = 13; 204 = 4; 208 = 6; 209 = 0; 210 = 0; 211 = $flagDisciplineCooldowns } },
    # Torve-esprit (Mindbender): Shadowfiend's summon on a 1 min cooldown
    @{ Id = 93537; Clone = 34433; Name = 'Torve-esprit'; IconPath = 'Interface\Icons\Spell_Shadow_SoulLeech_3'; FallbackIconSpell = 34433; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask
       Description = "Invoque un Torve-esprit qui attaque la cible pendant 15 s et vous rend du mana. Ses coups comptent pour l'Expiation."
       Fields = @{ 211 = $flagDisciplineCooldowns } },
    # Ravissement (Rapture): Power Word: Shield absorbs 30% more for 8 s, its cooldown taken back (mod-priest)
    @{ Id = 93539; Clone = $selfBuff; Name = 'Ravissement'; IconPath = 'Interface\Icons\Spell_Holy_Rapture'; FallbackIconSpell = 47535; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask; NoEquipment = $true
       Description = "Pendant 8 s, Mot de pouvoir : Bouclier n'a plus de temps de recharge et absorbe 30% de plus."
       AuraDescription = "Mot de pouvoir : Bouclier n'a plus de temps de recharge et absorbe 30% de plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 30; Misc = 8 })
       Fields = @{ 1 = 0; 40 = 31; 41 = 0; 122 = $maskShield; 123 = 0; 124 = 0; 131 = 784; 204 = 3; 205 = 133; 206 = 1500; 208 = 6; 209 = 0; 210 = 0; 211 = $flagDisciplineCooldowns; 225 = 2 } },
    # Évangélisme (Evangelism): a self dummy; mod-priest extends every Atonement by 6 s
    @{ Id = 93540; Clone = $selfBuff; Name = 'Évangélisme'; IconPath = 'Interface\Icons\Spell_Holy_DivineIntervention'; FallbackIconSpell = 17; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $discipline; ClassMask = $classMask; NoEquipment = $true
       Description = 'Prolonge de 6 s votre Expiation sur tous vos alliés.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 0; 41 = 0; 131 = 8253; 204 = 2; 205 = 133; 206 = 1500; 208 = 6; 209 = 0; 210 = 0; 211 = $flagDisciplineCooldowns; 225 = 2 } },
    # Déferlante de lumière: Evangelism's instant heal on every atoned ally (amount set by mod-priest)
    @{ Id = 93541; Clone = $healHit; Name = 'Déferlante de lumière'; IconPath = 'Interface\Icons\Spell_Holy_SurgeOfLight'; FallbackIconSpell = 17; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },

    # --- Sacré ------------------------------------------------------------------------------------------------------
    # Mot sacré : Sérénité (Holy Word: Serenity): an instant big heal, 1 min; Flash Heal, Heal and Greater Heal bring it
    # 6 s closer (mod-priest)
    @{ Id = 93550; Clone = $castHeal; Name = 'Mot sacré : Sérénité'; IconPath = 'Interface\Icons\Spell_Holy_Heal02'; FallbackIconSpell = 2061; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = 'Soigne instantanément la cible alliée d''un montant considérable. Soins rapides, Soins et Soins supérieurs en réduisent le temps de recharge de 6 s.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 5000 })
       Fields = @{ 28 = 1; 204 = 5; 208 = 6; 209 = 0; 210 = 0; 211 = $flagSerenity } },
    # Mot sacré : Sanctification (Holy Word: Sanctify): the target and 5 injured allies within 10 yd of it (93556); Prayer
    # of Healing brings it 6 s closer, Renew 2 s
    @{ Id = 93551; Clone = $castHeal; Name = 'Mot sacré : Sanctification'; IconPath = 'Interface\Icons\Spell_Holy_DivineProvidence'; FallbackIconSpell = 596; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = 'Soigne instantanément la cible et les 5 alliés les plus blessés à moins de 10 m d''elle. Prière de soins en réduit le temps de recharge de 6 s, Rénovation de 2 s.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1800 })
       Fields = @{ 28 = 1; 131 = 8253; 204 = 6; 208 = 6; 209 = 0; 210 = 0; 211 = $flagSanctify } },
    # Mot sacré : Châtier (Holy Word: Chastise): holy damage and a 4 s incapacitation; Smite brings it 4 s closer
    @{ Id = 93552; Clone = 48123; Name = 'Mot sacré : Châtier'; IconPath = 'Interface\Icons\Spell_Holy_Excorcism_02'; FallbackIconSpell = 585; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask
       Description = 'Châtie la cible : lourds dégâts du Sacré, et elle est neutralisée 4 s. Châtiment en réduit le temps de recharge de 4 s.'
       AuraDescription = 'Neutralisé.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 1500 },
           @{ Index = 1; Effect = 6; Aura = 12; TargetA = 6 })
       Fields = @{ 28 = 1; 40 = 35; 84 = 12; 204 = 2; 208 = 6; 209 = 0; 210 = 0; 211 = $flagChastise } },
    # Écho de lumière (Echo of Light): Holy's mastery, a share of each heal over 6 s (amount set by mod-priest)
    @{ Id = 93553; Clone = 48068; Name = 'Écho de lumière'; IconPath = 'Interface\Icons\Spell_Holy_Aspiration'; FallbackIconSpell = 139; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible toutes les 2 s.'; AuraDescription = 'Rend des points de vie toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 8; TargetA = 21; Value = 1 })
       Fields = @{ 2 = 0; 40 = 32; 46 = 6; 98 = 2000; 131 = 0; 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },
    # Apothéose (Apotheosis): 20 s of Holy Words free (the cost modifier) and recharging four times as fast (mod-priest)
    @{ Id = 93554; Clone = $selfBuff; Name = 'Apothéose'; IconPath = 'Interface\Icons\Spell_Holy_AuraMastery'; FallbackIconSpell = 10060; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $holy; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pendant 20 s, vos Mots sacrés se rechargent quatre fois plus vite et ne coûtent pas de mana.'
       AuraDescription = 'Mots sacrés rechargés quatre fois plus vite, et sans coût.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 1 = 0; 40 = 18; 41 = 0; 122 = 0; 123 = 0; 124 = ($flagSerenity -bor $flagSanctify -bor $flagChastise); 131 = 7553; 204 = 3; 205 = 133; 206 = 1500; 208 = 6; 209 = 0; 210 = 0; 211 = $flagApotheosis; 225 = 2 } },
    # Tisse-lumière (Lightweaver): the next Greater Heal 30% faster and 15% stronger per stack, 2 stacks
    @{ Id = 93555; Clone = 2983; Name = 'Tisse-lumière'; IconPath = 'Interface\Icons\Spell_Holy_GreaterHeal'; FallbackIconSpell = 2060; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Soins supérieurs renforcé.'; AuraDescription = "Votre prochain Soins supérieurs se lance 30% plus vite et soigne 15% de plus par charge."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -30; Misc = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 15; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 40 = 8; 122 = $maskGreaterHeal; 123 = 0; 124 = 0; 125 = $maskGreaterHeal; 126 = 0; 127 = 0; 208 = 6 } },
    @{ Id = 93556; Clone = $healHit; Name = 'Mot sacré : Sanctification'; IconPath = 'Interface\Icons\Spell_Holy_DivineProvidence'; FallbackIconSpell = 596; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1800 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagSanctify } },
    # Ondulation cosmique (Cosmic Ripple) and Sillage de lumière (Trail of Light)
    @{ Id = 93557; Clone = $healHit; Name = 'Ondulation cosmique'; IconPath = 'Interface\Icons\Spell_Holy_DivineHymn'; FallbackIconSpell = 64843; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 600 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 93558; Clone = $healHit; Name = 'Sillage de lumière'; IconPath = 'Interface\Icons\Spell_Holy_FlashHeal'; FallbackIconSpell = 2061; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = @{ 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0 } },

    # --- Ombre ------------------------------------------------------------------------------------------------------
    # Devouring Plague's instant hit, as the plague lands (mod-priest; the stock ranks carry the DoT)
    @{ Id = 93560; Clone = $computed; Name = 'Peste dévorante'; IconPath = 'Interface\Icons\Spell_Shadow_BlackPlague'; FallbackIconSpell = 2944; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 500 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagDevouring; 225 = 32; 226 = 0 } },
    # Éclair du Vide (Void Bolt): only in Voidform (mod-priest), 6 s, 12 Insanity; the target's Shadow Word: Pain and
    # Vampiric Touch last 3 s longer
    @{ Id = 93561; Clone = $shadowBolt; Name = 'Éclair du Vide'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowBolt'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 6000; Level = 1; Spellbook = $true; SkillLine = $shadowMagic; ClassMask = $classMask
       Description = "Utilisable en Forme du Vide. Un éclair du Vide qui inflige des dégâts d'Ombre, vous rend 12 points de Démence et prolonge de 3 s votre Mot de l'ombre : Douleur et votre Toucher vampirique sur la cible."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1000 })
       Fields = @{ 1 = 0; 28 = 1; 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagVoidBolt } },
    # Intuition ténébreuse (Shadowy Insight): the next Mind Blast instant
    @{ Id = 93562; Clone = 2983; Name = 'Intuition ténébreuse'; IconPath = 'Interface\Icons\Spell_Shadow_SiphonMana'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Attaque mentale instantanée.'; AuraDescription = 'Votre prochaine Attaque mentale est instantanée.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = @{ 40 = 8; 122 = $maskMindBlast; 123 = 0; 124 = 0; 208 = 6 } },
    # Éruption du Vide (Void Eruption): the target, and mod-priest's splash (93564) on the enemies within 10 yd; then
    # Voidform (93565)
    @{ Id = 93563; Clone = $shadowBolt; Name = 'Éruption du Vide'; IconPath = 'Interface\Icons\Spell_Shadow_SealOfKings'; FallbackIconSpell = 15473; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $shadowMagic; ClassMask = $classMask
       Description = "Libère le Vide sur la cible et les ennemis proches, puis vous entrez en Forme du Vide pendant 20 s : dégâts +20%, hâte des sorts +10%, et Éclair du Vide devient utilisable."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1500 })
       Fields = @{ 1 = 0; 28 = 1; 131 = 8069; 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagEruption } },
    @{ Id = 93564; Clone = $computed; Name = 'Éruption du Vide'; IconPath = 'Interface\Icons\Spell_Shadow_SealOfKings'; FallbackIconSpell = 15473; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagEruption; 225 = 32; 226 = 0 } },
    @{ Id = 93565; Clone = 2983; Name = 'Forme du Vide'; IconPath = 'Interface\Icons\Spell_Shadow_SealOfKings'; FallbackIconSpell = 15473; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts et hâte augmentés.'; AuraDescription = "Dégâts infligés augmentés de 20%, hâte des sorts de 10%. Éclair du Vide est utilisable."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 20; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 216; TargetA = 1; Value = 10 })
       Fields = @{ 40 = 18 } },
    # Ascension sombre (Dark Ascension): 30 Insanity at once, periodic damage 25% higher for 20 s (mod-priest)
    @{ Id = 93567; Clone = $selfBuff; Name = 'Ascension sombre'; IconPath = 'Interface\Icons\Spell_Shadow_Charm'; FallbackIconSpell = 15473; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $shadowMagic; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous gagnez aussitôt 30 points de Démence, et vos dégâts périodiques augmentent de 25% pendant 20 s.'
       AuraDescription = 'Dégâts périodiques augmentés de 25%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 18; 41 = 0; 131 = 3619; 204 = 0; 205 = 133; 206 = 1500; 208 = 6; 209 = 0; 210 = 0; 211 = 0; 225 = 32 } },
    # Apparitions ténébreuses (Shadowy Apparitions) and Lien psychique (Psychic Link): amounts set by mod-priest
    @{ Id = 93568; Clone = $computed; Name = 'Apparition ténébreuse'; IconPath = 'Interface\Icons\Spell_Shadow_Haunting'; FallbackIconSpell = 2944; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0; 225 = 32; 226 = 0 } },
    # Fracas des ombres (Shadow Crash): Shadowfury's layout, the enemies within 8 yd of the chosen spot; mod-priest puts
    # Vampiric Touch on 5 of them (9 with Ombres murmurantes)
    @{ Id = 93569; Clone = 47847; Name = 'Fracas des ombres'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowFury'; FallbackIconSpell = 47847; Cost = 0; Cooldown = 15000; Level = 1; Spellbook = $true; SkillLine = $shadowMagic; ClassMask = $classMask
       Description = "Projette une masse d'ombre à l'endroit visé : dégâts d'Ombre aux ennemis dans un rayon de 8 m, et votre Toucher vampirique sur 5 d'entre eux."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 16; Value = 800 })
       Fields = @{ 1 = 0; 2 = 0; 40 = 0; 46 = 5; 92 = 14; 204 = 0; 206 = 1500; 208 = 6; 209 = 0; 210 = 0; 211 = $flagShadowCrash } },
    # Torrent du Vide (Void Torrent): a 3 s channel on the target, a tick (93571) every second, 8 Insanity each
    @{ Id = 93570; Clone = 5143; Name = 'Torrent du Vide'; IconPath = 'Interface\Icons\Spell_Shadow_MindRot'; FallbackIconSpell = 15407; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $shadowMagic; ClassMask = $classMask
       Description = "Canalise un torrent du Vide sur la cible pendant 3 s : dégâts d'Ombre chaque seconde, et 8 points de Démence à chaque fois."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 },
           @{ Index = 1; Effect = 6; Aura = 23; TargetA = 1 })
       Fields = @{ 40 = 27; 41 = 0; 46 = 4; 99 = 1000; 117 = 93571; 131 = 12637; 204 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = $flagVoidTorrent; 225 = 32 } },
    @{ Id = 93571; Clone = 58381; Name = 'Torrent du Vide'; IconPath = 'Interface\Icons\Spell_Shadow_MindRot'; FallbackIconSpell = 15407; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 77; Value = 900 })
       Fields = @{ 208 = 6; 209 = 0; 210 = 0; 211 = $flagVoidTorrent; 225 = 32 } },
    @{ Id = 93572; Clone = $computed; Name = 'Lien psychique'; IconPath = 'Interface\Icons\Spell_Shadow_MindTwisting'; FallbackIconSpell = 8092; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = @{ 41 = 0; 46 = 6; 47 = 0; 131 = 0; 208 = 6; 209 = 0; 210 = 0; 211 = 0; 225 = 32; 226 = 0 } },
    # Dévoreur d'esprit (Mind Devourer), Fouet mental : Démence, Porte-mort (Deathspeaker): procs mod-priest reads
    @{ Id = 93573; Clone = 2983; Name = "Dévoreur d'esprit"; IconPath = 'Interface\Icons\Spell_Shadow_UnholyFrenzy'; FallbackIconSpell = 2944; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Peste dévorante gratuite.'; AuraDescription = 'Votre prochaine Peste dévorante ne coûte pas de Démence.'
       Fields = @{ 40 = 8 } },
    @{ Id = 93574; Clone = 2983; Name = 'Fouet mental : Démence'; IconPath = 'Interface\Icons\Spell_Shadow_SiphonMana'; FallbackIconSpell = 15407; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Fouet mental renforcé.'; AuraDescription = 'Votre prochain Fouet mental inflige 50% de dégâts en plus.'
       Fields = @{ 40 = 18 } },
    @{ Id = 93575; Clone = 2983; Name = 'Porte-mort'; IconPath = 'Interface\Icons\Spell_Shadow_DeathScream'; FallbackIconSpell = 32379; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Mot de l'ombre : Mort renforcé."; AuraDescription = "Votre prochain Mot de l'ombre : Mort frappe comme sur une cible sous 20% de vie."
       Fields = @{ 40 = 8 } },

    # --- Spec passives ----------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-priest also reads them to know which one
    # is on.
    # Discipline: Atonement (mod-priest); Power Word: Shield absorbs 20% more (word 0 0x1, all effects), Penance's bolts
    # (word 1 0x18000) deal and heal 15% more, Smite (word 0 0x80) deals 30% more
    @{ Id = 93680; Clone = 2983; Name = 'Discipline'; IconPath = 'Interface\Icons\Spell_Holy_PowerWordShield'; FallbackIconSpell = 17; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Mot de pouvoir : Bouclier, Mot de pouvoir : Radiance, Soins rapides et Rénovation accordent l'Expiation : vos dégâts soignent alors l'allié qui la porte de 35% de leur montant. Mot de pouvoir : Bouclier absorbe 20% de plus, Pénitence inflige et soigne 15% de plus, et Châtiment inflige 30% de dégâts en plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 20; Misc = 8 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 15; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 2; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 30; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = $maskShield; 123 = 0; 124 = 0; 125 = 0; 126 = 0x18000; 127 = 0; 128 = 0x80; 129 = 0; 130 = 0; 208 = 6 } },
    # Holy: Echo of Light (mod-priest) and 10% more healing; Smite and Holy Fire (word 0 0x100080) deal 25% more
    @{ Id = 93681; Clone = 2983; Name = 'Sacré'; IconPath = 'Interface\Icons\Spell_Holy_GuardianSpirit'; FallbackIconSpell = 2060; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Écho de lumière : vos soins soignent aussi la cible de 20% de leur montant en 6 s. Vos soins augmentent de 10%, Châtiment et Flammes sacrées infligent 25% de dégâts en plus, et vos soins réduisent le temps de recharge de vos Mots sacrés."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 136; TargetA = 1; Value = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 25; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 125 = 0x100080; 126 = 0; 127 = 0; 208 = 6 } },
    # Shadow: Insanity (mod-priest), 10% more shadow damage, and the core spells (Mind Blast, Shadow Word: Pain, Vampiric
    # Touch, Shadow Word: Death, Mind Flay) cost half their mana
    @{ Id = 93682; Clone = 2983; Name = 'Ombre'; IconPath = 'Interface\Icons\Spell_Shadow_Shadowform'; FallbackIconSpell = 15473; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Vous combattez avec la Démence : Attaque mentale, Toucher vampirique, Mot de l'ombre : Douleur, Fouet mental et vos autres sorts d'Ombre la remplissent, Peste dévorante en consomme 50. Vos dégâts d'Ombre augmentent de 10%, et vos sorts d'Ombre principaux coûtent moitié moins de mana."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 32 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -50; Misc = $SPELLMOD_COST })
       Fields = @{ 125 = ($maskMindBlast -bor 0x8000); 126 = 0x402; 127 = 0x40; 208 = 6 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-priest reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\priest\talentTree.json') -Family 6

return $spells
