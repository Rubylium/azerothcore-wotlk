# The Rogue's spell data for its retail-style talent trees (localTools/rogue/talentTree.json): the abilities its
# nodes teach, the auras its scripted talents show, and the rank spells of its new talents. Behaviour lives in
# modules/mod-rogue; this file owns client/server spell data only. Ids 92190-92399: 92190-92199 spec passives,
# 92200-92299 talent ranks (generated from the tree), 92300-92399 abilities and auras. The Combat tree is the Crimson
# Duelist rework, whose data stays at the top of patchSinisterStrike.ps1.
#
# Clone field notes: 1 category, 2 dispel type, 3 mechanic, 28 casting time index (1 instant), 40 duration index (1
# 10 s, 8 15 s, 18 20 s, 21 never, 27 3 s, 31 8 s, 32 6 s), 46 range index (1 self, 2 melee, 4 30 yd), 92-94 radius
# index (13 10 yd), 131 visual, 205-206 global cooldown category and time, 208 family (8 Rogue), 209-211 family flags,
# 213 damage class (0 none: it cannot miss), 225 school.

$classMask = 8
$assassination = 253
$combat = 38
$subtlety = 39

$spells = @(
    # --- Spec passives --------------------------------------------------------------------------------------------
    # Sinister Strike costs nothing since the Combat rework (Crimson Duelist makes it an Energy builder); Assassinat and
    # Finesse learn this with their spec, which puts the stock 45 Energy back. Word 0's 0x2 is Sinister Strike's own
    # flag (Quick Cut and Blade Echo clone it, but only Combat has them).
    @{ Id = 92191; Clone = 2983; Name = 'Frappe sinistre'; IconPath = 'Interface\Icons\Spell_Shadow_RitualOfSacrifice'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Frappe sinistre coûte 45 points d'énergie."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 45; Misc = $SPELLMOD_COST })
       Fields = @{ 122 = 0x2; 208 = 8 } },
    # Assassinat's own passive: Cold Blood's cooldown 3 min -> 30 s (word 1 0x40), and the mark mod-rogue reads for the
    # spec's damage over time (Envenom spreading the bleeds, Fan of Knives' poisons, Virulence)
    @{ Id = 92192; Clone = 2983; Name = 'Maître des toxines'; IconPath = 'Interface\Icons\Ability_Rogue_DeviousPoisons'; FallbackIconSpell = 1329; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Envenimer propage vos saignements, Déluge de lames empoisonne chaque ennemi touché, et chacune de vos afflictions augmente vos dégâts. Sang-froid : 30 s de recharge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -150000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0; 123 = 0x40; 124 = 0; 208 = 8 } },
    # Finesse's own passive: Shadow Dance's cooldown 1 min -> 30 s (word 1 0x2000000)
    @{ Id = 92193; Clone = 2983; Name = 'Danseur des ombres'; IconPath = 'Interface\Icons\Ability_Rogue_ShadowDance'; FallbackIconSpell = 51713; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Danse de l'ombre : 30 s de recharge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -30000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0; 123 = 0x2000000; 124 = 0; 208 = 8 } },

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
       Fields = @{ 1 = 0; 3 = 0; 46 = 4; 131 = 3239; 205 = 133; 206 = 1000; 208 = 8; 209 = 0; 210 = 0; 211 = 0; 213 = 0 } },
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
       Fields = @{ 2 = 0; 40 = 1; 41 = 3; 46 = 4; 131 = 250; 205 = 133; 206 = 1000; 208 = 8; 209 = 0; 210 = 0; 211 = 0; 213 = 0; 225 = 1 } },
    # Exsanguiner: a melee-range dummy; mod-rogue deals the rest of the caster's Rupture and Garrote at once
    @{ Id = 92313; Clone = 1766; Name = 'Exsanguiner'; IconPath = 'Interface\Icons\Spell_DeathKnight_BloodBoil'; FallbackIconSpell = 1943; Cost = 25; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = 'Vos Rupture et Garrot sur la cible lui infligent sur-le-champ tous leurs dégâts restants.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 1 = 0; 3 = 0; 131 = 250; 205 = 133; 206 = 1000; 208 = 8; 209 = 0; 210 = 0; 211 = 0; 213 = 0 } },

    # Tempête cramoisie: a finisher on the target (it spends the combo points there), a dummy mod-rogue turns into a slash
    # and a bleed on every enemy within 10 yd, longer with every combo point
    @{ Id = 92330; Clone = 48668; Name = 'Tempête cramoisie'; IconPath = 'Interface\Icons\Ability_Rogue_BloodSplatter'; FallbackIconSpell = 48672; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $assassination; ClassMask = $classMask
       Description = "Coup de grâce qui entaille tous les ennemis à 10 m et les fait saigner pendant 2 s de plus par point de combo, jusqu'à 12 s."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 209 = 0; 210 = 0; 211 = 0 } },
    # Its bleed: amount and length set by mod-rogue for the combo points spent; one tick every 2 s
    @{ Id = 92331; Clone = 48672; Name = 'Tempête cramoisie'; IconPath = 'Interface\Icons\Ability_Rogue_BloodSplatter'; FallbackIconSpell = 48672; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Saigne.'; AuraDescription = 'Saigne.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 40 = 8; 98 = 2000; 209 = 0; 210 = 0; 211 = 0 } },
    # Virulence: 2% damage per affliction of the rogue on its enemies, up to 15 (mod-rogue keeps the count)
    @{ Id = 92332; Clone = 2983; Name = 'Virulence'; IconPath = 'Interface\Icons\Ability_Rogue_DeviousPoisons'; FallbackIconSpell = 2818; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 15; Spellbook = $false
       Description = 'Dégâts augmentés.'; AuraDescription = 'Dégâts augmentés de 2% par saignement ou poison que vous entretenez sur vos ennemis.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 2; Misc = 127 }); Fields = @{ 40 = 21 } },

    # --- Finesse --------------------------------------------------------------------------------------------------
    # Symboles de mort: 15% damage for 10 s and 40 Energy, Cold Blood's look. Pure data.
    @{ Id = 92320; Clone = 14177; Name = 'Symboles de mort'; IconPath = 'Interface\Icons\Spell_Shadow_DeathScream'; FallbackIconSpell = 14177; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Vos dégâts augmentent de 15% pendant 10 s et vous récupérez 40 points d'énergie."
       AuraDescription = 'Dégâts augmentés de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 15; Misc = 127 }, @{ Index = 1; Effect = 30; TargetA = 1; Value = 40; Misc = 3 })
       Fields = @{ 34 = 0; 35 = 0; 36 = 0; 40 = 1; 41 = 3; 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Tempête de shurikens: Fan of Knives at 10 yd for 35 Energy; mod-rogue adds a combo point per enemy hit, up to 5
    @{ Id = 92321; Clone = 51723; Name = 'Tempête de shurikens'; IconPath = 'Interface\Icons\Ability_Rogue_FanOfKnives'; FallbackIconSpell = 51723; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask; NoEquipment = $true
       Description = "Projette des shurikens sur tous les ennemis à 10 m : dégâts d'arme, et 1 point de combo par ennemi touché, jusqu'à 5."
       Fields = @{ 92 = 13; 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Lames de l'ombre: a 10 s dummy buff with Shadow Dance's look, every 30 s; mod-rogue adds the damage and combo points
    @{ Id = 92322; Clone = 51713; Name = "Lames de l'ombre"; IconPath = 'Interface\Icons\Spell_Shadow_ShadowWordDominate'; FallbackIconSpell = 51713; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Pendant 10 s, vos attaques automatiques infligent 50% de dégâts en plus et vos techniques génèrent 1 point de combo de plus."
       AuraDescription = "Attaques automatiques : 50% de dégâts en plus. Techniques : 1 point de combo de plus."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 40 = 1; 208 = 8; 209 = 0; 210 = 0; 211 = 0 } },
    # Poudre noire: a finisher on the target; mod-rogue deals shadow damage to every enemy within 10 yd
    @{ Id = 92340; Clone = 48668; Name = 'Poudre noire'; IconPath = 'Interface\Icons\Spell_Shadow_Shadowfury'; FallbackIconSpell = 30283; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Coup de grâce qui inflige des dégâts d'Ombre à tous les ennemis à 10 m, plus par point de combo."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 209 = 0; 210 = 0; 211 = 0; 225 = 32 } },
    # Technique secrète: a finisher every 30 s; mod-rogue strikes every enemy within 10 yd three times, the rogue and two
    # shadows of it
    @{ Id = 92341; Clone = 48668; Name = 'Technique secrète'; IconPath = 'Interface\Icons\Ability_Rogue_ShadowStrikes'; FallbackIconSpell = 51713; Cost = 30; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $subtlety; ClassMask = $classMask
       Description = "Coup de grâce : vous et deux ombres de vous frappez tous les ennemis à 10 m, plus fort par point de combo."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 209 = 0; 210 = 0; 211 = 0; 225 = 32 } },
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
       Fields = @{ 40 = 21; 122 = 0xFFFFFFFF; 123 = 0xFFFFFFFF; 124 = 0xFFFFFFFF; 208 = 8 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-rogue reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\rogue\talentTree.json') -Family 8

return $spells
