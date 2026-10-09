# The Rogue's spell data for its retail-style talent trees (localTools/rogue/talentTree.json): the abilities its
# nodes teach, the auras its scripted talents show, and the rank spells of its new talents. Behaviour lives in
# modules/mod-rogue; this file owns client/server spell data only. Ids 92190-92399: 92190-92199 spec passives,
# 92200-92299 talent ranks (generated from the tree), 92300-92399 abilities and auras. The Combat tab is Hors-la-loi,
# retail's Outlaw (.agents/plans/outlaw-rogue): its kit 92342-92351, its procs and Roll the Bones' buffs 92352-92369;
# its tree's ranks ran past 92299, so 92306-92308 are generated talent ranks too (Entailleur rapide 2, Fioriture
# fatale 1-2).
#
# Clone field notes: 1 category, 2 dispel type, 3 mechanic, 28 casting time index (1 instant), 40 duration index (1
# 10 s, 8 15 s, 18 20 s, 21 never, 27 3 s, 31 8 s, 32 6 s), 46 range index (1 self, 2 melee, 4 30 yd), 92-94 radius
# index (13 10 yd), 131 visual, 205-206 global cooldown category and time, 208 family (8 Rogue), 209-211 family flags,
# 213 damage class (0 none: it cannot miss), 225 school.
# More for Hors-la-loi: 3 mechanic (11 snare), 36 charges, 40 duration index (9 30 s, 28 5 s, 39 2 s, 187 1 s per
# combo point: 0 to 5 s), 46 range index (3 20 yd, 7 10 yd), 47 missile speed (float bits), 49 stack (0 shows the
# charges), 83-85 the effects' mechanic (11 snare, 12 stun), 98 periodic interval, 213 damage class (3 ranged: missed or
# deflected, never dodged or parried).

# Finesse's retail looks (localTools/rogue/ascensionVisuals.json): Look gives an imported look's id for field 131
. (Join-Path $repoRoot 'localTools\rogue\Looks.ps1')

$classMask = 8
$assassination = 253
$combat = 38
$subtlety = 39

# Hors-la-loi's own family flags, word 2 (field 211): no stock Rogue spell carries one (stock modifiers name 0x1 and
# 0x10 there, so those stay unused). Œil de lynx's range modifier names Pistol Shot's and Between the Eyes',
# Opportunité's cost modifier Pistol Shot's.
$flagPistolShot = 0x2
$flagBetweenTheEyes = 0x4
$flagDispatch = 0x8
$flagRollTheBones = 0x20
$flagBladeFlurry = 0x40
$flagAdrenalineRush = 0x80
$flagBladeRush = 0x100
$flagKillingSpree = 0x200
$flagKeepItRolling = 0x400
$flagDreadblades = 0x800
# The bullet's flight, yards a second (Deadly Throw's)
$bulletSpeed = [BitConverter]::ToUInt32([BitConverter]::GetBytes([single]50), 0)

