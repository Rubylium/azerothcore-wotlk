# The Death Knight's spell data for its retail-style talent trees (localTools/deathknight/talentTree.json): the
# abilities its nodes and specializations teach, the auras its scripted talents show, and the rank spells of its new
# talents. Behaviour lives in modules/mod-death-knight; this file owns client/server spell data only.
# Ids 92500-92799: 92500-92599 talent ranks (generated from the tree), 92600-92699 abilities and auras, 92700-92709
# the specializations' passives.
#
# Clone field notes: 1 category, 9 attributes ex 5, 12-13 stances, 28 casting time index (1 instant), 40 duration
# index (1 10 s, 8 15 s, 9 30 s, 18 20 s, 21 never, 28 5 s, 29 12 s, 31 8 s, 32 6 s, 35 4 s, 4 2 min), 41 power type
# (5 runes, 6 runic power), 42 cost (runic power in tenths), 46 range index (1 self, 4 30 yd), 80-82 base points, 86-88
# target A, 89-91 target B (15 enemies around the source), 92-94 radius index (9 20 yd, 14 8 yd, 20 25 yd, 32 12 yd),
# 98-100 periodic interval, 116-118 triggered spell, 131 visual, 204 mana cost percentage (Cone of Cold's, cleared),
# 208 family (15 Death Knight), 209-211 family flags, 225 school (1 physical, 16 frost, 32 shadow), 226 rune cost
# (SpellRuneCost: 0 none, 768 Frost, 780 Unholy, 787 Blood, each giving 10 runic power).
#
# Targets: 1 the caster, 5 its pet, 6 the enemy target, 22 around the caster (with target B 15), 104 a cone in front.
# Weapon strikes keep Blood Strike's layout (121 normalized weapon damage plus the flat bonus, 31 the weapon
# percentage applied to both); area strikes take Whirlwind's (no target needed).

$classMask = 32
$blood = 770
$frost = 771
$unholy = 772

# The new abilities' own family flags, word 2 (bits 0x100-0x40000000 are free among the Death Knight's spells): no
# WotLK talent modifier reaches them by accident, and a new talent can aim at one alone
$flagMarrowrend = 0x100
$flagCaress = 0x200
$flagConsumption = 0x400
$flagBonestorm = 0x800
$flagSoulReaper = 0x1000
$flagOutbreak = 0x2000
$flagPillar = 0x4000
$flagFrostscythe = 0x8000
$flagWinter = 0x10000
$flagBreath = 0x20000
$flagFury = 0x40000
$flagAdvance = 0x80000
$flagApocalypse = 0x100000
$flagTransformation = 0x200000
$flagEpidemic = 0x400000
$flagAssault = 0x800000
$flagWound = 0x1000000
$flagWraithWalk = 0x2000000
$flagTombstone = 0x4000000

