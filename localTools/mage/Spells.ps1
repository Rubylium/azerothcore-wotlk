# The Mage's spell data for its retail-style talent trees (localTools/mage/talentTree.json): the abilities its
# nodes teach, the auras its scripted talents show, and the rank spells of its new talents. Behaviour lives in
# modules/mod-mage; this file owns client/server spell data only. Ids 92000-92199: 92000-92099 talent ranks (generated
# from the tree), 92100-92199 abilities and auras.
#
# Clone field notes: 28 casting time index (1 instant), 40 duration index (1 10 s, 8 15 s, 9 30 s, 18 20 s, 21 never,
# 27 3 s, 29 12 s, 31 8 s, 32 6 s, 4 2 min), 46 range index (1 self, 4 30 yd), 47 missile speed (float bits), 92-94
# radius index (29 6 yd), 98-100 periodic interval, 131 visual, 208 family (3 Mage), 209-211 family flags, 225 school.

$classMask = 128
$arcane = 237
$fire = 8
$frost = 6

# The abilities' own family flags, word 2: kept apart from every stock mage spell, so no WotLK talent modifier reaches
# them by accident, and a new talent can aim at one alone (Alter Time's is in talentTree.json)
$flagAlterTime = 0x1000
$flagShockwave = 0x2000
$flagPhoenix = 0x4000
$flagTempest = 0x8000