$spells = @(
    # --- Spec passives --------------------------------------------------------------------------------------------
    # Sinister Strike costs nothing in the data (patchSinisterStrike.ps1, since the old Combat rework); every spec
    # learns this with its specialization, which puts the stock 45 Energy back. Word 0's 0x2 is Sinister Strike's own
    # flag.
    @{ Id = 92191; Clone = 2983; Name = 'Frappe sinistre'; IconPath = 'Interface\Icons\Spell_Shadow_RitualOfSacrifice'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Frappe sinistre coûte 45 points d'énergie."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 45; Misc = $SPELLMOD_COST })
       Fields = @{ 122 = 0x2; 208 = 8 } },
    # Assassinat's own passive: Cold Blood's cooldown 3 min -> 30 s (word 1 0x40), and the mark mod-rogue reads for the
    # spec's damage over time (Envenom spreading the bleeds, Fan of Knives' poisons, Virulence)
    @{ Id = 92192; Clone = 2983; Name = 'Maître des toxines'; IconPath = 'Interface\Icons\Ability_Rogue_DeviousPoisons'; FallbackIconSpell = 1329; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Rupture et Garrot infligent 150% de dégâts en plus. Envenimer et Tempête cramoisie propagent vos saignements, Éventail de couteaux empoisonne chaque ennemi touché et rapporte un point de combo par ennemi, et chacune de vos afflictions augmente vos dégâts. Sang-froid : 30 s de recharge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -150000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0; 123 = 0x40; 124 = 0; 208 = 8 } },
    # Finesse's own passive: Shadow Dance's cooldown 1 min -> 30 s (word 1 0x2000000), and the mark mod-rogue reads for
    # the spec's damage (Eviscerate +120%, Backstab, Ambush and Hemorrhage +50%, a builder's critical strike adds a
    # combo point)
    @{ Id = 92193; Clone = 2983; Name = 'Danseur des ombres'; IconPath = 'Interface\Icons\Ability_Rogue_ShadowDance'; FallbackIconSpell = 51713; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Éviscération inflige 120% de dégâts en plus, Attaque sournoise, Embuscade et Hémorragie 50% de plus, et le coup critique d'une technique qui génère des points de combo en rapporte un de plus. Danse de l'ombre : 30 s de recharge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -30000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0; 123 = 0x2000000; 124 = 0; 208 = 8 } },
    # Hors-la-loi's own passive: the mark mod-rogue reads (IsOutlaw) for the spec's built-in talents - Restless Blades,
    # Combat Potency, Ruthlessness - and Sinister Strike's second strike that grants Opportunité
    @{ Id = 92194; Clone = 2983; Name = 'Hors-la-loi'; IconPath = 'Interface\Icons\ability_rogue_restlessblades'; FallbackIconSpell = 13750; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Frappe sinistre a 35% de chances de frapper une seconde fois, ce qui rapporte 1 point de combo de plus et vous accorde Opportunité. Lames sans repos : chaque coup de grâce réduit de 1 s par point de combo dépensé le temps de recharge de Poussée d'adrénaline, Entre les deux yeux, Déluge de lames, Jeter les os, Frappe fantomatique, Ruée des lames, Série meurtrière, Rejouer, Lames d'effroi, Sprint et Disparition. Potentiel de combat : les coups de votre main gauche ont 20% de chances de vous rendre 8 points d'énergie. Cruauté : vos coups de grâce ont 20% de chances par point de combo de vous rendre un point de combo."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 208 = 8 } },

    # --- Class tree -----------------------------------------------------------------------------------------------
    # Thé au chardon: the Thistle Tea's 100 Energy, off the global cooldown, on its own 1 min cooldown (no potion
    # category)
    @{ Id = 92310; Clone = 9512; Name = 'Thé au chardon'; IconPath = 'Interface\Icons\INV_Drink_Milk_02'; FallbackIconSpell = 9512; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = "Restaure instantanément 100 points d'énergie."
       Effects = @(@{ Index = 0; Effect = 30; TargetA = 1; Value = 100; Misc = 3 })
       Fields = @{ 1 = 0; 41 = 3; 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Marqué pour la mort: 5 combo points on the target at 30 yd, Hunter's Mark over its head. mod-rogue resets the
    # cooldown when the target dies within the minute.
    @{ Id = 92311; Clone = 1766; Name = 'Marqué pour la mort'; IconPath = 'Interface\Icons\Ability_Hunter_Assassinate'; FallbackIconSpell = 1130; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = 'Marque la cible et lui ajoute 5 points de combo. Si elle meurt dans la minute, le temps de recharge est réinitialisé.'
       Effects = @(@{ Index = 0; Effect = 80; TargetA = 6; Value = 5 })
       Fields = @{ 1 = 0; 3 = 0; 46 = 4; 131 = (Look 'MarkedForDeath'); 205 = 133; 206 = 1000; 208 = 8; 209 = 0; 210 = 0; 211 = 0; 213 = 0 } },
    @{ Id = 92300; Clone = 2983; Name = 'Feinte esquive'; IconPath = 'Interface\Icons\Ability_Rogue_Feint'; FallbackIconSpell = 1966; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits de 25%.'; AuraDescription = 'Dégâts subis réduits de 25%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 87; TargetA = 1; Value = -25; Misc = 127 }); Fields = @{ 40 = 32 } },
    @{ Id = 92301; Clone = 2983; Name = 'Évasion améliorée'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowWard'; FallbackIconSpell = 5277; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts des sorts subis réduits de 25%.'; AuraDescription = 'Dégâts des sorts subis réduits de 25%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 87; TargetA = 1; Value = -25; Misc = 126 }); Fields = @{ 40 = 8 } },
    @{ Id = 92302; Clone = 2983; Name = 'Vivacité'; IconPath = 'Interface\Icons\Ability_Rogue_SliceDice'; FallbackIconSpell = 5171; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Hâte augmentée.'; AuraDescription = 'Vitesse d''attaque augmentée de 1% par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 1 }); Fields = @{ 40 = 8 } },
    @{ Id = 92303; Clone = 2983; Name = "Élan de l'ombre"; IconPath = 'Interface\Icons\Ability_Vanish'; FallbackIconSpell = 1856; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vitesse de déplacement augmentée de 30%.'; AuraDescription = 'Vitesse de déplacement augmentée de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 1; Value = 30 }); Fields = @{ 40 = 27 } },
    @{ Id = 92304; Clone = 2983; Name = 'Maître assassin'; IconPath = 'Interface\Icons\Ability_Rogue_MasterOfSubtlety'; FallbackIconSpell = 31223; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Chances de coup critique augmentées de 30%.'; AuraDescription = 'Chances de coup critique augmentées de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModCritPct; TargetA = 1; Value = 30 }); Fields = @{ 40 = 27 } },
    # Poison sangsue: never cast, it names the heal in the combat log and the meters
    @{ Id = 92305; Clone = 2983; Name = 'Poison sangsue'; IconPath = 'Interface\Icons\Spell_Shadow_LifeDrain02'; FallbackIconSpell = 2818; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vos attaques vous soignent.'; Fields = @{ 225 = 8 } },

    # --- Assassinat -----------------------------------------------------------------------------------------------
    # Vendetta: a 10 s mark on the enemy (Hunter's Mark's layout, a dummy aura mod-rogue reads) and 40 Energy back, every
    # 30 s: no long cooldowns for the rogue
    @{ Id = 92312; Clone = 1130; Name = 'Vendetta'; IconPath = 'Interface\Icons\Ability_Rogue_FeignDeath'; FallbackIconSpell = 1943; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = "Marque la cible pendant 10 s : vos attaques lui infligent 20% de dégâts en plus, et vous récupérez 40 points d'énergie."
       AuraDescription = 'Subit 20% de dégâts en plus du voleur.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }, @{ Index = 1; Effect = 30; TargetA = 1; Value = 40; Misc = 3 })
       Fields = @{ 2 = 0; 40 = 1; 41 = 3; 46 = 4; 131 = (Look 'Vendetta'); 205 = 133; 206 = 1000; 208 = 8; 209 = 0; 210 = 0; 211 = 0; 213 = 0; 225 = 1 } },
    # Exsanguiner: a melee-range dummy; mod-rogue deals the rest of the caster's Rupture and Garrote at once
    @{ Id = 92313; Clone = 1766; Name = 'Exsanguiner'; IconPath = 'Interface\Icons\Spell_DeathKnight_BloodBoil'; FallbackIconSpell = 1943; Cost = 25; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = 'Vos Rupture et Garrot sur la cible lui infligent sur-le-champ tous leurs dégâts restants.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 1 = 0; 3 = 0; 131 = (Look 'Exsanguinate'); 205 = 133; 206 = 1000; 208 = 8; 209 = 0; 210 = 0; 211 = 0; 213 = 0 } },

    # Tempête cramoisie: a finisher on the target (it spends the combo points there), a dummy mod-rogue turns into a slash
    # and a bleed on every enemy within 10 yd, longer with every combo point; retail's crimson spin around the rogue
    @{ Id = 92330; Clone = 48668; Name = 'Tempête cramoisie'; IconPath = 'Interface\Icons\Ability_Rogue_BloodSplatter'; FallbackIconSpell = 48672; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = "Coup de grâce qui propage la Rupture et le Garrot de votre cible à tous les ennemis à 10 m, puis les entaille et les fait saigner pendant 2 s de plus par point de combo, jusqu'à 12 s ; moins sur chacun au-delà de 5 ennemis."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 131 = (Look 'CrimsonTempest'); 209 = 0; 210 = 0; 211 = 0 } },
    # Its bleed: amount and length set by mod-rogue for the combo points spent; one tick every 2 s. A slash on each enemy
    # it lands on, without Rupture's swing (the rogue would swing again for every enemy)
    @{ Id = 92331; Clone = 48672; Name = 'Tempête cramoisie'; IconPath = 'Interface\Icons\Ability_Rogue_BloodSplatter'; FallbackIconSpell = 48672; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Saigne.'; AuraDescription = 'Saigne.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 40 = 8; 98 = 2000; 131 = (Look 'CrimsonBleed'); 209 = 0; 210 = 0; 211 = 0 } },
    # Virulence: 2% damage per affliction of the rogue on its enemies, up to 15 (mod-rogue keeps the count)
    @{ Id = 92332; Clone = 2983; Name = 'Virulence'; IconPath = 'Interface\Icons\Ability_Rogue_DeviousPoisons'; FallbackIconSpell = 2818; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 15; Spellbook = $false
       Description = 'Dégâts augmentés.'; AuraDescription = 'Dégâts augmentés de 2% par saignement ou poison que vous entretenez sur vos ennemis.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 2; Misc = 127 }); Fields = @{ 40 = 21 } },

    # --- Finesse --------------------------------------------------------------------------------------------------
    # Symboles de mort: 15% damage for 10 s and 40 Energy; the deathmark on the chest, red symbols at the hands while it
    # lasts. Pure data.
    @{ Id = 92320; Clone = 14177; Name = 'Symboles de mort'; IconPath = 'Interface\Icons\Spell_Shadow_DeathScream'; FallbackIconSpell = 14177; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Vos dégâts augmentent de 15% pendant 10 s et vous récupérez 40 points d'énergie."
       AuraDescription = 'Dégâts augmentés de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 15; Misc = 127 }, @{ Index = 1; Effect = 30; TargetA = 1; Value = 40; Misc = 3 })
       Fields = @{ 34 = 0; 35 = 0; 36 = 0; 40 = 1; 41 = 3; 131 = (Look 'SymbolsOfDeath'); 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Tempête de shurikens: Fan of Knives at 10 yd for 35 Energy; mod-rogue sets its hit to 180% of the attack power (40% of it on fewer than 3 enemies), adds a combo point for the first enemy hit and
    # two for every other one, up to 5 (Shadow Blades: one more). Retail's storm and shuriken tosses.
    @{ Id = 92321; Clone = 51723; Name = 'Tempête de shurikens'; IconPath = 'Interface\Icons\Ability_Rogue_FanOfKnives'; FallbackIconSpell = 51723; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask; NoEquipment = $true
       Description = "Projette des shurikens sur tous les ennemis à 10 m : dégâts égaux à 168% des dégâts de l'arme (40% de ceux-ci contre moins de 3 ennemis, moins au-delà de 5), 1 point de combo pour le premier ennemi touché et 2 pour chacun des suivants, jusqu'à 5."
       Fields = @{ 92 = 13; 131 = (Look 'ShurikenStorm'); 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Lames de l'ombre: a 10 s dummy buff, shadow on both weapons while it lasts, every 30 s; mod-rogue adds the damage
    # and combo points
    @{ Id = 92322; Clone = 51713; Name = "Lames de l'ombre"; IconPath = 'Interface\Icons\Spell_Shadow_ShadowWordDominate'; FallbackIconSpell = 51713; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Pendant 10 s, vos attaques automatiques infligent 50% de dégâts en plus et vos techniques génèrent 1 point de combo de plus."
       AuraDescription = "Attaques automatiques : 50% de dégâts en plus. Techniques : 1 point de combo de plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 40 = 1; 131 = (Look 'ShadowBlades'); 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Poudre noire: a finisher on the target; mod-rogue deals shadow damage to every enemy within 10 yd, more for every
    # enemy beyond the first (up to two) and in Shadow Dance, and gives energy back for the enemies beyond the first; a
    # shadow nova around the rogue (mod-rogue plays the hit on every enemy)
    @{ Id = 92340; Clone = 48668; Name = 'Poudre noire'; IconPath = 'Interface\Icons\Spell_Shadow_Shadowfury'; FallbackIconSpell = 30283; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Coup de grâce qui inflige des dégâts d'Ombre à tous les ennemis à 10 m, plus par point de combo : 25% de plus par ennemi touché au-delà du premier, jusqu'à 50%, et 25% de plus pendant Danse de l'ombre ; moins sur chacun au-delà de 5 ennemis. Vous rend 6 points d'énergie par ennemi touché au-delà du premier, jusqu'à 18."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 131 = (Look 'BlackPowder'); 209 = 0; 210 = 0; 211 = 0; 225 = 32 } },
    # Technique secrète: a finisher every 30 s; mod-rogue strikes every enemy within 10 yd three times, the rogue (Goremaw's
    # Bite's look) and two shadows of it, summoned beside the target
    @{ Id = 92341; Clone = 48668; Name = 'Technique secrète'; IconPath = 'Interface\Icons\Ability_Rogue_ShadowStrikes'; FallbackIconSpell = 51713; Cost = 30; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Coup de grâce : vous et deux ombres de vous frappez votre cible et, pour moitié, tous les ennemis à 10 m, plus fort par point de combo ; moins sur chacun au-delà de 5 ennemis."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 131 = (Look 'SecretTechnique'); 209 = 0; 210 = 0; 211 = 0; 225 = 32 } },
    # Ombre: Technique secrète's shadows wear it (mod-rogue), Shadowform's dark see-through skin over a mist
    @{ Id = 92327; Clone = 2983; Name = 'Ombre'; IconPath = 'Interface\Icons\Ability_Rogue_ShadowStrikes'; FallbackIconSpell = 51713; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Une ombre du voleur.'; AuraDescription = 'Une ombre du voleur.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 }); Fields = @{ 40 = 21; 131 = (Look 'ShadowClone') } },
    # Coup dans le noir: the next Cheap Shot costs nothing (one charge, spent by the cast)
    @{ Id = 92323; Clone = 2983; Name = 'Coup dans le noir'; IconPath = 'Interface\Icons\Ability_CheapShot'; FallbackIconSpell = 1833; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochain Coup bas ne coûte pas d''énergie.'; AuraDescription = 'Prochain Coup bas gratuit.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 36 = 1; 40 = 21; 122 = 0x400; 208 = 8 } },
    @{ Id = 92324; Clone = 2983; Name = 'Poignards profonds'; IconPath = 'Interface\Icons\INV_Weapon_ShortBlade_14'; FallbackIconSpell = 53; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Dégâts augmentés.'; AuraDescription = 'Dégâts augmentés de 4% par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 4; Misc = 127 }); Fields = @{ 40 = 31 } },
    @{ Id = 92325; Clone = 2983; Name = 'Terreurs nocturnes'; IconPath = 'Interface\Icons\Spell_Shadow_PsychicScream'; FallbackIconSpell = 3409; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vitesse de déplacement réduite de 30%.'; AuraDescription = 'Vitesse de déplacement réduite de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 33; TargetA = 1; Value = -30 }); Fields = @{ 3 = 11; 40 = 31 } },
    # Focalisation des ombres: held by mod-rogue while the rogue is stealthed or dancing
    @{ Id = 92326; Clone = 2983; Name = 'Focalisation des ombres'; IconPath = 'Interface\Icons\Ability_Stealth'; FallbackIconSpell = 1784; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Vos techniques coûtent 20% d'énergie en moins."; AuraDescription = "Techniques : 20% d'énergie en moins."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -20; Misc = $SPELLMOD_COST })
       Fields = @{ 40 = 21; 122 = 0xFFFFFFFF; 123 = 0xFFFFFFFF; 124 = 0xFFFFFFFF; 208 = 8 } },

    # --- Hors-la-loi (retail's Outlaw) ----------------------------------------------------------------------------
    # The kit is learned with the specialization (talentTree.json specSpells), the four other abilities through the
    # tree. Damage is mod-rogue's: every strike below lands as a dummy (effect 0) the module turns into a hit, so only
    # its target, range, cost, cooldown, combo points and global cooldown are data. Each carries its own family flag
    # (word 2), every inherited one cleared.
    #
    # Tir de pistolet: a bullet at 20 yd (Sinister Strike's builder layout, a ranged hit: missed or deflected, never
    # dodged or parried). Effect 0 the hit (mod-rogue: 130% of a main-hand hit), effect 1 the combo point, effect 2 the
    # 50% snare for 6 s. Opportunité makes it free (its own cost modifier).
    @{ Id = 92342; Clone = 48638; Name = 'Tir de pistolet'; IconPath = 'Interface\Icons\ability_rogue_pistolshot'; FallbackIconSpell = 26679; Cost = 40; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Tire sur l'ennemi à 20 m : dégâts physiques égaux à 130% d'un coup de votre arme principale, 1 point de combo, et sa vitesse de déplacement est réduite de 50% pendant 6 s."
       AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(
           @{ Index = 0; Effect = 3; TargetA = 6 },
           @{ Index = 1; Effect = 80; TargetA = 6; Value = 1 },
           @{ Index = 2; Effect = 6; Aura = 33; TargetA = 6; Value = -50 })
       Fields = @{ 1 = 0; 3 = 0; 40 = 32; 46 = 3; 47 = $bulletSpeed; 83 = 0; 84 = 0; 85 = 11; 131 = (Look 'OutlawPistolShot'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagPistolShot; 213 = 3; 225 = 1 } },
    # Achever (Dispatch): Eviscerate's finisher layout at melee range; mod-rogue deals Eviscerate's damage for the combo
    # points spent, times its own factor
    @{ Id = 92343; Clone = 48668; Name = 'Achever'; IconPath = 'Interface\Icons\Ability_Rogue_Waylay'; FallbackIconSpell = 48668; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Coup de grâce qui transperce la cible : dégâts physiques, plus élevés par point de combo."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 131 = (Look 'OutlawDispatch'); 209 = 0; 210 = 0; 211 = $flagDispatch; 225 = 1 } },
    # Entre les deux yeux: a finisher shot at 20 yd every 45 s. Effect 0 the hit (mod-rogue), effect 1 the stun, 1 s
    # per combo point (Kidney Shot's duration, 0 to 5 s by the points spent); mod-rogue adds the critical strike buff
    # (92364)
    @{ Id = 92344; Clone = 48668; Name = 'Entre les deux yeux'; IconPath = 'Interface\Icons\INV_Weapon_Rifle_01'; FallbackIconSpell = 26679; Cost = 25; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Coup de grâce : une balle entre les deux yeux de l'ennemi, à 20 m. Inflige des dégâts physiques et étourdit la cible 1 s par point de combo, et vos chances de coup critique augmentent de 20% pendant 3 s par point de combo."
       AuraDescription = 'Étourdi.'
       Effects = @(
           @{ Index = 0; Effect = 3; TargetA = 6 },
           @{ Index = 1; Effect = 6; Aura = 12; TargetA = 6 })
       Fields = @{ 40 = 187; 46 = 3; 47 = $bulletSpeed; 83 = 0; 84 = 12; 85 = 0; 131 = (Look 'OutlawBetweenTheEyes'); 209 = 0; 210 = 0; 211 = $flagBetweenTheEyes; 213 = 3; 225 = 1 } },
    # Jeter les os: a self cast every 45 s, no aura of its own; mod-rogue takes the current buffs away and grants one or
    # two of 92354-92359
    @{ Id = 92345; Clone = 2983; Name = 'Jeter les os'; IconPath = 'Interface\Icons\ability_rogue_rollthebones'; FallbackIconSpell = 2983; Cost = 25; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Lancez les os : vos effets de Jeter les os disparaissent et vous en gagnez un au hasard pendant 30 s, parfois deux (25% de chances) - Bordée, Trésor enfoui, Grande mêlée, Précision impitoyable, Tête de mort ou Cap assuré."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 9; 131 = (Look 'OutlawRollTheBones'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagRollTheBones; 213 = 0 } },
    # Déluge de lames: a 10 s dummy buff every 30 s; mod-rogue strikes up to 6 enemies around on use
    # (rogue.outlaw_blade_flurry_targets) and repeats the rogue's single-target damage on them while it lasts
    @{ Id = 92346; Clone = 2983; Name = 'Déluge de lames'; IconPath = 'Interface\Icons\Ability_Warrior_PunishingBlow'; FallbackIconSpell = 13877; Cost = 15; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Frappe jusqu'à 6 ennemis proches pour 50% des dégâts de votre arme principale. Pendant 10 s, les dégâts que vous infligez à une seule cible sont répétés sur jusqu'à 6 autres ennemis à 8 m, pour 45% de leur montant."
       AuraDescription = "Vos dégâts sur une seule cible sont répétés sur jusqu'à 6 ennemis proches, pour 45%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 1; 131 = (Look 'OutlawBladeFlurry'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagBladeFlurry; 213 = 0 } },
    # Poussée d'adrénaline: real auras, Energy regeneration doubled and attack speed +20% for 20 s, every 3 min
    @{ Id = 92347; Clone = 13750; Name = "Poussée d'adrénaline"; IconPath = 'Interface\Icons\Spell_Shadow_ShadowWordDominate'; FallbackIconSpell = 13750; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Votre régénération d'énergie augmente de 100% et votre vitesse d'attaque de 20% pendant 20 s."
       AuraDescription = "Régénération d'énergie augmentée de 100%. Vitesse d'attaque augmentée de 20%."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModPowerRegenPercent; TargetA = 1; Value = 100; Misc = 3 },
           @{ Index = 1; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 20 })
       Fields = @{ 1 = 0; 40 = 18; 131 = (Look 'OutlawAdrenalineRush'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagAdrenalineRush } },
    # Ruée des lames (talent): onto the enemy at 20 yd every 45 s; mod-rogue rushes the rogue there (MoveCharge) and
    # strikes it (200% of a main-hand hit, Blade Flurry's targets too), and puts 92361 on the rogue. No charge effect:
    # the core refuses a charge from melee range, and a melee rogue is almost always there (1 cast in 6 bot-minutes)
    @{ Id = 92348; Clone = 1766; Name = 'Ruée des lames'; IconPath = 'Interface\Icons\ability_ironmaidens_bladerush'; FallbackIconSpell = 36554; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Fondez sur l'ennemi à 20 m et frappez-le pour 200% des dégâts de votre arme principale, de même que les cibles de Déluge de lames. Vous récupérez ensuite 5 points d'énergie par seconde pendant 5 s."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 1 = 0; 3 = 0; 46 = 3; 131 = (Look 'OutlawBladeRush'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagBladeRush; 213 = 0 } },
    # Série meurtrière (talent): every 2 min at 10 yd; effect 0 the dummy mod-rogue turns into 7 strikes of both
    # weapons over 2 s, teleporting around the target; effect 1 a 2 s aura on the rogue (the look's state while it
    # lasts)
    @{ Id = 92349; Clone = 1766; Name = 'Série meurtrière'; IconPath = 'Interface\Icons\Ability_Rogue_MurderSpree'; FallbackIconSpell = 51690; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Vous vous téléportez autour de l'ennemi à 10 m et le frappez 7 fois en 2 s avec vos deux armes. Déluge de lames répète ces coups."
       AuraDescription = 'Frappe la cible de toutes parts.'
       Effects = @(
           @{ Index = 0; Effect = 3; TargetA = 6 },
           @{ Index = 1; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 3 = 0; 40 = 39; 46 = 7; 131 = (Look 'OutlawKillingSpree'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagKillingSpree; 213 = 0 } },
    # Rejouer (talent): a self cast every 3 min; mod-rogue lengthens the current Roll the Bones buffs by 30 s
    @{ Id = 92350; Clone = 2983; Name = 'Rejouer'; IconPath = 'Interface\Icons\INV_Misc_Dice_02'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = 'Vos effets actuels de Jeter les os durent 30 s de plus.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 1 = 0; 131 = 0; 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagKeepItRolling; 213 = 0 } },
    # Lames d'effroi (talent): a 10 s dummy buff every 1 min 30 s; mod-rogue makes the builders give the maximum combo
    # points while it lasts
    @{ Id = 92351; Clone = 2983; Name = "Lames d'effroi"; IconPath = 'Interface\Icons\inv_sword_1h_artifactskywall_d_06dual'; FallbackIconSpell = 51690; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $combat; ClassMask = $classMask
       Description = "Vos lames maudites vous possèdent pendant 10 s : vos techniques qui génèrent des points de combo vous accordent le maximum de points de combo."
       AuraDescription = 'Vos techniques qui génèrent des points de combo vous accordent le maximum de points de combo.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 1 = 0; 40 = 1; 131 = (Look 'OutlawDreadblades'); 205 = 133; 206 = 1000; 209 = 0; 210 = 0; 211 = $flagDreadblades; 213 = 0 } },

    # Opportunité: Pistol Shot free (its cost modifier) for 10 s. Charges, not stacks (a stack would multiply the
    # modifier): one when applied, two with Marteau en éventail (mod-rogue sets them), each Pistol Shot cast spends one
    # by itself - not the triggered extra shots, which pay no cost. Field 49 = 0 shows the charges on the buff.
    @{ Id = 92352; Clone = 2983; Name = 'Opportunité'; IconPath = 'Interface\Icons\ability_rogue_pistolshot'; FallbackIconSpell = 26679; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Votre prochain Tir de pistolet ne coûte pas d'énergie et inflige 50% de dégâts en plus."
       AuraDescription = "Prochain Tir de pistolet : gratuit et 50% de dégâts en plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 36 = 1; 40 = 1; 49 = 0; 122 = 0; 123 = 0; 124 = $flagPistolShot; 131 = (Look 'OutlawOpportunity'); 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Audace: Ambush usable outside stealth for 10 s (Shadow Dance's form exemption, Ambush's flag only); mod-rogue
    # removes it when Ambush is cast
    @{ Id = 92353; Clone = 2983; Name = 'Audace'; IconPath = 'Interface\Icons\Ability_Rogue_Ambush'; FallbackIconSpell = 8676; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Embuscade est utilisable sans être camouflé.'; AuraDescription = 'Embuscade est utilisable sans être camouflé.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 275; TargetA = 1 })
       Fields = @{ 40 = 1; 122 = 0x200; 123 = 0; 124 = 0; 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Roll the Bones' six buffs, 30 s each (mod-rogue sets a shorter one for Compter les chances), the dice over the
    # rogue's head. Trésor enfoui, Grande mêlée and Précision impitoyable are real auras; the other three are
    # dummies mod-rogue reads.
    @{ Id = 92354; Clone = 2983; Name = 'Bordée'; IconPath = 'Interface\Icons\ability_rogue_rollthebones07'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vos techniques qui génèrent des points de combo en rapportent 1 de plus.'
       AuraDescription = 'Vos techniques qui génèrent des points de combo en rapportent 1 de plus.'
       Fields = @{ 40 = 9; 131 = (Look 'OutlawRtbBuff'); 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92355; Clone = 2983; Name = 'Trésor enfoui'; IconPath = 'Interface\Icons\ability_rogue_rollthebones05'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Votre régénération d'énergie augmente de 25%."; AuraDescription = "Régénération d'énergie augmentée de 25%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModPowerRegenPercent; TargetA = 1; Value = 25; Misc = 3 })
       Fields = @{ 40 = 9; 131 = (Look 'OutlawRtbBuff'); 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92356; Clone = 2983; Name = 'Grande mêlée'; IconPath = 'Interface\Icons\ability_rogue_rollthebones02'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Votre vitesse d'attaque augmente de 40%."; AuraDescription = "Vitesse d'attaque augmentée de 40%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 40 })
       Fields = @{ 40 = 9; 131 = (Look 'OutlawRtbBuff'); 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92357; Clone = 2983; Name = 'Précision impitoyable'; IconPath = 'Interface\Icons\ability_rogue_rollthebones03'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vos chances de coup critique augmentent de 20%.'; AuraDescription = 'Chances de coup critique augmentées de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModCritPct; TargetA = 1; Value = 20 })
       Fields = @{ 40 = 9; 131 = (Look 'OutlawRtbBuff'); 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92358; Clone = 2983; Name = 'Tête de mort'; IconPath = 'Interface\Icons\ability_rogue_rollthebones01'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Frappe sinistre a 25% de chances de plus de frapper une seconde fois.'
       AuraDescription = 'Frappe sinistre a 25% de chances de plus de frapper une seconde fois.'
       Fields = @{ 40 = 9; 131 = (Look 'OutlawRtbBuff'); 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92359; Clone = 2983; Name = 'Cap assuré'; IconPath = 'Interface\Icons\Ability_rogue_rollthebones04'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vos coups de grâce réduisent de 0,5 s de plus par point de combo les temps de recharge de Lames sans repos.'
       AuraDescription = 'Coups de grâce : temps de recharge de Lames sans repos réduits de 0,5 s de plus par point de combo.'
       Fields = @{ 40 = 9; 131 = (Look 'OutlawRtbBuff'); 209 = 0; 210 = 0; 211 = 0 } },
    # Mèches de Peau-Verte: the next Pistol Shot deals 300% more (mod-rogue reads and removes it), 15 s
    @{ Id = 92360; Clone = 2983; Name = 'Mèches de Peau-Verte'; IconPath = 'Interface\Icons\INV_Misc_Ammo_Gunpowder_02'; FallbackIconSpell = 26679; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochain Tir de pistolet inflige 300% de dégâts en plus.'; AuraDescription = 'Prochain Tir de pistolet : 300% de dégâts en plus.'
       Fields = @{ 40 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Dés pipés: Adrenaline Rush's pending extra roll, shown until the next Roll the Bones spends it (mod-rogue puts it
    # on and takes it off; a bot reads it)
    @{ Id = 92367; Clone = 2983; Name = 'Dés pipés'; IconPath = 'Interface\Icons\INV_Misc_Dice_01'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre prochain Jeter les os accorde un bonus de plus.'; AuraDescription = 'Prochain Jeter les os : un bonus de plus.'
       Fields = @{ 40 = 21; 209 = 0; 210 = 0; 211 = 0 } },
    # Ruée des lames' Energy: 5 every second for 5 s, a real periodic energize mod-rogue casts after the charge
    @{ Id = 92361; Clone = 2983; Name = 'Ruée des lames'; IconPath = 'Interface\Icons\ability_ironmaidens_bladerush'; FallbackIconSpell = 36554; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Vous récupérez 5 points d'énergie par seconde."; AuraDescription = "Récupère 5 points d'énergie par seconde."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 24; TargetA = 1; Value = 5; Misc = 3 })
       Fields = @{ 40 = 28; 98 = 1000; 209 = 0; 210 = 0; 211 = 0 } },
    # Tir de pistolet's snare on its own (Pistol Shot carries the same as its effect 2): for a shot mod-rogue fires
    # without casting 92342
    @{ Id = 92362; Clone = 2983; Name = 'Tir de pistolet'; IconPath = 'Interface\Icons\ability_rogue_pistolshot'; FallbackIconSpell = 26679; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vitesse de déplacement réduite de 50%.'; AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 33; TargetA = 6; Value = -50 })
       Fields = @{ 3 = 11; 40 = 32; 46 = 4; 209 = 0; 210 = 0; 211 = 0; 213 = 0 } },
    # The helpers mod-rogue names (92363-92369; 92365, Between the Eyes' own stun, is not needed: 92344 carries it).
    # 92363 and 92366 are never cast: they name the hits mod-rogue deals - Blade Flurry's repeat and Main Gauche's
    # off-hand strike - in the combat log and the meters (Sinister Strike's melee layout, physical, no weapon needed,
    # no visual, no family flag)
    @{ Id = 92363; Clone = 48638; Name = 'Déluge de lames'; IconPath = 'Interface\Icons\Ability_Warrior_PunishingBlow'; FallbackIconSpell = 13877; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un coup répété par Déluge de lames.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = @{ 1 = 0; 68 = -1; 69 = 0; 70 = 0; 131 = 0; 205 = 0; 206 = 0; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    @{ Id = 92366; Clone = 48638; Name = 'Main gauche'; IconPath = 'Interface\Icons\INV_Weapon_ShortBlade_15'; FallbackIconSpell = 13877; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un coup de votre arme de main gauche.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = @{ 1 = 0; 68 = -1; 69 = 0; 70 = 0; 131 = 0; 205 = 0; 206 = 0; 209 = 0; 210 = 0; 211 = 0; 225 = 1 } },
    # 92364: Entre les deux yeux' critical strike buff, +20% for 3 s per combo point (mod-rogue sets the duration)
    @{ Id = 92364; Clone = 2983; Name = 'Entre les deux yeux'; IconPath = 'Interface\Icons\INV_Weapon_Rifle_01'; FallbackIconSpell = 26679; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vos chances de coup critique augmentent de 20%.'; AuraDescription = 'Chances de coup critique augmentées de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModCritPct; TargetA = 1; Value = 20 })
       Fields = @{ 40 = 8; 209 = 0; 210 = 0; 211 = 0 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-rogue reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\rogue\talentTree.json') -Family 8

return $spells