$spells = @(
    # --- Spec passives --------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-death-knight also reads them to know
    # which one is on. Spell modifiers on the stock spells' family flags: Death and Decay word 0 0x20, Blood Boil
    # word 0 0x40000, Howling Blast word 1 0x2, Obliterate word 1 0x20000, Frost Strike word 1 0x4, Summon Gargoyle
    # word 1 0x80. A cost modifier also reaches rune costs (Spell::TakeRunePower applies SPELLMOD_COST to each rune),
    # which is how Rime's Freezing Fog frees Howling Blast: -100% takes every rune off.
    # Death and Decay is the Death Knight's area tool in every specialization: free of runes, on its cooldown only.
    # Sentinelle sanguine (Blood): Blood Boil costs no rune (mod-death-knight gives it charges instead: 2, 7.5 s each)
    # and Death and Decay comes back every 15 s
    @{ Id = 92700; Clone = 2983; Name = 'Sentinelle sanguine'; IconPath = 'Interface\Icons\Spell_Deathknight_BloodPresence'; FallbackIconSpell = 48266; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Furoncle sanglant ne coûte plus de rune : il a 2 charges, rechargées en 7,5 s, et propage votre Peste de sang à chaque ennemi touché. Brise-moelle vous entoure de Bouclier d'os, et Frappe de mort vous rend 25% des dégâts subis pendant les 5 dernières secondes (7% de vos points de vie au moins). Mort et décomposition ne coûte plus de rune et se recharge en 15 s ; dedans, Frappe au cœur touche 3 ennemis de plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST },
           @{ Index = 2; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -15000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0x40000; 123 = 0; 124 = 0; 125 = 0x20; 126 = 0; 127 = 0; 128 = 0x20; 129 = 0; 130 = 0; 208 = 15 } },
    # Cœur de l'hiver (Frost): Howling Blast loses its 8 s cooldown (runes are its limit), Obliterate and Frost Strike
    # deal 10% more
    @{ Id = 92701; Clone = 2983; Name = "Cœur de l'hiver"; IconPath = 'Interface\Icons\Spell_Deathknight_FrostPresence'; FallbackIconSpell = 48263; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Rafale hurlante n'a plus de temps de recharge, et Anéantissement et Frappe de givre infligent 10% de dégâts en plus. Mort et décomposition ne coûte plus de rune."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST },
           @{ Index = 1; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -8000; Misc = $SPELLMOD_COOLDOWN },
           @{ Index = 2; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 10; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = 0x20; 123 = 0; 124 = 0; 125 = 0; 126 = 0x2; 127 = 0; 128 = 0; 129 = 0x20004; 130 = 0; 208 = 15 } },
    # Maître de la peste (Unholy): Death and Decay every 20 s and free, Summon Gargoyle every 1 min 30 s
    @{ Id = 92702; Clone = 2983; Name = 'Maître de la peste'; IconPath = 'Interface\Icons\Spell_Deathknight_UnholyPresence'; FallbackIconSpell = 48265; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Frappe purulente inflige des Plaies purulentes que Frappe du Fléau fait éclater. Mort et décomposition ne coûte plus de rune et se recharge en 20 s ; dedans, Frappe du Fléau frappe jusqu'à 4 ennemis de plus. Invocation d'une gargouille se recharge en 1 min 30 s."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST },
           @{ Index = 1; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -10000; Misc = $SPELLMOD_COOLDOWN },
           @{ Index = 2; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -90000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 122 = 0x20; 123 = 0; 124 = 0; 125 = 0x20; 126 = 0; 127 = 0; 128 = 0; 129 = 0x80; 130 = 0; 208 = 15 } },

    # --- Class tree -----------------------------------------------------------------------------------------------
    # Marche spectrale (Wraith Walk): 70% speed and immune to snares (mechanic 11) and roots (7) for 4 s
    @{ Id = 92600; Clone = 49039; Name = 'Marche spectrale'; IconPath = 'Interface\Icons\Spell_Shadow_Twilight'; FallbackIconSpell = 49039; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = 'Pendant 4 s, votre vitesse de déplacement augmente de 70% et vous êtes insensible aux ralentissements et aux entraves.'
       AuraDescription = 'Vitesse de déplacement augmentée de 70%. Insensible aux ralentissements et aux entraves.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 1; Value = 70 },
           @{ Index = 1; Effect = 6; Aura = 77; TargetA = 1; Misc = 11 },
           @{ Index = 2; Effect = 6; Aura = 77; TargetA = 1; Misc = 7 })
       Fields = @{ 9 = 0; 40 = 35; 208 = 15; 209 = 0; 210 = 0; 211 = $flagWraithWalk; 226 = 0 } },
    # Faucheuse d'âmes (Soul Reaper): a strike (100% weapon damage plus 350) and a 5 s mark; mod-death-knight brings the
    # scythe down (92602) when the mark ends on an enemy under 35% health
    @{ Id = 92601; Clone = 49930; Name = "Faucheuse d'âmes"; IconPath = 'Interface\Icons\Spell_Shadow_SoulLeech_3'; FallbackIconSpell = 47476; Cost = 0; Cooldown = 6000; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = "Frappe la cible (100% des dégâts de l'arme) et la marque pendant 5 s. Si elle a alors moins de 35% de ses points de vie, elle subit de lourds dégâts d'Ombre."
       AuraDescription = "Subira de lourds dégâts d'Ombre dans 5 s si elle a moins de 35% de ses points de vie."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 6; Value = 350 },
           @{ Index = 1; Effect = 31; TargetA = 6; Value = 100 },
           @{ Index = 2; Effect = 6; Aura = $A_Dummy; TargetA = 6 })
       Fields = @{ 1 = 0; 40 = 28; 208 = 15; 209 = 0; 210 = 0; 211 = $flagSoulReaper; 226 = 780 } },
    @{ Id = 92602; Clone = 47632; Name = "Faucheuse d'âmes"; IconPath = 'Interface\Icons\Spell_Shadow_SoulLeech_3'; FallbackIconSpell = 47476; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1000 })
       Fields = @{ 208 = 15; 209 = 0; 210 = 0; 211 = $flagSoulReaper; 225 = 32 } },
    # Poussée de fièvre (Outbreak): a 30 yd dummy; mod-death-knight infects the target and every enemy within 10 yd of
    # it with both diseases
    @{ Id = 92603; Clone = 49909; Name = 'Poussée de fièvre'; IconPath = 'Interface\Icons\Spell_Shadow_CreepingPlague'; FallbackIconSpell = 50842; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = "Infecte la cible et tous les ennemis à 10 m autour d'elle de Fièvre de givre et de Peste de sang."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = @{ 46 = 4; 131 = 11172; 208 = 15; 209 = 0; 210 = 0; 211 = $flagOutbreak; 225 = 32; 226 = 0 } },
    # Soif de sang (Blood Draw): the marker that it cannot happen again for 2 min, and the name of its heal
    @{ Id = 92604; Clone = 2983; Name = 'Soif de sang'; IconPath = 'Interface\Icons\Spell_Shadow_LifeDrain02'; FallbackIconSpell = 55233; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Soif de sang ne peut plus se déclencher.'; AuraDescription = 'Soif de sang ne peut plus se déclencher.'; Fields = @{ 40 = 4 } },
    # Serres glaciales (Icy Talons): 3% attack speed a stack, 3 stacks, 6 s
    @{ Id = 92605; Clone = 2983; Name = 'Serres glaciales'; IconPath = 'Interface\Icons\Spell_Deathknight_IcyTalons'; FallbackIconSpell = 50887; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = "Vitesse d'attaque augmentée."; AuraDescription = "Vitesse d'attaque augmentée de 3% par charge."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 3 }); Fields = @{ 40 = 32 } },
    # Sol profané: held by mod-death-knight while the Death Knight stands in its own Death and Decay. Heart Strike hits
    # 3 more enemies (SPELLMOD_JUMP_TARGETS on word 0 0x1000000); the specs' other bonuses in it are scripted.
    @{ Id = 92607; Clone = 2983; Name = 'Sol profané'; IconPath = 'Interface\Icons\Spell_Shadow_DeathAndDecay'; FallbackIconSpell = 49938; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vous vous tenez dans votre Mort et décomposition.'
       AuraDescription = 'Dans votre Mort et décomposition : Frappe au cœur et Frappe du Fléau frappent plus d''ennemis.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 3; Misc = 17 })
       Fields = @{ 40 = 21; 122 = 0x1000000; 123 = 0; 124 = 0; 208 = 15 } },

    # --- Sang -----------------------------------------------------------------------------------------------------
    # Brise-moelle (Marrowrend): 60% weapon damage plus 500 for an Unholy rune; mod-death-knight adds 3 Bone Shield
    @{ Id = 92610; Clone = 49930; Name = 'Brise-moelle'; IconPath = 'Interface\Icons\INV_Misc_Bone_01'; FallbackIconSpell = 49924; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $blood; ClassMask = $classMask
       Description = "Frappe la cible avec des os (60% des dégâts de l'arme plus 300) et vous confère 3 charges de Bouclier d'os, jusqu'à 10."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 6; Value = 500 },
           @{ Index = 1; Effect = 31; TargetA = 6; Value = 60 })
       Fields = @{ 131 = 11831; 208 = 15; 209 = 0; 210 = 0; 211 = $flagMarrowrend; 226 = 780 } },
    # Blood Boil's charges, shown as stacks (mod-death-knight keeps the count)
    @{ Id = 92611; Clone = 2983; Name = 'Charges de Furoncle sanglant'; IconPath = 'Interface\Icons\Spell_DeathKnight_BloodBoil'; FallbackIconSpell = 49941; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Furoncle sanglant.'; AuraDescription = 'Charges de Furoncle sanglant disponibles.'; Fields = @{ 40 = 21 } },
    # Caresse de la mort (Death's Caress): Shadow damage at 30 yd for a Blood rune; mod-death-knight adds Blood Plague
    # and 2 Bone Shield
    @{ Id = 92612; Clone = 49909; Name = 'Caresse de la mort'; IconPath = 'Interface\Icons\Spell_Shadow_DeathScream'; FallbackIconSpell = 49895; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $blood; ClassMask = $classMask
       Description = "Frappe la cible à distance : dégâts d'Ombre, Peste de sang, et 2 charges de Bouclier d'os."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 250 })
       Fields = @{ 46 = 4; 131 = 10755; 208 = 15; 209 = 0; 210 = 0; 211 = $flagCaress; 225 = 32; 226 = 787 } },
    # Consommation (Consumption): Whirlwind's layout in an 8 yd cone (every enemy in it: 212, Whirlwind's 4-target cap, is 0), 150% weapon damage plus 600, every 30 s;
    # mod-death-knight heals the Death Knight for a quarter of it
    @{ Id = 92613; Clone = 1680; Name = 'Consommation'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowFury'; FallbackIconSpell = 55262; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $blood; ClassMask = $classMask
       Description = "Frappe tous les ennemis devant vous (150% des dégâts de l'arme) et vous soigne de 25% des dégâts infligés."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 104; Value = 600 },
           @{ Index = 1; Effect = 31; TargetA = 104; Value = 150 })
       Fields = @{ 1 = 0; 12 = 0; 13 = 0; 41 = 5; 46 = 1; 92 = 14; 93 = 14; 212 = 0; 131 = 11117; 208 = 15; 209 = 0; 210 = 0; 211 = $flagConsumption; 225 = 1; 226 = 0 } },
    # Tempête d'os (Bonestorm): 60 runic power, an 8 s aura that triggers its whirl (92615) every second
    @{ Id = 92614; Clone = 49039; Name = "Tempête d'os"; IconPath = 'Interface\Icons\INV_Misc_Bone_ElfSkull_01'; FallbackIconSpell = 49222; Cost = 600; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $blood; ClassMask = $classMask
       Description = "Un tourbillon d'os inflige des dégâts d'Ombre à tous les ennemis à 8 m chaque seconde pendant 8 s, et vous soigne de 1% de vos points de vie par ennemi touché."
       AuraDescription = "Des os tourbillonnent autour de vous."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 23; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 31; 41 = 6; 98 = 1000; 116 = 92615; 208 = 15; 209 = 0; 210 = 0; 211 = $flagBonestorm; 226 = 0 } },
    @{ Id = 92615; Clone = 49941; Name = "Tempête d'os"; IconPath = 'Interface\Icons\INV_Misc_Bone_ElfSkull_01'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre aux ennemis proches."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 22; Value = 150 })
       Fields = @{ 89 = 15; 92 = 14; 131 = 0; 208 = 15; 209 = 0; 210 = 0; 211 = $flagBonestorm; 225 = 32; 226 = 0 } },
    # Pierre tombale (Tombstone): a self dummy; mod-death-knight trades up to 5 Bone Shield for the absorb (92617) and
    # runic power
    @{ Id = 92616; Clone = 49039; Name = 'Pierre tombale'; IconPath = 'Interface\Icons\INV_Misc_Bone_Skull_02'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $blood; ClassMask = $classMask
       Description = "Consume jusqu'à 5 charges de Bouclier d'os : chacune vous confère un bouclier absorbant 6% de vos points de vie maximum pendant 8 s et 6 points de puissance runique."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 0; 208 = 15; 209 = 0; 210 = 0; 211 = $flagTombstone; 226 = 0 } },
    @{ Id = 92617; Clone = 2983; Name = 'Pierre tombale'; IconPath = 'Interface\Icons\INV_Misc_Bone_Skull_02'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_SchoolAbsorb; TargetA = 1; Value = 1; Misc = 127 }); Fields = @{ 40 = 31 } },
    # Fléau cramoisi (Crimson Scourge): shown for 10 s when a Blood Plague tick resets Death and Decay
    @{ Id = 92619; Clone = 2983; Name = 'Fléau cramoisi'; IconPath = 'Interface\Icons\Spell_Shadow_BloodBoil'; FallbackIconSpell = 49941; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Mort et décomposition est prête.'; AuraDescription = 'Mort et décomposition est prête.'; Fields = @{ 40 = 1 } },
    # Bouclier d'os (retail's, from Marrowrend): 3% armor and 1% attack speed a stack, 10 stacks, 30 s. A melee hit
    # taken removes one, at most one every 2 s (mod-death-knight).
    @{ Id = 92630; Clone = 2983; Name = "Bouclier d'os"; IconPath = 'Interface\Icons\INV_Misc_Bone_HumanSkull_01'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = "Des os vous protègent."; AuraDescription = "Armure augmentée de 3% et vitesse d'attaque de 1% par charge. Un coup en mêlée subi brise une charge."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 101; TargetA = 1; Value = 3; Misc = 1 },
           @{ Index = 1; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 1 })
       Fields = @{ 40 = 9 } },

    # --- Givre ----------------------------------------------------------------------------------------------------
    # Pilier de givre (Pillar of Frost): 20% Strength for 12 s, every 45 s, no rune
    @{ Id = 92640; Clone = 51271; Name = 'Pilier de givre'; IconPath = 'Interface\Icons\Spell_Frost_FrozenCore'; FallbackIconSpell = 51271; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = 'Votre Force augmente de 20% pendant 12 s.'
       AuraDescription = 'Force augmentée de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModTotalStatPercentage; TargetA = 1; Value = 20; Misc = 0 })
       Fields = @{ 40 = 29; 208 = 15; 209 = 0; 210 = 0; 211 = $flagPillar; 226 = 0 } },
    # Faux de givre (Frostscythe): Whirlwind's layout in an 8 yd cone, 75% weapon damage plus 250 as Frost, a Frost rune
    @{ Id = 92641; Clone = 1680; Name = 'Faux de givre'; IconPath = 'Interface\Icons\Ability_Warrior_Cleave'; FallbackIconSpell = 55268; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = "Fauche tous les ennemis devant vous : 75% des dégâts de l'arme plus 190, sous forme de dégâts de Givre."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 104; Value = 250 },
           @{ Index = 1; Effect = 31; TargetA = 104; Value = 75 })
       Fields = @{ 1 = 0; 12 = 0; 13 = 0; 41 = 5; 46 = 1; 92 = 14; 93 = 14; 212 = 0; 131 = 11612; 208 = 15; 209 = 0; 210 = 0; 211 = $flagFrostscythe; 225 = 16; 226 = 768 } },
    # Hiver impitoyable (Remorseless Winter): an 8 s aura that triggers its storm (92643) every second, a Frost rune
    @{ Id = 92642; Clone = 49039; Name = 'Hiver impitoyable'; IconPath = 'Interface\Icons\Spell_Frost_IceStorm'; FallbackIconSpell = 49203; Cost = 0; Cooldown = 20000; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = "Une tempête de glace vous entoure pendant 8 s et inflige des dégâts de Givre chaque seconde aux ennemis à 8 m."
       AuraDescription = 'Une tempête de glace vous entoure.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 23; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 31; 41 = 5; 98 = 1000; 116 = 92643; 131 = 11156; 208 = 15; 209 = 0; 210 = 0; 211 = $flagWinter; 226 = 768 } },
    @{ Id = 92643; Clone = 49941; Name = 'Hiver impitoyable'; IconPath = 'Interface\Icons\Spell_Frost_IceStorm'; FallbackIconSpell = 49203; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Givre aux ennemis proches.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 22; Value = 90 })
       Fields = @{ 89 = 15; 92 = 14; 131 = 0; 208 = 15; 209 = 0; 210 = 0; 211 = $flagWinter; 225 = 16; 226 = 0 } },
    # Souffle de Sindragosa (Breath of Sindragosa): a toggle, every minute; mod-death-knight breathes (92645) every
    # second for 15 runic power, until there is none left or the aura is cancelled
    @{ Id = 92644; Clone = 49039; Name = 'Souffle de Sindragosa'; IconPath = 'Interface\Icons\Spell_Frost_FrostBlast'; FallbackIconSpell = 49184; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = "Un souffle de givre continu inflige des dégâts de Givre aux ennemis devant vous chaque seconde, au prix de 15 points de puissance runique par seconde, jusqu'à ce que vous n'en ayez plus. Annulez l'effet pour l'arrêter."
       AuraDescription = 'Souffle de givre : 15 points de puissance runique par seconde.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = @{ 9 = 0; 40 = 21; 208 = 15; 209 = 0; 210 = 0; 211 = $flagBreath; 226 = 0 } },
    @{ Id = 92645; Clone = 42931; Name = 'Souffle de Sindragosa'; IconPath = 'Interface\Icons\Spell_Frost_FrostBlast'; FallbackIconSpell = 49184; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Givre aux ennemis devant vous.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 104; Value = 300 })
       Fields = @{ 1 = 0; 40 = 0; 204 = 0; 41 = 6; 92 = 32; 131 = 11617; 208 = 15; 209 = 0; 210 = 0; 211 = $flagBreath; 225 = 16; 226 = 0 } },
    # Fureur du wyrm de givre (Frostwyrm's Fury): a 25 yd cone of heavy Frost damage and a 50% slow, every 1 min 30 s
    @{ Id = 92646; Clone = 42931; Name = 'Fureur du wyrm de givre'; IconPath = 'Interface\Icons\Achievement_Boss_Sindragosa'; FallbackIconSpell = 49184; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = 'Un wyrm de givre souffle devant vous : lourds dégâts de Givre sur 25 m, et les ennemis touchés sont ralentis de 50% pendant 6 s.'
       AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 104; Value = 2500 },
           @{ Index = 1; Effect = 6; Aura = 33; TargetA = 104; Value = -50 })
       Fields = @{ 1 = 0; 40 = 32; 204 = 0; 41 = 5; 92 = 20; 93 = 20; 131 = 11617; 208 = 15; 209 = 0; 210 = 0; 211 = $flagFury; 225 = 16; 226 = 0 } },
    # Avancée glaciale (Glacial Advance): 30 runic power, a 20 yd cone of Frost damage, every 6 s - on one enemy no more