$spells = @(
    # --- Class tree -----------------------------------------------------------------------------------------------
    # Distorsion temporelle: Bloodlust, the Mage's. It keeps Bloodlust's effects (30% haste on the group for 40 s)
    # and its script (spell_sha_bloodlust, bound in mod-mage's SQL): whoever has it, or Heroism or Bloodlust, is Sated
    # for 10 min.
    @{ Id = 92100; Clone = 2825; Name = 'Distorsion temporelle'; IconPath = 'Interface\Icons\Spell_Nature_TimeStop'; FallbackIconSpell = 2825; Cost = 0; Cooldown = 300000; Level = 1; Spellbook = $true; SkillLine = $arcane; ClassMask = $classMask
       Description = "Déforme le temps : votre groupe et vous gagnez 30% de hâte pendant 40 s. Ceux qui en profitent ne peuvent plus en bénéficier, ni d'un effet similaire, pendant 10 min."
       AuraDescription = 'Hâte augmentée de 30%.'
       Fields = @{ 208 = 3; 209 = 0; 210 = 0; 211 = 0; 225 = 64 } },
    # Invisibilité supérieure: the Invisibility aura itself (32612 - it drops the caster from combat and hides them for
    # 20 s), cast at once instead of after Invisibility's fade. Its end starts the 3 s damage reduction (mod-mage).
    @{ Id = 92101; Clone = 32612; Name = 'Invisibilité supérieure'; IconPath = 'Interface\Icons\Ability_Mage_Invisibility'; FallbackIconSpell = 66; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $arcane; ClassMask = $classMask
       Description = "Vous rend invisible sur-le-champ pendant 20 s ; les ennemis vous perdent de vue. Attaquer ou lancer un sort met fin à l'effet. Pendant les 3 s qui suivent, vous subissez 60% de dégâts en moins."
       AuraDescription = 'Invisible.'
       Fields = @{ 208 = 3; 209 = 0; 210 = 0; 211 = 0 } },
    # Altération du temps: a 10 s buff. When it ends, however it ends (expiry, or cancelled by the player), mod-mage
    # puts the mage back where it was, with the health and mana it had then.
    @{ Id = 92102; Clone = 12043; Name = 'Altération du temps'; IconPath = 'Interface\Icons\INV_Misc_PocketWatch_01'; FallbackIconSpell = 12043; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $arcane; ClassMask = $classMask
       Description = "Fige votre instant présent pendant 10 s. Quand l'effet prend fin, ou que vous l'annulez (clic droit), vous revenez là où vous étiez, avec les points de vie et le mana que vous aviez alors."
       AuraDescription = "Vous reviendrez à cet instant quand l'effet prendra fin."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 34 = 0; 35 = 0; 36 = 0; 40 = 1; 208 = 3; 209 = 0; 210 = 0; 211 = $flagAlterTime } },
    @{ Id = 92103; Clone = 2983; Name = 'Invisibilité supérieure'; IconPath = 'Interface\Icons\Ability_Mage_Invisibility'; FallbackIconSpell = 66; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits de 60%.'; AuraDescription = 'Dégâts subis réduits de 60%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 87; TargetA = 1; Value = -60; Misc = 127 }); Fields = @{ 40 = 27 } },
    # The charges of Blink and Fire Blast, shown as stacks while one is spent (mod-mage keeps the count)
    @{ Id = 92106; Clone = 2983; Name = 'Charges de Clignotement'; IconPath = 'Interface\Icons\Spell_Arcane_Blink'; FallbackIconSpell = 1953; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Clignotement possède 2 charges.'; AuraDescription = 'Charges de Clignotement disponibles.'; Fields = @{ 40 = 21 } },
    @{ Id = 92107; Clone = 2983; Name = 'Charges de Trait de feu'; IconPath = 'Interface\Icons\Spell_Fire_Fireball'; FallbackIconSpell = 2136; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Trait de feu possède 2 charges.'; AuraDescription = 'Charges de Trait de feu disponibles.'; Fields = @{ 40 = 21 } },
    # Cautérisation: the burn after a lethal hit (mod-mage sets each tick to 5% of maximum health), and the marker
    # that it cannot happen again for 2 min
    @{ Id = 92108; Clone = 2983; Name = 'Cautérisation'; IconPath = 'Interface\Icons\Spell_Fire_Immolation'; FallbackIconSpell = 11129; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vous brûlez.'; AuraDescription = 'Vous brûlez.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 40 = 32; 98 = 1000; 225 = 4 } },
    @{ Id = 92109; Clone = 2983; Name = 'Cautérisé'; IconPath = 'Interface\Icons\Spell_Fire_Immolation'; FallbackIconSpell = 11129; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Cautérisation ne peut plus se déclencher.'; AuraDescription = 'Cautérisation ne peut plus se déclencher.'; Fields = @{ 40 = 4 } },
    # Flux d'incantation: 4% damage a stack, 1 to 5 stacks, cycled by mod-mage while in combat
    @{ Id = 92115; Clone = 2983; Name = "Flux d'incantation"; IconPath = 'Interface\Icons\Spell_Arcane_Arcane04'; FallbackIconSpell = 12042; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Dégâts augmentés.'; AuraDescription = 'Dégâts augmentés de 4% par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 4; Misc = 127 }); Fields = @{ 40 = 21 } },
    @{ Id = 92116; Clone = 2983; Name = 'Déplacement fulgurant'; IconPath = 'Interface\Icons\Spell_Fire_BurningSpeed'; FallbackIconSpell = 1953; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vitesse de déplacement augmentée de 40%.'; AuraDescription = 'Vitesse de déplacement augmentée de 40%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 31; TargetA = 1; Value = 40 }); Fields = @{ 40 = 27 } },

    # --- Arcanes --------------------------------------------------------------------------------------------------
    # Toucher du mage: a mark on the enemy for 8 s (Slow's targeting, a dummy aura); mod-mage stores a share of the
    # damage dealt to it and releases it when the mark ends
    @{ Id = 92110; Clone = 31589; Name = 'Toucher du mage'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane02'; FallbackIconSpell = 31589; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $arcane; ClassMask = $classMask
       Description = "Marque la cible pendant 8 s : elle accumule 25% des dégâts que vous lui infligez, puis explose et les inflige en dégâts des Arcanes à elle et aux ennemis proches."
       AuraDescription = 'Accumule les dégâts du mage.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 })
       Fields = @{ 40 = 31; 208 = 3; 209 = 0; 210 = 0; 211 = 0; 225 = 64 } },
    # Onde de choc arcanique: Blast Wave, in Arcane - damage, knockback and daze around the mage
    @{ Id = 92111; Clone = 42945; Name = 'Onde de choc arcanique'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane03'; FallbackIconSpell = 42921; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $arcane; ClassMask = $classMask
       Description = 'Une onde des Arcanes jaillit autour de vous : dégâts des Arcanes aux ennemis proches, repoussés et hébétés.'
       Fields = @{ 131 = 965; 208 = 3; 209 = 0; 210 = 0; 211 = $flagShockwave; 225 = 64 } },
    # Tempête du Néant: a 12 s damage over time ticking every second; mod-mage splashes half of each tick around the
    # target. Living Bomb's layout without its explosion (effect 1 dropped).
    @{ Id = 92112; Clone = 55360; Name = 'Tempête du Néant'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane01'; FallbackIconSpell = 55360; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $arcane; ClassMask = $classMask
       Description = "Déchaîne une tempête des Arcanes sur la cible pendant 12 s : elle subit des dégâts des Arcanes chaque seconde, et les ennemis proches la moitié."
       AuraDescription = 'Subit des dégâts des Arcanes chaque seconde.'
       Fields = @{ 72 = 0; 80 = 114; 98 = 1000; 131 = 68; 208 = 3; 209 = 0; 210 = 0; 211 = $flagTempest; 225 = 64 } },
    @{ Id = 92118; Clone = 2983; Name = 'Harmonie arcanique'; IconPath = 'Interface\Icons\Spell_Arcane_ArcaneTorrent'; FallbackIconSpell = 44425; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Votre prochain Barrage des arcanes inflige plus de dégâts.'; AuraDescription = 'Prochain Barrage des arcanes : 5% de dégâts en plus par charge.'; Fields = @{ 40 = 9 } },
    @{ Id = 92133; Clone = 2983; Name = 'Avatar arcanique'; IconPath = 'Interface\Icons\Spell_Arcane_ArcanePotency'; FallbackIconSpell = 12042; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Vitesse d'incantation augmentée de 20%."; AuraDescription = "Vitesse d'incantation augmentée de 20%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 65; TargetA = 1; Value = 20 }); Fields = @{ 40 = 8 } },

    # --- Feu ------------------------------------------------------------------------------------------------------
    # Combustion: every Fire spell critically strikes for 10 s (the stock Combustion's look, none of its charges)
    @{ Id = 92120; Clone = 11129; Name = 'Combustion'; IconPath = 'Interface\Icons\Spell_Fire_SealOfFire'; FallbackIconSpell = 11129; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $fire; ClassMask = $classMask
       Description = 'Pendant 10 s, tous vos sorts de Feu infligent un coup critique.'
       AuraDescription = 'Tous vos sorts de Feu infligent un coup critique.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 71; TargetA = 1; Value = 100; Misc = 4 })
       Fields = @{ 34 = 0; 35 = 0; 36 = 0; 40 = 1; 208 = 3; 209 = 0; 210 = 0; 211 = 0 } },
    # Flammes du phénix: Fire Blast as a 30 yd missile with Fireball's look, on a 25 s cooldown; mod-mage splashes half
    # of it on the enemies around the target
    @{ Id = 92121; Clone = 42873; Name = 'Flammes du phénix'; IconPath = 'Interface\Icons\Spell_Fire_Flare'; FallbackIconSpell = 42873; Cost = 0; Cooldown = 25000; Level = 1; Spellbook = $true; SkillLine = $fire; ClassMask = $classMask
       Description = "Projette l'esprit d'un phénix sur la cible : dégâts de Feu, et la moitié aux ennemis proches."
       # 47 = 24.0f: Fireball's missile speed, so the phoenix flies
       Fields = @{ 46 = 4; 47 = 1103101952; 74 = 301; 80 = 1299; 131 = 67; 208 = 3; 209 = 0; 210 = 0; 211 = $flagPhoenix } },
    # Météore: ground-targeted like Flamestrike, instant; its dummy marks the spot and mod-mage brings the impact down
    # 3 s later (92124). 86 = 87 is TARGET_DEST_DEST, the spot the player picked.
    @{ Id = 92122; Clone = 42926; Name = 'Météore'; IconPath = 'Interface\Icons\Spell_Fire_MeteorStorm'; FallbackIconSpell = 42926; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $fire; ClassMask = $classMask
       Description = "Appelle un météore sur la zone ciblée : il s'écrase 3 s plus tard, infligeant de lourds dégâts de Feu aux ennemis."
       Fields = @{ 28 = 1; 71 = 3; 72 = 0; 74 = 0; 80 = 0; 86 = 87; 95 = 0; 208 = 3; 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92124; Clone = 42926; Name = 'Météore'; IconPath = 'Interface\Icons\Spell_Fire_MeteorStorm'; FallbackIconSpell = 42926; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un météore s''écrase.'
       Fields = @{ 28 = 1; 42 = 0; 72 = 0; 74 = 401; 80 = 2399; 208 = 3; 209 = 0; 210 = 0; 211 = 0 } },
    @{ Id = 92129; Clone = 2983; Name = 'Bénédiction du roi-soleil'; IconPath = 'Interface\Icons\Spell_Fire_Fire'; FallbackIconSpell = 11366; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 8; Spellbook = $false
       Description = 'À 8 charges, votre prochaine Explosion pyrotechnique est instantanée et déclenche Combustion.'; AuraDescription = 'Chaleurs continues consommées.'; Fields = @{ 40 = 21 } },

    # --- Givre ----------------------------------------------------------------------------------------------------
    # Tempête de comètes: Ice Lance's missile carries a dummy; on arrival mod-mage rains 7 comets (92131) around the
    # target
    @{ Id = 92130; Clone = 42914; Name = 'Tempête de comètes'; IconPath = 'Interface\Icons\Spell_Frost_IceStorm'; FallbackIconSpell = 42914; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = 'Fait pleuvoir 7 comètes glacées autour de la cible, chacune infligeant des dégâts de Givre aux ennemis proches.'
       Fields = @{ 71 = 3; 74 = 0; 80 = 0; 208 = 3; 209 = 0; 210 = 0; 211 = 0 } },
    # A comet: Flamestrike's damage, in Frost, 6 yd, with Frost Nova's look
    @{ Id = 92131; Clone = 42926; Name = 'Tempête de comètes'; IconPath = 'Interface\Icons\Spell_Frost_IceStorm'; FallbackIconSpell = 42914; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une comète glacée s''écrase.'
       Fields = @{ 28 = 1; 42 = 0; 72 = 0; 74 = 101; 80 = 699; 92 = 29; 131 = 17; 208 = 3; 209 = 0; 210 = 0; 211 = 0; 225 = 16 } },
    @{ Id = 92125; Clone = 2983; Name = 'Veines gelées'; IconPath = 'Interface\Icons\Spell_Frost_ColdHearted'; FallbackIconSpell = 12472; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Chances de coup critique avec les sorts augmentées de 20%.'; AuraDescription = 'Chances de coup critique avec les sorts augmentées de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 57; TargetA = 1; Value = 20 }); Fields = @{ 40 = 18 } },
    @{ Id = 92126; Clone = 2983; Name = 'Froid mordant'; IconPath = 'Interface\Icons\Spell_Frost_ChillingBlast'; FallbackIconSpell = 116; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Dégâts de Givre augmentés.'; AuraDescription = 'Dégâts de Givre augmentés de 1% par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 1; Misc = 16 }); Fields = @{ 40 = 31 } },
    @{ Id = 92127; Clone = 2983; Name = 'Stalactites'; IconPath = 'Interface\Icons\Spell_Frost_Glacier'; FallbackIconSpell = 30455; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Votre prochain Javelot de glace les projette toutes.'; AuraDescription = 'Stalactites prêtes à être projetées par Javelot de glace.'; Fields = @{ 40 = 9 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-mage reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\mage\talentTree.json') -Family 3

return $spells