# than a Frost Strike, it pays off on a pack
    @{ Id = 92647; Clone = 42931; Name = 'Avancée glaciale'; IconPath = 'Interface\Icons\Spell_Frost_Glacier'; FallbackIconSpell = 55268; Cost = 300; Cooldown = 6000; Level = 1; Spellbook = $true; SkillLine = $frost; ClassMask = $classMask
       Description = 'Projette des pointes de glace devant vous : dégâts de Givre à tous les ennemis sur 20 m.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 104; Value = 250 })
       Fields = @{ 1 = 0; 40 = 0; 204 = 0; 41 = 6; 92 = 9; 131 = 11617; 208 = 15; 209 = 0; 210 = 0; 211 = $flagAdvance; 225 = 16; 226 = 0 } },

    # --- Impie ----------------------------------------------------------------------------------------------------
    # Frappe purulente (Festering Strike): Blood Strike itself, 50% weapon damage instead of 40% (80/81 raw base points,
    # stored minus one), its family flags kept so Blood Strike's disease bonus, Sudden Doom, Desolation and Reaping
    # reach it; mod-death-knight adds 2 Festering Wounds
    @{ Id = 92660; Clone = 49930; Name = 'Frappe purulente'; IconPath = 'Interface\Icons\Spell_Shadow_CreepingPlague'; FallbackIconSpell = 49930; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = "Frappe la cible (50% des dégâts de l'arme plus 382, davantage par maladie) et lui inflige 2 Plaies purulentes, jusqu'à 6. Frappe du Fléau en fait éclater une, Apocalypse jusqu'à 4."
       Fields = @{ 81 = 49; 131 = 11832 } },
    @{ Id = 92661; Clone = 2983; Name = 'Plaie purulente'; IconPath = 'Interface\Icons\Spell_Shadow_CreepingPlague'; FallbackIconSpell = 49930; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 6; Spellbook = $false
       Description = 'Une plaie purulente.'; AuraDescription = "Chaque plaie éclate sous Frappe du Fléau et Apocalypse et inflige des dégâts d'Ombre."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 9 } },
    @{ Id = 92662; Clone = 47632; Name = 'Plaie purulente'; IconPath = 'Interface\Icons\Spell_Shadow_CreepingPlague'; FallbackIconSpell = 49930; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Une plaie purulente éclate : dégâts d'Ombre."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 300 })
       Fields = @{ 208 = 15; 209 = 0; 210 = 0; 211 = $flagWound; 225 = 32 } },
    # Apocalypse: 100% weapon damage plus 700, every 45 s; mod-death-knight bursts up to 4 wounds, a rune back for each
    @{ Id = 92663; Clone = 49930; Name = 'Apocalypse'; IconPath = 'Interface\Icons\Spell_Shadow_DarkSummoning'; FallbackIconSpell = 55271; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = "Frappe la cible (100% des dégâts de l'arme plus 700) et fait éclater jusqu'à 4 de ses Plaies purulentes ; chaque plaie éclatée réactive l'une de vos runes."
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 6; Value = 700 },
           @{ Index = 1; Effect = 31; TargetA = 6; Value = 100 })
       Fields = @{ 1 = 0; 131 = 11832; 208 = 15; 209 = 0; 210 = 0; 211 = $flagApocalypse; 226 = 0 } },
    # Transformation sombre (Dark Transformation): on the pet, Ghoul Frenzy's layout - 60% damage, 30% attack speed,
    # 30% bigger, 15 s, every 45 s
    @{ Id = 92664; Clone = 63560; Name = 'Transformation sombre'; IconPath = 'Interface\Icons\Spell_Shadow_AbominationExplosion'; FallbackIconSpell = 63560; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = 'Transforme votre goule en abomination pendant 15 s : elle inflige 60% de dégâts en plus et attaque 30% plus vite.'
       AuraDescription = 'Transformée en abomination : dégâts augmentés de 60%, vitesse d''attaque de 30%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 5; Value = 60; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 5; Value = 30 },
           @{ Index = 2; Effect = 6; Aura = $A_ModScale; TargetA = 5; Value = 30 })
       Fields = @{ 40 = 8; 204 = 0; 208 = 15; 209 = 0; 210 = 0; 211 = $flagTransformation; 226 = 0 } },
    # Épidémie (Epidemic): 30 runic power, a self dummy; mod-death-knight strikes (92666) every enemy within 40 yd that
    # carries the Death Knight's Blood Plague
    @{ Id = 92665; Clone = 49895; Name = 'Épidémie'; IconPath = 'Interface\Icons\Ability_Creature_Disease_05'; FallbackIconSpell = 50842; Cost = 300; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = "Inflige des dégâts d'Ombre à chaque ennemi atteint de votre Peste de sang dans un rayon de 40 m."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 46 = 1; 131 = 11172; 208 = 15; 209 = 0; 210 = 0; 211 = $flagEpidemic } },
    @{ Id = 92666; Clone = 47632; Name = 'Épidémie'; IconPath = 'Interface\Icons\Ability_Creature_Disease_05'; FallbackIconSpell = 50842; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 200 })
       Fields = @{ 208 = 15; 209 = 0; 210 = 0; 211 = $flagEpidemic; 225 = 32 } },
    # Assaut impie (Unholy Assault): 20% haste for 20 s, every minute; mod-death-knight puts 4 wounds on the target
    @{ Id = 92667; Clone = 49039; Name = 'Assaut impie'; IconPath = 'Interface\Icons\Spell_Shadow_UnholyFrenzy'; FallbackIconSpell = 49016; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $unholy; ClassMask = $classMask
       Description = 'Votre hâte augmente de 20% pendant 20 s, et votre cible reçoit 4 Plaies purulentes.'
       AuraDescription = 'Hâte augmentée de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 20 })
       Fields = @{ 9 = 0; 40 = 18; 208 = 15; 209 = 0; 210 = 0; 211 = $flagAssault; 226 = 0 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-death-knight reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\deathknight\talentTree.json') -Family 15

return $spells
