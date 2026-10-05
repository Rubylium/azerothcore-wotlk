# The Faucheur's spell data (class 15, modules/mod-reaper): its abilities, the auras of its soul resource, the damage
# and heal carriers its scripts cast, and the rank spells of its talent trees (localTools/reaper/talentTree.json).
# Behaviour lives in modules/mod-reaper; this file owns client/server spell data only. Ascension's Reaper is the model
# (its numbers, tooltips and looks); the rows are our own, built on stock layouts, since a third of Ascension's use
# effects only its engine has.
# Ids: 98000-98099 the class kit, its resource and the class tree's abilities, 98100-98199 Moisson, 98200-98299 Âme,
# 98300-98399 Domination, 98400-98799 talent ranks (generated from the trees), 98900-98999 the specializations.
#
# Every look is one imported from the Ascension client as it is (localTools/reaper/ascensionVisuals.json, through
# localTools/ascensionImport/importVisuals.py): field 131 names the imported SpellVisual by its key.
#
# Field notes as in localTools/barbarian/Spells.ps1; besides: 24 caster aura spell (the abilities that consume souls
# ask for an Âme moissonnée, so the client greys them too), 36 proc charges (a modifier aura spent by the next spell
# it changes), 41 power type 6 (runic power: costs and gains in tenths, 300 = 30), 101-103 the effects' value
# multipliers (a float; a leech's healing), 225 school (16 frost, 32 shadow, 48 shadowfrost).

$classMask = 16384
$skillLine = 913
$family = 21
$gcd = 1500
$runic = 6

# Family flags, one bit per ability (talentTree.json's modifiers name them). Word 0: the class kit and the class tree.
$fReap = 0x1; $fMurder = 0x2; $fSoulrend = 0x4; $fStride = 0x8; $fUnderwalk = 0x10; $fReliquary = 0x20
$fSoulBolt = 0x40; $fGhostClaw = 0x80; $fSoulShock = 0x100; $fDeathwind = 0x200; $fSoulslam = 0x400
$fDeathstalker = 0x800; $fShear = 0x1000; $fLure = 0x2000; $fVeilwalk = 0x4000; $fLimbo = 0x8000
$fLitany = 0x10000; $fScreech = 0x20000; $fBargain = 0x40000; $fRush = 0x80000; $fTap = 0x100000
$fWraithblade = 0x200000; $fTormented = 0x400000; $fMaso = 0x800000; $fWithering = 0x1000000
$fSoulHarvest = 0x2000000; $fStrideHit = 0x4000000; $fSoulrendDebuff = 0x8000000
# Word 1: Moisson in the low half, Âme in the high half
$fSlaughter = 0x1; $fCrows = 0x2; $fDoomrend = 0x4; $fHarvestTime = 0x8; $fSever = 0x10; $fGrounds = 0x20
$fShudder = 0x40; $fShudderHit = 0x80; $fThresh = 0x100; $fBloodFrenzy = 0x200; $fDarkrend = 0x400
$fDoomrendShield = 0x800
$fDirge = 0x10000; $fDirgeHit = 0x20000; $fDeathchaser = 0x40000; $fEndbringer = 0x80000; $fGhostly = 0x100000
$fGhostlyHit = 0x200000; $fShade = 0x400000; $fRenewal = 0x800000; $fGravesite = 0x1000000
$fApparition = 0x2000000; $fSoulrot = 0x4000000; $fAnimaAmbush = 0x8000000; $fSpectralSelf = 0x10000000
# Word 2: Domination, and every Faucheur ability (0x40000000)
$fSoulStrike = 0x1; $fScythe = 0x2; $fScytheHit = 0x4; $fWarden = 0x8; $fWardenHit = 0x10; $fDreadwake = 0x20
$fRequiem = 0x40; $fBolstered = 0x80; $fDecimate = 0x100; $fSplinter = 0x200
$fAll = 0x40000000

$reaperLooks = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\mod-reaper\client-assets\imported\visuals.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json
function Look([string]$key) {
    $id = $reaperLooks.ids.$key
    if (-not $id) { throw "No imported Faucheur look '$key' (localTools\reaper\ascensionVisuals.json)." }
    return [uint32]$id
}

# Layouts: Mortal Strike's for a weapon strike, Whirlwind's for weapon damage around the Faucheur, Sprint's for a self
# buff, Ice Lance's for an instant spell at range, Shadowstep's for the strike from behind (its stock teleport, 36563),
# Intercept's rush, Stealth, Repentance's incapacitate, Death and Decay's ground area, Pummel's interrupt, Challenging
# Shout's burst around the Faucheur, Purge's dispel, Anti-Magic Shell's absorb, Taunt
$strike = 12294
$whirl = 1680
$selfBuff = 2983
$bolt = 30455
$shadowstep = 36554
$charge = 20252
$stealth = 1784
$incapacitate = 20066
$ground = 43265
$pummel = 6552
$burst = 1161
$purge = 370
$absorb = 48707
$taunt = 355

$one = [BitConverter]::ToUInt32([BitConverter]::GetBytes([single]1.0), 0)
$noItem = @{ 68 = [uint32]::MaxValue; 69 = 0; 70 = 0 }

# A weapon strike's effects: flat bonus, then the weapon percentage
function Strike($flat, $percent, $target = 6) {
    return @(
        @{ Index = 0; Effect = 121; TargetA = $target; Value = $flat },
        @{ Index = 1; Effect = 31; TargetA = $target; Value = $percent })
}

# A gain of runic power (in points: 10 = 10 runic power)
function Energize($index, $points) {
    return @{ Index = $index; Effect = 30; TargetA = 1; Value = 10 * $points; Misc = $runic }
}

# Every Faucheur spell: its own family and flags (the "every ability" bit in word 2), runic power, no stance, no
# category, no rune cost, melee by default
function Own($word0, $word1 = 0, $word2 = 0, $extra = @{}) {
    $fields = @{ 1 = 0; 12 = 0; 13 = 0; 14 = 0; 30 = 0; 41 = $runic; 205 = 133; 206 = $gcd; 208 = $family
        209 = $word0; 210 = $word1; 211 = ($word2 -bor $fAll); 213 = 2; 225 = 1; 226 = 0 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A spell or aura a script casts: off the global cooldown, no weapon, any range
function Carrier($word0, $word1 = 0, $word2 = 0, $extra = @{}) {
    $fields = Own $word0 $word1 $word2 @{ 46 = 13; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0; 206 = 0; 131 = 0 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A self buff: off the weapon, on the Faucheur
function Buff($word0, $word1 = 0, $word2 = 0, $extra = @{}) {
    $fields = Own $word0 $word1 $word2 @{ 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$spells = @(
    # --- Class kit (mod-reaper teaches these by level) -----------------------------------------------------------
    @{ Id = 98000; Clone = $strike; Name = 'Faucher'; IconPath = 'Interface\Icons\reap_reaper'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 1 Fragment d'âme et 10 points de puissance runique. Frappe l'ennemi pour 80% des dégâts de l'arme, de chacune de vos armes si vous en maniez deux."
       Effects = (Strike 10 80) + @(Energize 2 10); Fields = (Own $fReap 0 0 @{ 131 = (Look 'Reap') }) },
    # Faucher's off-hand blow when the Faucheur wields two weapons (the off hand's damage: SPELL_ATTR3_REQ_OFFHAND)
    @{ Id = 98025; Clone = $strike; Name = 'Faucher'; IconPath = 'Interface\Icons\reap_reaper'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "80% des dégâts de l'arme de main gauche."; Effects = (Strike 5 80)
       Fields = (Carrier $fReap 0 0 @{ 7 = 0x1000000; 46 = 2; 68 = 2; 69 = 0x2A5F3; 213 = 2 }) },
    @{ Id = 98001; Clone = $bolt; Name = 'Meurtre'; IconPath = 'Interface\Icons\_D3companion'; FallbackIconSpell = 47541; Cost = 300; Cooldown = 0; Level = 3; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 1 Âme moissonnée. Envoie des corbeaux d'âme sur un ennemi à 30 m : des dégâts d'Ombre, et ses chances de toucher sont réduites de 3% pendant 10 s."
       AuraDescription = 'Chances de toucher réduites de 3%.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 25 },
           @{ Index = 1; Effect = 6; Aura = 54; TargetA = 6; Value = -3 })
       Fields = (Own $fMurder 0 0 (@{ 40 = 1; 46 = 4; 213 = 1; 225 = 32; 131 = (Look 'Murder') } + $noItem)) },
    @{ Id = 98002; Clone = $strike; Name = "Déchirure d'âme"; IconPath = 'Interface\Icons\nhi_spiritweapons_Border'; FallbackIconSpell = 49998; Cost = 0; Cooldown = 8000; Level = 5; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Déchirez l'âme d'un ennemi : des dégâts d'Ombre, 15 points de puissance runique, et vos attaques physiques lui infligent 10% de dégâts supplémentaires pendant 15 s."
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 40 },
           (Energize 1 15),
           @{ Index = 2; Effect = 64; TargetA = 6 })
       Fields = (Own $fSoulrend 0 0 @{ 118 = 98021; 225 = 32; 131 = (Look 'Soulrend') }) },
    @{ Id = 98021; Clone = $selfBuff; Name = "Déchirure d'âme"; IconPath = 'Interface\Icons\nhi_spiritweapons_Border'; FallbackIconSpell = 49998; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Les attaques physiques du Faucheur infligent 10% de dégâts supplémentaires."
       AuraDescription = "Les attaques physiques du Faucheur infligent 10% de dégâts supplémentaires."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 271; TargetA = 6; Value = 10; Misc = 1 })
       Fields = (Carrier $fSoulrendDebuff 0 0 @{ 40 = 8; 213 = 0 }) },
    @{ Id = 98003; Clone = $shadowstep; Name = 'Foulée spectrale'; IconPath = 'Interface\Icons\_D3haunt'; FallbackIconSpell = 36554; Cost = 0; Cooldown = 20000; Level = 8; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Réapparaissez derrière votre cible à 25 m et frappez-la par surprise : des dégâts d'Ombre, et 20 points de puissance runique."
       Effects = @(
           @{ Index = 0; Effect = 64; TargetA = 6 },
           (Energize 1 20))
       Fields = (Own $fStride 0 0 (@{ 46 = 34; 116 = 36563; 131 = (Look 'SpectreStride') } + $noItem)) },
    # Foulée spectrale's strike as the Faucheur lands behind its target (mod-reaper)
    @{ Id = 98024; Clone = $bolt; Name = 'Foulée spectrale'; IconPath = 'Interface\Icons\_D3haunt'; FallbackIconSpell = 36554; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 30 })
       Fields = (Carrier $fStrideHit 0 0 @{ 47 = 0; 213 = 1; 225 = 32 }) },
    @{ Id = 98004; Clone = $stealth; Name = 'Marche funèbre'; IconPath = 'Interface\Icons\ability_rogue_ghostpirate'; FallbackIconSpell = 1784; Cost = 0; Cooldown = 30000; Level = 10; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Glissez dans le monde des esprits : vous êtes camouflé, mais votre vitesse de déplacement est réduite de 20%. Impossible à utiliser en combat.'
       AuraDescription = 'Camouflé. Vitesse de déplacement réduite de 20%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 36; TargetA = 1; Misc = 30 },
           @{ Index = 1; Effect = 6; Aura = 16; TargetA = 1; Value = 5 },
           @{ Index = 2; Effect = 6; Aura = 33; TargetA = 1; Value = -20 })
       Fields = (Buff $fUnderwalk 0 0 @{ 131 = (Look 'Underwalk') }) },
    @{ Id = 98005; Clone = $selfBuff; Name = 'Reliquaire des perdus'; IconPath = 'Interface\Icons\nhi_enragedspirit_Border'; FallbackIconSpell = 47193; Cost = 0; Cooldown = 20000; Level = 12; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme vos Âmes moissonnées et les change en Traits d'âme : un par âme, lancé chaque seconde, qui inflige des dégâts d'Ombregivre aux ennemis dans un cône de 10 m devant vous."
       AuraDescription = "Lance des Traits d'âme."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Buff $fReliquary 0 0 @{ 24 = 98017; 40 = 28; 131 = (Look 'Reliquary') }) },
    @{ Id = 98006; Clone = $bolt; Name = "Trait d'âme"; IconPath = 'Interface\Icons\nhi_enragedspirit_Border'; FallbackIconSpell = 47193; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombregivre aux ennemis devant le Faucheur."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 24; Value = 40 })
       Fields = (Carrier $fSoulBolt 0 0 @{ 46 = 1; 47 = 0; 92 = 13; 212 = 0; 213 = 1; 225 = 48; 131 = (Look 'SoulBolt') }) },
    @{ Id = 98007; Clone = $bolt; Name = 'Griffe fantôme'; IconPath = 'Interface\Icons\warlock_spelldrain'; FallbackIconSpell = 45524; Cost = 0; Cooldown = 15000; Level = 14; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Glacez un ennemi à 20 m d'une griffe fantomatique : vous lui volez 50% de sa vitesse de déplacement pendant 8 s. Tuer un ennemi réinitialise la recharge."
       AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 33; TargetA = 6; Value = -50 },
           @{ Index = 1; Effect = 64; TargetA = 1 })
       Fields = (Own $fGhostClaw 0 0 (@{ 40 = 31; 46 = 3; 47 = 0; 83 = 11; 117 = 98008; 213 = 1; 225 = 32; 131 = (Look 'GhostClaw') } + $noItem)) },
    @{ Id = 98008; Clone = $selfBuff; Name = 'Griffe fantôme'; IconPath = 'Interface\Icons\warlock_spelldrain'; FallbackIconSpell = 45524; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Vitesse de déplacement augmentée de 50%.'; AuraDescription = 'Vitesse de déplacement augmentée de 50%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 31; TargetA = 1; Value = 50 })
       Fields = (Carrier $fGhostClaw 0 0 @{ 40 = 31; 46 = 1; 131 = (Look 'GhostClawSpeed') }) },
    @{ Id = 98009; Clone = $incapacitate; Name = "Choc d'âme"; IconPath = 'Interface\Icons\nhi_spiritrune_Border'; FallbackIconSpell = 20066; Cost = 0; Cooldown = 30000; Level = 16; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Choquez l'âme d'un ennemi à 15 m : il ne peut plus agir pendant 20 s. Les dégâts subis rompent l'effet. Fonctionne sur les humanoïdes, les morts-vivants, les bêtes et les draconiens."
       AuraDescription = 'Ne peut plus bouger ni agir. Les dégâts rompent l''effet.'
       Fields = (Own $fSoulShock 0 0 (@{ 17 = 99; 40 = 18; 46 = 11; 213 = 1; 225 = 32; 131 = (Look 'SoulShock') } + $noItem)) },
    @{ Id = 98010; Clone = $ground; Name = 'Vent de mort'; IconPath = 'Interface\Icons\nhi_frost_swirl_Border'; FallbackIconSpell = 43265; Cost = 300; Cooldown = 0; Level = 18; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 1 Âme moissonnée. Créez un nuage de brume à l'endroit ciblé pendant 10 s : toutes les 2 s, les ennemis à 8 m subissent des dégâts de Givre, et vous récupérez 25% des dégâts infligés."
       AuraDescription = 'Dégâts de Givre toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 27; Aura = 3; TargetA = 28; Value = 20 })
       Fields = (Own $fDeathwind 0 0 (@{ 40 = 1; 46 = 4; 92 = 14; 98 = 2000; 213 = 1; 225 = 16; 131 = (Look 'Deathwind') } + $noItem)) },
    @{ Id = 98011; Clone = $burst; Name = "Fracas d'âmes"; IconPath = 'Interface\Icons\custom_T_Nhance_RPG_Icons_GhostBlast_Border'; FallbackIconSpell = 49203; Cost = 0; Cooldown = 45000; Level = 20; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Fracassez l'âme des ennemis à 10 m : des dégâts d'Ombregivre, et ils sont étourdis pendant 3 s. Consomme vos Âmes moissonnées : chacune augmente ces dégâts de 30%. Génère 20 points de puissance runique."
       AuraDescription = 'Étourdi.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 12; TargetA = 22 },
           @{ Index = 1; Effect = 2; TargetA = 22; Value = 20 },
           (Energize 2 20))
       Fields = (Buff $fSoulslam 0 0 @{ 40 = 27; 83 = 12; 89 = 15; 90 = 15; 92 = 13; 93 = 13; 212 = 0; 213 = 1; 225 = 48; 131 = (Look 'Soulslam') }) },
    @{ Id = 98012; Clone = $bolt; Name = 'Traqueur de mort'; IconPath = 'Interface\Icons\inv_helm_plate_raiddeathknight_p_01'; FallbackIconSpell = 49576; Cost = 0; Cooldown = 90000; Level = 24; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Marquez un ennemi à 20 m pendant 15 s : votre vitesse de déplacement augmente de 40% et vous voyez les ennemis camouflés à proximité. Traquer un humanoïde lui inflige Anxiété (10% de chances de rater ses attaques). Infliger des dégâts rompt l'effet."
       AuraDescription = 'Traqué.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 4; TargetA = 6 },
           @{ Index = 1; Effect = 64; TargetA = 1 })
       Fields = (Own $fDeathstalker 0 0 (@{ 40 = 8; 46 = 3; 47 = 0; 117 = 98022; 213 = 1; 225 = 8; 131 = (Look 'Deathstalker') } + $noItem)) },
    @{ Id = 98022; Clone = $selfBuff; Name = 'Traqueur de mort'; IconPath = 'Interface\Icons\inv_helm_plate_raiddeathknight_p_01'; FallbackIconSpell = 49576; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Vitesse de déplacement augmentée de 40%. Voit les ennemis camouflés.'; AuraDescription = 'Vitesse de déplacement augmentée de 40%. Voit les ennemis camouflés.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 31; TargetA = 1; Value = 40 },
           @{ Index = 1; Effect = 6; Aura = 228; TargetA = 1; Value = 170 })
       Fields = (Carrier $fDeathstalker 0 0 @{ 40 = 8; 46 = 1; 131 = (Look 'DeathstalkerSpeed') }) },
    @{ Id = 98023; Clone = $selfBuff; Name = 'Anxiété'; IconPath = 'Interface\Icons\inv_helm_plate_raiddeathknight_p_01'; FallbackIconSpell = 49576; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Chances de rater ses attaques augmentées de 10%.'; AuraDescription = "Quelque chose cloche. Vos chances de rater vos attaques augmentent de 10%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 54; TargetA = 6; Value = -10 })
       Fields = (Carrier $fDeathstalker 0 0 @{ 40 = 8; 213 = 1 }) },
    @{ Id = 98013; Clone = $purge; Name = "Tonte d'âme"; IconPath = 'Interface\Icons\ability_argus_soulbombdebuffsmall'; FallbackIconSpell = 370; Cost = 0; Cooldown = 16000; Level = 28; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Purgez l'âme d'un ennemi à 30 m : 2 effets magiques bénéfiques sont dissipés."
       Effects = @(@{ Index = 0; Effect = 38; TargetA = 6; Value = 2; Misc = 1 })
       Fields = (Own $fShear 0 0 (@{ 46 = 4; 213 = 1; 225 = 1; 131 = (Look 'SoulShear') } + $noItem)) },
    @{ Id = 98014; Clone = $burst; Name = "Leurre de pierre d'âme"; IconPath = 'Interface\Icons\inv_jewelcrafting_70_gem03_blue'; FallbackIconSpell = 1161; Cost = 0; Cooldown = 30000; Level = 30; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Placez un leurre mortel à l'endroit ciblé, à 30 m au plus : les ennemis à 8 m de lui sont provoqués et vous attaquent pendant 3 s."
       AuraDescription = 'Provoqué.'
       Effects = @(
           @{ Index = 0; Effect = 114; TargetA = 16 },
           @{ Index = 1; Effect = 6; Aura = 11; TargetA = 16 })
       Fields = (Own $fLure 0 0 (@{ 16 = 0x40; 40 = 27; 46 = 4; 89 = 0; 90 = 0; 92 = 14; 93 = 14; 212 = 0; 213 = 1; 131 = (Look 'SoulstoneLure') } + $noItem)) },

    # --- The resource: Fragments d'âme fill Âmes moissonnées, 3 of which give the Infusion d'âme (mod-reaper) ------
    @{ Id = 98016; Clone = 2983; Name = "Fragment d'âme"; IconPath = 'Interface\Icons\inv_custom_ReforgeTokenFragment'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 3
       Description = "À 3 fragments, vous générez une Âme moissonnée."; AuraDescription = "À 3 fragments, vous générez une Âme moissonnée."
       Fields = @{ 40 = 9; 208 = $family } },
    @{ Id = 98017; Clone = 2983; Name = 'Âme moissonnée'; IconPath = 'Interface\Icons\ability_warlock_soulswap'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 3
       Description = "À 3 âmes, vous obtenez l'Infusion d'âme."; AuraDescription = "À 3 âmes, vous obtenez l'Infusion d'âme, qui renforce les techniques consommant vos âmes."
       Fields = @{ 40 = 3; 208 = $family } },
    @{ Id = 98018; Clone = $selfBuff; Name = "Infusion d'âme"; IconPath = 'Interface\Icons\spell_warlock_soulburn'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Vos techniques qui consomment vos 3 Âmes moissonnées sont 20% plus efficaces."
       AuraDescription = "Vos techniques qui consomment vos 3 Âmes moissonnées sont 20% plus efficaces."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Carrier 0 0 0 @{ 40 = 3; 46 = 1; 131 = (Look 'SoulInfusion') }) },
    # The hidden passive every Faucheur carries: its procs (world SQL spell_proc) are the auto attacks it lands and the
    # parries and dodges it makes, which the talents of mod-reaper react to
    @{ Id = 98055; Clone = $selfBuff; Name = 'Faucheur'; IconPath = 'Interface\Icons\reap_reaper'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = 'Le Faucheur moissonne les âmes.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 }); Fields = @{ 208 = $family } },
    # The spellbook's explanation of the resource
    @{ Id = 98019; Clone = $selfBuff; Name = "Collecteur d'âmes"; IconPath = 'Interface\Icons\nhi_spiritfire_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Passif. Vos techniques génèrent des Fragments d'âme : 3 fragments forment une Âme moissonnée, et 3 Âmes moissonnées vous donnent l'Infusion d'âme. Certaines techniques consomment vos âmes pour des effets puissants. Votre puissance runique se dissipe hors combat."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Carrier 0 0 0 @{ 4 = 0x40; 40 = 21; 46 = 1 }) },

    # --- Class tree abilities --------------------------------------------------------------------------------------
    @{ Id = 98030; Clone = $selfBuff; Name = 'Litanie sinistre'; IconPath = 'Interface\Icons\nhi_scrollofsoulbending_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 90000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Embrassez vos pensées sinistres et générez instantanément 3 Âmes moissonnées.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Buff $fLitany 0 0 @{ 131 = (Look 'SinisterLitany') }) },
    @{ Id = 98031; Clone = $selfBuff; Name = 'Limbes'; IconPath = 'Interface\Icons\Ability_Rogue_ShadowDance'; FallbackIconSpell = 51713; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Arrachez-vous à votre enveloppe mortelle : vous êtes insensible aux sorts néfastes pendant 5 s et récupérez 30% de vos points de vie et de votre puissance runique maximum. Utilisable étourdi, apeuré ou désorienté."
       AuraDescription = 'Insensible aux sorts néfastes.'
       Effects = @(
           @{ Index = 0; Effect = 136; TargetA = 1; Value = 30 },
           @{ Index = 1; Effect = 137; TargetA = 1; Value = 30; Misc = $runic },
           @{ Index = 2; Effect = 6; Aura = 39; TargetA = 1; Misc = 126 })
       Fields = (Buff $fLimbo 0 0 @{ 9 = 0x60008; 40 = 28; 205 = 0; 206 = 0; 131 = (Look 'Limbo') }) },
    @{ Id = 98033; Clone = $selfBuff; Name = 'Pas du voile'; IconPath = 'Interface\Icons\_Diablo3_Thunderclap_Warlock'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Votre vitesse de déplacement augmente de 60% pendant 4 s et les effets de ralentissement et d''immobilisation sont dissipés. Utilisable camouflé.'
       AuraDescription = 'Vitesse de déplacement augmentée de 60%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 31; TargetA = 1; Value = 60 },
           @{ Index = 1; Effect = 108; TargetA = 1; Misc = 7 },
           @{ Index = 2; Effect = 108; TargetA = 1; Misc = 11 })
       Fields = (Buff $fVeilwalk 0 0 @{ 5 = 0x20; 40 = 35; 205 = 0; 206 = 0; 131 = (Look 'Veilwalk') }) },
    @{ Id = 98034; Clone = $burst; Name = 'Hurlement spectral'; IconPath = 'Interface\Icons\_D3horrify'; FallbackIconSpell = 47476; Cost = 200; Cooldown = 90000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Hurlez avec une intention spectrale : les ennemis à 10 m sont réduits au silence pendant 3 s, puis subissent des dégâts d'Ombregivre."
       AuraDescription = "Réduit au silence. Subira des dégâts d'Ombregivre à la fin de l'effet."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 27; TargetA = 22 })
       Fields = (Buff $fScreech 0 0 @{ 40 = 27; 83 = 9; 89 = 15; 92 = 13; 212 = 0; 213 = 1; 225 = 48; 131 = (Look 'GhastlyScreech') }) },
    @{ Id = 98035; Clone = $bolt; Name = 'Hurlement spectral'; IconPath = 'Interface\Icons\_D3horrify'; FallbackIconSpell = 47476; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombregivre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 30 })
       Fields = (Carrier $fScreech 0 0 @{ 47 = 0; 213 = 1; 225 = 48; 131 = (Look 'GhastlyScreechBurst') }) },
    @{ Id = 98036; Clone = $absorb; Name = 'Pacte du Geôlier'; IconPath = 'Interface\Icons\nhi_spiritarmor_Border'; FallbackIconSpell = 48707; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Invoquez le Geôlier : un bouclier absorbe des dégâts égaux à 30% de vos points de vie maximum pendant 10 s, et vous êtes insensible à la peur et au charme."
       AuraDescription = 'Absorbe des dégâts. Insensible à la peur et au charme.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 69; TargetA = 1; Value = 1; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 77; TargetA = 1; Misc = 5 },
           @{ Index = 2; Effect = 6; Aura = 77; TargetA = 1; Misc = 1 })
       Fields = (Buff $fBargain 0 0 @{ 40 = 1; 205 = 0; 206 = 0; 225 = 32; 131 = (Look 'JailersBargain') }) },
    @{ Id = 98037; Clone = $charge; Name = 'Ruée de la faux'; IconPath = 'Interface\Icons\ability_demonhunter_bladedance'; FallbackIconSpell = 20252; Cost = 0; Cooldown = 15000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Foncez sur un ennemi entre 8 et 25 m et générez 15 points de puissance runique.'
       Effects = @(@{ Index = 0; Effect = 96; TargetA = 6 }, (Energize 1 15))
       Fields = (Own $fRush 0 0 (@{ 46 = 95; 205 = 0; 206 = 0; 131 = (Look 'ScytheRush') } + $noItem)) },
    @{ Id = 98039; Clone = $selfBuff; Name = "Ponction d'âme"; IconPath = 'Interface\Icons\_D3soulharvest'; FallbackIconSpell = 47568; Cost = 0; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Arrachez une part de l'essence de votre propre âme : génère 50 points de puissance runique. Utilisable camouflé."
       Effects = @(Energize 0 50)
       Fields = (Buff $fTap 0 0 @{ 5 = 0x20; 205 = 0; 206 = 0; 131 = (Look 'SoulTap') }) },
    @{ Id = 98040; Clone = $strike; Name = 'Lame spectrale'; IconPath = 'Interface\Icons\inv_archaeology_orcclans_tattooknife'; FallbackIconSpell = 49020; Cost = 400; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 3 Âmes moissonnées. Frappe pour 150% des dégâts de l'arme plus des dégâts d'Ombre, en ignorant les résistances."
       Effects = @(
           @{ Index = 0; Effect = 31; TargetA = 6; Value = 150 },
           @{ Index = 1; Effect = 2; TargetA = 6; Value = 50 })
       Fields = (Own $fWraithblade 0 0 @{ 8 = 0x1; 225 = 32; 131 = (Look 'Wraithblade') }) },
    @{ Id = 98041; Clone = $selfBuff; Name = 'Âmes tourmentées'; IconPath = 'Interface\Icons\_SoulsDevourer_Blue'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 20000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme vos Âmes moissonnées : chacune devient une Âme tourmentée et génère 10 points de puissance runique. Tant qu'il vous en reste, vous subissez 10% de dégâts directs en moins, et chaque attaque directe subie en consomme une et vous soigne. Dure 18 s."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Buff $fTormented 0 0 @{ 24 = 98017; 131 = (Look 'TormentedSouls') }) },
    @{ Id = 98042; Clone = $selfBuff; Name = 'Âmes tourmentées'; IconPath = 'Interface\Icons\_SoulsDevourer_Blue'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts directs subis réduits de 10%. Chaque attaque directe subie consomme une Âme tourmentée et vous soigne.'
       AuraDescription = 'Dégâts directs subis réduits de 10%. Chaque attaque directe subie consomme une Âme tourmentée et vous soigne.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 87; TargetA = 1; Value = -10; Misc = 127 })
       Fields = (Carrier $fTormented 0 0 @{ 40 = 85; 46 = 1; 49 = 6; 131 = (Look 'TormentedSoulsBuff') }) },
    @{ Id = 98053; Clone = $selfBuff; Name = 'Âme tourmentée'; IconPath = 'Interface\Icons\_SoulsDevourer_Blue'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier $fTormented 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    @{ Id = 98043; Clone = $selfBuff; Name = 'Rage masochiste'; IconPath = 'Interface\Icons\ability_rogue_vendetta'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 90000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Après 1 s, vous subissez des dégâts égaux à 36% de votre puissance d'attaque et la rage vous gagne : vos dégâts augmentent de 20% pendant 15 s."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Buff $fMaso 0 0 @{ 205 = 0; 206 = 0; 131 = (Look 'MasochisticRage') }) },
    @{ Id = 98044; Clone = $selfBuff; Name = 'Rage masochiste'; IconPath = 'Interface\Icons\ability_rogue_vendetta'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts infligés augmentés de 20%.'; AuraDescription = 'Dégâts infligés augmentés de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 20; Misc = 127 })
       Fields = (Carrier $fMaso 0 0 @{ 40 = 8; 46 = 1; 131 = (Look 'MasochisticRageBuff') }) },
    @{ Id = 98045; Clone = $strike; Name = 'Toucher flétrissant'; IconPath = 'Interface\Icons\_D3spiritbarrage'; FallbackIconSpell = 50536; Cost = 0; Cooldown = 15000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Votre toucher pourrit l'armure et la volonté de la cible : son armure est réduite de 20% et les dégâts magiques qu'elle subit augmentent de 10% pendant 12 s. Génère 10 points de puissance runique."
       AuraDescription = 'Armure réduite de 20%. Dégâts magiques subis augmentés de 10%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 101; TargetA = 6; Value = -20; Misc = 1 },
           @{ Index = 1; Effect = 6; Aura = 87; TargetA = 6; Value = 10; Misc = 126 },
           (Energize 2 10))
       Fields = (Own $fWithering 0 0 (@{ 40 = 29; 213 = 1; 225 = 32; 131 = (Look 'WitheringTouch') } + $noItem)) },
    # Moisson d'âme (talent): the blast at the Faucheur's target as it harvests a soul; it heals from its damage
    @{ Id = 98046; Clone = $bolt; Name = "Moisson d'âme"; IconPath = 'Interface\Icons\nhi_spiritfire_Border'; FallbackIconSpell = 47541; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre, et le Faucheur récupère des points de vie."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 20 })
       Fields = (Carrier $fSoulHarvest 0 0 @{ 47 = 0; 213 = 1; 225 = 32; 131 = (Look 'ReapedSoul') }) },
    @{ Id = 98056; Clone = $selfBuff; Name = "Moisson d'âme"; IconPath = 'Interface\Icons\nhi_spiritfire_Border'; FallbackIconSpell = 47541; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier $fSoulHarvest 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    @{ Id = 98047; Clone = $absorb; Name = 'Forme spectrale'; IconPath = 'Interface\Icons\ability_argus_deathfog'; FallbackIconSpell = 48707; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 69; TargetA = 1; BasePoints = 0; Misc = 127 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1; 131 = (Look 'GhastlyForm') }) },
    @{ Id = 98048; Clone = $selfBuff; Name = 'Fantôme'; IconPath = 'Interface\Icons\_SoulsDevourer_Purple'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Les dégâts de votre prochaine technique sont augmentés de 30%.'; AuraDescription = 'Les dégâts de votre prochaine technique sont augmentés de 30%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 108; TargetA = 1; Value = 30; Misc = 0 })
       Fields = (Carrier 0 0 0 @{ 36 = 1; 40 = 1; 46 = 1; 122 = 0; 123 = 0; 124 = $fAll; 131 = (Look 'Ghost') }) },
    @{ Id = 98049; Clone = $selfBuff; Name = 'Depuis les ombres'; IconPath = 'Interface\Icons\ability_priest_cascade_shadow'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Chances de coup critique augmentées de 10%.'; AuraDescription = 'Chances de coup critique augmentées de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 290; TargetA = 1; Value = 10 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1 }) },
    @{ Id = 98050; Clone = $selfBuff; Name = 'Domination'; IconPath = 'Interface\Icons\spell_warlock_demonwrath'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Armure augmentée de 15%.'; AuraDescription = 'Armure augmentée de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 101; TargetA = 1; Value = 15; Misc = 1 })
       Fields = (Carrier 0 0 0 @{ 40 = 8; 46 = 1 }) },
    @{ Id = 98051; Clone = $selfBuff; Name = 'Damné'; IconPath = 'Interface\Icons\novart_magicspell_12_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre vitesse de déplacement ne peut pas descendre sous 70%.'; AuraDescription = 'Votre vitesse de déplacement ne peut pas descendre sous 70%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 305; TargetA = 1; Value = 70 })
       Fields = (Carrier 0 0 0 @{ 40 = 32; 46 = 1 }) },
    @{ Id = 98052; Clone = $selfBuff; Name = "Récolteur d'âmes"; IconPath = 'Interface\Icons\ability_deathknight_soulreaper'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Génère 2 points de puissance runique par seconde.'; AuraDescription = 'Génère 2 points de puissance runique par seconde.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 24; TargetA = 1; Value = 20; Misc = $runic })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1; 98 = 1000 }) },
    @{ Id = 98054; Clone = $selfBuff; Name = "Revigoration d'essence"; IconPath = 'Interface\Icons\inv_misc_lesseressence'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },

    # --- Moisson ---------------------------------------------------------------------------------------------------
    @{ Id = 98100; Clone = $strike; Name = 'Massacre'; IconPath = 'Interface\Icons\ability_revendreth_warrior'; FallbackIconSpell = 5308; Cost = 200; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 1 Âme moissonnée. Éradiquez un ennemi sous 35% de ses points de vie : 130% des dégâts de l'arme plus des dégâts."
       Effects = (Strike 30 130); Fields = (Own 0 $fSlaughter 0 @{ 21 = 13; 131 = (Look 'Slaughter') }) },
    @{ Id = 98101; Clone = $whirl; Name = 'Moisson des corbeaux'; IconPath = 'Interface\Icons\inv_polearm_2h_warfrontshorde_c_01red'; FallbackIconSpell = 1680; Cost = 300; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Frappe tous les ennemis à 8 m : 55% des dégâts de l'arme. Chaque ennemi touché vous donne un Fragment d'âme, 3 au plus."
       Effects = (Strike 6 55 22); Fields = (Own 0 $fCrows 0 @{ 46 = 1; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 116 = 0; 117 = 0; 212 = 0; 131 = (Look 'CrowsHarvest') }) },
    @{ Id = 98102; Clone = $strike; Name = 'Lacération funeste'; IconPath = 'Interface\Icons\inv_knife_1h_revendrethquest_b_03'; FallbackIconSpell = 772; Cost = 0; Cooldown = 6000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Lacérez un ennemi : 140% des dégâts de l'arme plus des dégâts, et un bouclier sombre absorbe les prochains soins qu'il reçoit pendant 15 s. Génère 15 points de puissance runique."
       Effects = (Strike 17 140) + @(@{ Index = 2; Effect = 64; TargetA = 6 })
       Fields = (Own 0 $fDoomrend 0 @{ 118 = 98103; 131 = (Look 'Doomrend') }) },
    @{ Id = 98103; Clone = $selfBuff; Name = 'Lacération funeste'; IconPath = 'Interface\Icons\inv_knife_1h_revendrethquest_b_03'; FallbackIconSpell = 772; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Absorbe les prochains soins reçus.'; AuraDescription = 'Absorbe les prochains soins reçus.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 301; TargetA = 6; Value = 1; Misc = 127 })
       Fields = (Carrier 0 $fDoomrendShield 0 @{ 40 = 8; 213 = 0; 131 = (Look 'DoomrendShield') }) },
    @{ Id = 98104; Clone = $selfBuff; Name = "L'heure de la moisson"; IconPath = 'Interface\Icons\inv_mace_1h_revendreth_d_01'; FallbackIconSpell = 51271; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Vos dégâts et vos chances de coup critique augmentent de 15% pendant 15 s, et vos techniques qui consomment des Âmes moissonnées ont 50% de chances de ne pas les consommer."
       AuraDescription = "Dégâts et chances de coup critique augmentés de 15%. Vos techniques qui consomment des Âmes moissonnées ont 50% de chances de ne pas les consommer."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 15; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 290; TargetA = 1; Value = 15 })
       Fields = (Buff 0 $fHarvestTime 0 @{ 40 = 8; 205 = 0; 206 = 0; 131 = (Look 'HarvestTime') }) },
    @{ Id = 98105; Clone = $pummel; Name = 'Sectionner'; IconPath = 'Interface\Icons\_Virus_Red'; FallbackIconSpell = 47528; Cost = 100; Cooldown = 15000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Drainez les forces vitales d'un ennemi : vous lui volez des points de vie, interrompez son incantation et l'empêchez de lancer un sort de cette école pendant 4 s."
       Effects = @(
           @{ Index = 0; Effect = 68; TargetA = 6 },
           @{ Index = 1; Effect = 9; TargetA = 6; Value = 22 })
       Fields = (Own 0 $fSever 0 (@{ 102 = $one; 205 = 0; 206 = 0; 213 = 1; 225 = 32; 131 = (Look 'Sever') } + $noItem)) },
    @{ Id = 98106; Clone = $ground; Name = 'Champ de moisson'; IconPath = 'Interface\Icons\inv_shoulder_plate_raiddeathknightmythic_s_01'; FallbackIconSpell = 43265; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Créez un champ de moisson à l'endroit ciblé pendant 10 s : la hâte et la vitesse de déplacement des ennemis qui s'y trouvent sont réduites de 20%, et ceux qui tentent d'en sortir sont ramenés en son centre."
       AuraDescription = "Hâte et vitesse de déplacement réduites de 20%. Ramené au centre du champ s'il tente d'en sortir."
       Effects = @(
           @{ Index = 0; Effect = 27; Aura = 192; TargetA = 28; Value = -20 },
           @{ Index = 1; Effect = 27; Aura = 33; TargetA = 28; Value = -20 })
       Fields = (Own 0 $fGrounds 0 (@{ 40 = 1; 46 = 4; 92 = 13; 93 = 13; 98 = 0; 213 = 1; 225 = 32; 131 = (Look 'HarvestingGrounds') } + $noItem)) },
    @{ Id = 98108; Clone = $strike; Name = 'Faux frémissante'; IconPath = 'Interface\Icons\ability_xavius_crushingshadows'; FallbackIconSpell = 12294; Cost = 400; Cooldown = 10000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Frappez un ennemi 5 fois en 1,5 s : 30% des dégâts de l'arme par coup, chacun générant un Fragment d'âme."
       Effects = (Strike 0 30); Fields = (Own 0 $fShudder 0 @{ 131 = (Look 'ShudderScythe') }) },
    @{ Id = 98109; Clone = $strike; Name = 'Faux frémissante'; IconPath = 'Interface\Icons\ability_xavius_crushingshadows'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "30% des dégâts de l'arme."; Effects = (Strike 0 30)
       Fields = (Carrier 0 $fShudderHit 0 @{ 46 = 2; 68 = 2; 69 = 0x2A5F3; 213 = 2; 131 = (Look 'ShudderScytheHit') }) },
    @{ Id = 98110; Clone = $selfBuff; Name = 'Battage'; IconPath = 'Interface\Icons\inv_sword_2h_artifactsoulrend_d_02'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Faucher frappe aussi les ennemis à 8 m.'; AuraDescription = 'Faucher frappe aussi les ennemis à 8 m.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Carrier 0 $fThresh 0 @{ 40 = 1; 46 = 1; 131 = (Look 'Bloodshatter') }) },
    @{ Id = 98111; Clone = $whirl; Name = 'Battage'; IconPath = 'Interface\Icons\inv_sword_2h_artifactsoulrend_d_02'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "110% des dégâts de l'arme aux ennemis à 8 m."; Effects = (Strike 0 110 22)
       Fields = (Carrier 0 $fThresh 0 @{ 46 = 1; 68 = 2; 69 = 0x2A5F3; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 116 = 0; 117 = 0; 212 = 0; 213 = 2; 131 = (Look 'Thresh') }) },
    @{ Id = 98112; Clone = $selfBuff; Name = 'Frénésie sanglante'; IconPath = 'Interface\Icons\_D3mantraofretribution'; FallbackIconSpell = 55078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre toutes les 0,75 s."; AuraDescription = "Dégâts d'Ombre toutes les 0,75 s, puis une dernière explosion."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 3; TargetA = 6; Value = 10 })
       Fields = (Carrier 0 $fBloodFrenzy 0 @{ 40 = 32; 98 = 750; 213 = 1; 225 = 32; 131 = (Look 'BloodFrenzy') }) },
    @{ Id = 98113; Clone = $bolt; Name = 'Frénésie sanglante'; IconPath = 'Interface\Icons\_D3mantraofretribution'; FallbackIconSpell = 55078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 40 })
       Fields = (Carrier 0 $fBloodFrenzy 0 @{ 47 = 0; 213 = 1; 225 = 32; 131 = (Look 'BloodFrenzyTick') }) },
    # Faux déchirante (talent): a bleed of up to 5 stacks; mod-reaper sizes each tick from attack power
    @{ Id = 98114; Clone = 1943; Name = 'Faux déchirante'; IconPath = 'Interface\Icons\inv_staff_114'; FallbackIconSpell = 1943; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; BleedAura = $true
       Description = 'Saigne toutes les 2 s.'; AuraDescription = 'Saigne toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 3; TargetA = 6; BasePoints = 0 })
       Fields = (Carrier 0 $fDarkrend 0 @{ 3 = 15; 40 = 1; 46 = 13; 49 = 5; 83 = 15; 98 = 2000; 131 = (Look 'DarkrendScythe') }) },
    @{ Id = 98115; Clone = $selfBuff; Name = 'Extinction'; IconPath = 'Interface\Icons\novart_physical_ability_2_Border'; FallbackIconSpell = 5308; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Votre prochain Massacre est utilisable quels que soient les points de vie de la cible et coûte 10 points de puissance runique de moins."
       AuraDescription = "Votre prochain Massacre est utilisable quels que soient les points de vie de la cible et coûte 10 points de puissance runique de moins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 262; TargetA = 1 },
           @{ Index = 1; Effect = 6; Aura = 107; TargetA = 1; Value = -100; Misc = 14 })
       Fields = (Carrier 0 0 0 @{ 36 = 1; 40 = 1; 46 = 1; 122 = 0; 123 = $fSlaughter; 124 = 0; 125 = 0; 126 = $fSlaughter; 127 = 0 }) },
    @{ Id = 98116; Clone = $selfBuff; Name = 'Soif cramoisie'; IconPath = 'Interface\Icons\5_engineerskill03_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre prochain Meurtre inflige 10% de dégâts en plus et coûte 10% de puissance runique en moins par application.'
       AuraDescription = 'Votre prochain Meurtre inflige 10% de dégâts en plus et coûte 10% de puissance runique en moins par application.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 108; TargetA = 1; Value = 10; Misc = 0 },
           @{ Index = 1; Effect = 6; Aura = 108; TargetA = 1; Value = -10; Misc = 14 })
       Fields = (Carrier 0 0 0 @{ 40 = 31; 46 = 1; 49 = 5; 122 = $fMurder; 123 = 0; 124 = 0; 125 = $fMurder; 126 = 0; 127 = 0 }) },
    @{ Id = 98117; Clone = $selfBuff; Name = 'Ruine'; IconPath = 'Interface\Icons\Ability_Rogue_Rupture'; FallbackIconSpell = 1943; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts subis de Lacération funeste et de Massacre augmentés.'; AuraDescription = 'Dégâts subis de Lacération funeste et de Massacre augmentés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 6 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 213 = 0 }) },
    @{ Id = 98118; Clone = $selfBuff; Name = 'Frénésie ruineuse'; IconPath = 'Interface\Icons\Ability_Rogue_Rupture'; FallbackIconSpell = 1943; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Hâte augmentée de 15%.'; AuraDescription = 'Hâte en mêlée augmentée de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 138; TargetA = 1; Value = 15 })
       Fields = (Carrier 0 0 0 @{ 40 = 32; 46 = 1; 131 = (Look 'RuinousFrenzy') }) },
    @{ Id = 98119; Clone = $selfBuff; Name = 'Âmes pour le massacre'; IconPath = 'Interface\Icons\novart_helmet_7_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Vos attaques automatiques infligent des dégâts d'Ombre supplémentaires."; AuraDescription = "Vos attaques automatiques infligent des dégâts d'Ombre supplémentaires."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1 }) },
    @{ Id = 98120; Clone = $bolt; Name = 'Âmes pour le massacre'; IconPath = 'Interface\Icons\novart_helmet_7_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 47 = 0; 213 = 1; 225 = 32 }) },
    @{ Id = 98121; Clone = $selfBuff; Name = 'Moissonneur'; IconPath = 'Interface\Icons\ability_revendreth_deathknight'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    # The group auras of three talents, kept up by mod-reaper while the talent is known (Trueshot Aura's layout)
    @{ Id = 98123; Clone = 19506; Name = 'Moissonneur sanglant'; IconPath = 'Interface\Icons\ability_ironmaidens_whirlofblood'; FallbackIconSpell = 17007; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Chances de coup critique augmentées de 3%.'; AuraDescription = 'Chances de coup critique augmentées de 3%.'
       Effects = @(@{ Index = 0; Effect = 65; Aura = 290; TargetA = 1; Value = 3 })
       Fields = (Carrier 0 0 0 @{ 4 = 0x40; 40 = 21; 41 = 0; 46 = 1; 92 = 23 }) },
    @{ Id = 98125; Clone = 19506; Name = 'Présence de la mort'; IconPath = 'Interface\Icons\nhi_spiritamulet_Border'; FallbackIconSpell = 31869; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts infligés augmentés de 3%.'; AuraDescription = 'Dégâts infligés augmentés de 3%.'
       Effects = @(@{ Index = 0; Effect = 65; Aura = 79; TargetA = 1; Value = 3; Misc = 127 })
       Fields = (Carrier 0 0 0 @{ 4 = 0x40; 40 = 21; 41 = 0; 46 = 1; 92 = 23 }) },
    @{ Id = 98126; Clone = 19506; Name = 'Garde éthérée'; IconPath = 'Interface\Icons\ability_racial_etherealconnection'; FallbackIconSpell = 53138; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts subis réduits de 3%.'; AuraDescription = 'Dégâts subis réduits de 3%.'
       Effects = @(@{ Index = 0; Effect = 65; Aura = 87; TargetA = 1; Value = -3; Misc = 127 })
       Fields = (Carrier 0 0 0 @{ 4 = 0x40; 40 = 21; 41 = 0; 46 = 1; 92 = 23 }) },
    @{ Id = 98127; Clone = $selfBuff; Name = 'Soif cramoisie'; IconPath = 'Interface\Icons\5_engineerskill03_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },

    # --- Âme ---------------------------------------------------------------------------------------------------------
    @{ Id = 98200; Clone = $strike; Name = 'Complainte'; IconPath = 'Interface\Icons\ability_demonhunter_soulcleave4'; FallbackIconSpell = 49143; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 2 Fragments d'âme et 10 points de puissance runique. Frappez l'ennemi de vos armes : 70% des dégâts de chacune en dégâts d'Ombregivre, 130% avec une dague."
       Effects = @(
           @{ Index = 0; Effect = 31; TargetA = 6; Value = 70 },
           (Energize 1 10))
       Fields = (Own 0 $fDirge 0 @{ 225 = 48; 131 = (Look 'Dirge') }) },
    @{ Id = 98201; Clone = $strike; Name = 'Complainte'; IconPath = 'Interface\Icons\ability_demonhunter_soulcleave4'; FallbackIconSpell = 49143; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "70% des dégâts de l'arme de main gauche en dégâts d'Ombregivre."
       Effects = @(@{ Index = 0; Effect = 31; TargetA = 6; Value = 70 })
       Fields = (Carrier 0 $fDirgeHit 0 @{ 7 = 0x1000000; 46 = 2; 68 = 2; 69 = 0x2A5F3; 213 = 2; 225 = 48 }) },
    @{ Id = 98202; Clone = $selfBuff; Name = 'Chasse-mort'; IconPath = 'Interface\Icons\nhi_spiritstab_Border'; FallbackIconSpell = 55095; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Forcez la mort sur un ennemi à 30 m : des dégâts d'Ombregivre toutes les 2 s pendant 12 s. Génère 15 points de puissance runique."
       AuraDescription = "Dégâts d'Ombregivre toutes les 2 s."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 3; TargetA = 6; Value = 15 },
           (Energize 1 15))
       Fields = (Own 0 $fDeathchaser 0 (@{ 40 = 29; 46 = 4; 98 = 2000; 213 = 1; 225 = 48; 131 = (Look 'Deathchaser') } + $noItem)) },
    @{ Id = 98203; Clone = $selfBuff; Name = 'Porteur de fin'; IconPath = 'Interface\Icons\nhi_frozenghost_Border'; FallbackIconSpell = 51271; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Devenez un Porteur de fin pendant 15 s : Complainte inflige 30% de dégâts en plus, sa portée augmente de 5 m, elle vous soigne de 75% des dégâts infligés et génère une Âme moissonnée."
       AuraDescription = "Complainte inflige 30% de dégâts en plus, porte 5 m plus loin, vous soigne et génère une Âme moissonnée."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 108; TargetA = 1; Value = 30; Misc = 0 },
           @{ Index = 1; Effect = 6; Aura = 107; TargetA = 1; Value = 5; Misc = 5 })
       Fields = (Buff 0 $fEndbringer 0 @{ 40 = 8; 122 = 0; 123 = ($fDirge -bor $fDirgeHit); 124 = 0; 125 = 0; 126 = $fDirge; 127 = 0; 205 = 0; 206 = 0; 131 = (Look 'Endbringer') }) },
    @{ Id = 98204; Clone = $selfBuff; Name = 'Arme fantomatique'; IconPath = 'Interface\Icons\custom_66_spirit_sword_Border'; FallbackIconSpell = 49143; Cost = 200; Cooldown = 40000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Imprégnez votre arme d'un pouvoir fantomatique pendant 15 s : vos attaques en mêlée infligent 25% de dégâts supplémentaires sous forme de dégâts de Givre."
       AuraDescription = "Vos attaques en mêlée infligent 25% de dégâts supplémentaires sous forme de dégâts de Givre."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Buff 0 $fGhostly 0 @{ 40 = 8; 205 = 0; 206 = 0; 131 = (Look 'GhostlyWeapon') }) },
    @{ Id = 98205; Clone = $bolt; Name = 'Arme fantomatique'; IconPath = 'Interface\Icons\custom_66_spirit_sword_Border'; FallbackIconSpell = 49143; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts de Givre.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = (Carrier 0 $fGhostlyHit 0 @{ 47 = 0; 213 = 1; 225 = 16 }) },
    @{ Id = 98206; Clone = $selfBuff; Name = 'Ombre'; IconPath = 'Interface\Icons\_Shadow_LightBlue'; FallbackIconSpell = 26889; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Devenez une ombre : vous entrez en Marche funèbre, même en combat, et les effets de ralentissement et d''immobilisation sont dissipés.'
       Effects = @(
           @{ Index = 0; Effect = 108; TargetA = 1; Misc = 7 },
           @{ Index = 1; Effect = 108; TargetA = 1; Misc = 11 },
           @{ Index = 2; Effect = 3; TargetA = 1 })
       Fields = (Buff 0 $fShade 0 @{ 205 = 0; 206 = 0; 131 = (Look 'Shade') }) },
    @{ Id = 98207; Clone = $selfBuff; Name = 'Renouveau sépulcral'; IconPath = 'Interface\Icons\nhi_soulblessing_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 480000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Réinitialise la recharge de Limbes, de Fracas d'âmes et d'Ombre. Utilisable camouflé."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Buff 0 $fRenewal 0 @{ 5 = 0x20; 205 = 0; 206 = 0; 131 = (Look 'SepulchralRenewal') }) },
    # Lieu de sépulture (talent): the marked ground (mod-reaper looks for it under the enemies a critical strike hits)
    @{ Id = 98208; Clone = $ground; Name = 'Lieu de sépulture'; IconPath = 'Interface\Icons\nhi_soulgrave_Border'; FallbackIconSpell = 43265; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre lieu de sépulture.'; AuraDescription = "Dans le lieu de sépulture d'un Faucheur."
       Effects = @(@{ Index = 0; Effect = 27; Aura = 4; TargetA = 18 })
       Fields = (Carrier 0 $fGravesite 0 @{ 16 = 0; 40 = 8; 46 = 1; 92 = 13; 131 = (Look 'Gravesite') }) },
    @{ Id = 98209; Clone = $bolt; Name = 'Apparition'; IconPath = 'Interface\Icons\nhi_soulgrave_Border'; FallbackIconSpell = 47541; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 20 })
       Fields = (Carrier 0 $fApparition 0 @{ 213 = 1; 225 = 32; 131 = (Look 'GravesiteApparition') }) },
    @{ Id = 98210; Clone = $selfBuff; Name = "Pourriture d'âme"; IconPath = 'Interface\Icons\nhi_souldrain_Border'; FallbackIconSpell = 55095; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "À la fin de l'effet, des dégâts d'Ombre pour chaque application."; AuraDescription = "À la fin de l'effet, la cible subit des dégâts d'Ombre pour chaque application."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 6 })
       Fields = (Carrier 0 $fSoulrot 0 @{ 40 = 29; 49 = 20; 213 = 1; 131 = (Look 'Soulrot') }) },
    @{ Id = 98211; Clone = $bolt; Name = "Pourriture d'âme"; IconPath = 'Interface\Icons\nhi_souldrain_Border'; FallbackIconSpell = 55095; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = (Carrier 0 $fSoulrot 0 @{ 47 = 0; 213 = 1; 225 = 32; 131 = (Look 'JailersCall') }) },
    @{ Id = 98212; Clone = $selfBuff; Name = "Embuscade d'anima"; IconPath = 'Interface\Icons\ability_warlock_darkarts'; FallbackIconSpell = 36554; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre chaque seconde."; AuraDescription = "Dégâts d'Ombre chaque seconde."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 3; TargetA = 6; BasePoints = 0 })
       Fields = (Carrier 0 $fAnimaAmbush 0 @{ 40 = 32; 98 = 1000; 213 = 1; 225 = 32 }) },
    @{ Id = 98213; Clone = $selfBuff; Name = 'La fin est proche'; IconPath = 'Interface\Icons\ability_warlock_soullink'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une version spectrale de vous-même combat à vos côtés.'; AuraDescription = 'Une version spectrale de vous-même lance Complainte et Meurtre sur votre cible.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 226; TargetA = 1 })
       Fields = (Carrier 0 $fSpectralSelf 0 @{ 40 = 8; 46 = 1; 98 = 2000; 131 = (Look 'Endbringer') }) },
    @{ Id = 98214; Clone = $selfBuff; Name = 'Purgatoire'; IconPath = 'Interface\Icons\ability_warlock_improvedsoulleech'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts de Déchirure d'âme et de Reliquaire des perdus augmentés de 20%."; AuraDescription = "Dégâts de Déchirure d'âme et de Reliquaire des perdus augmentés de 20% par application."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 108; TargetA = 1; Value = 20; Misc = 0 })
       Fields = (Carrier 0 0 0 @{ 40 = 8; 46 = 1; 49 = 2; 122 = ($fSoulrend -bor $fReliquary -bor $fSoulBolt); 123 = 0; 124 = 0 }) },
    @{ Id = 98215; Clone = $bolt; Name = 'La fin est proche'; IconPath = 'Interface\Icons\ability_warlock_soullink'; FallbackIconSpell = 49143; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombregivre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 20 })
       Fields = (Carrier 0 $fSpectralSelf 0 @{ 213 = 1; 225 = 48; 131 = (Look 'Murder') }) },
    @{ Id = 98216; Clone = $selfBuff; Name = 'Au-delà du voile'; IconPath = 'Interface\Icons\_SoulHarvest2_Blue'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Vitesse de déplacement augmentée de 30%. Marche sur l'eau. Rend 2% des points de vie maximum toutes les 3 s."
       AuraDescription = "Vitesse de déplacement augmentée de 30%. Marche sur l'eau. Rend 2% des points de vie maximum toutes les 3 s."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 31; TargetA = 1; Value = 30 },
           @{ Index = 1; Effect = 6; Aura = 104; TargetA = 1 },
           @{ Index = 2; Effect = 6; Aura = 20; TargetA = 1; Value = 2 })
       Fields = (Carrier 0 0 0 @{ 40 = 21; 46 = 1; 100 = 3000 }) },
    @{ Id = 98217; Clone = $selfBuff; Name = 'Réflexes spirituels'; IconPath = 'Interface\Icons\nhi_curseweak_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Chances d'esquiver augmentées de 25%."; AuraDescription = "Chances d'esquiver augmentées de 25%. Esquiver déchaîne Moisson d'âme."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 49; TargetA = 1; Value = 25 })
       Fields = (Carrier 0 0 0 @{ 40 = 21; 46 = 1 }) },
    @{ Id = 98218; Clone = $bolt; Name = 'Appel du Geôlier'; IconPath = 'Interface\Icons\inv_mawrat'; FallbackIconSpell = 47541; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 10 })
       Fields = (Carrier 0 0 0 @{ 47 = 0; 213 = 1; 225 = 32; 131 = (Look 'JailersCall') }) },
    @{ Id = 98219; Clone = $bolt; Name = 'Garde fantomatique'; IconPath = 'Interface\Icons\inv_qiraj_hiltornate'; FallbackIconSpell = 49143; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombregivre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 47 = 0; 213 = 1; 225 = 48 }) },
    @{ Id = 98220; Clone = $selfBuff; Name = 'Lamentation'; IconPath = 'Interface\Icons\Spell_Shadow_PsychicScream'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    @{ Id = 98221; Clone = $selfBuff; Name = 'Porteur de fin'; IconPath = 'Interface\Icons\nhi_frozenghost_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    @{ Id = 98223; Clone = $selfBuff; Name = 'Âme affaiblie'; IconPath = 'Interface\Icons\nhi_souldarkening_Border'; FallbackIconSpell = 49998; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre et de Givre subis du Faucheur augmentés de 10%."; AuraDescription = "Dégâts d'Ombre et de Givre subis du Faucheur augmentés de 10%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 271; TargetA = 6; Value = 10; Misc = 48 })
       Fields = (Carrier $fSoulrendDebuff 0 0 @{ 40 = 8; 213 = 0 }) },
    @{ Id = 98224; Clone = $selfBuff; Name = 'Armes forgées d''âme'; IconPath = 'Interface\Icons\inv_weapon_shortblade_105'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre prochain Meurtre est gratuit.'; AuraDescription = 'Votre prochain Meurtre est gratuit.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 108; TargetA = 1; Value = -100; Misc = 14 })
       Fields = (Carrier 0 0 0 @{ 36 = 1; 40 = 31; 46 = 1; 122 = $fMurder; 123 = 0; 124 = 0 }) },

    # --- Domination --------------------------------------------------------------------------------------------------
    @{ Id = 98300; Clone = $strike; Name = "Frappe d'âme"; IconPath = 'Interface\Icons\spell_deathknight_frozenruneweapon'; FallbackIconSpell = 49998; Cost = 400; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 1 Âme moissonnée. Frappez l'ennemi : 75% des dégâts de l'arme plus des dégâts, en dégâts d'Ombre, et vous récupérez 40% des dégâts infligés plus 10% de vos points de vie manquants."
       Effects = (Strike 30 75); Fields = (Own 0 0 $fSoulStrike @{ 225 = 32; 131 = (Look 'SoulStrike') }) },
    @{ Id = 98301; Clone = $selfBuff; Name = 'Faux spectrale'; IconPath = 'Interface\Icons\inv_archaeology_80_witch_bonescythe'; FallbackIconSpell = 49206; Cost = 0; Cooldown = 45000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme vos Âmes moissonnées : chacune invoque une Faux spectrale qui frappe votre cible pendant 15 s. Frappe d'âme fait frapper vos Faux spectrales la cible et 5 ennemis proches. La menace qu'elles génèrent vous revient."
       AuraDescription = 'Des Faux spectrales combattent à vos côtés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 226; TargetA = 1 })
       Fields = (Buff 0 0 $fScythe @{ 24 = 98017; 40 = 8; 49 = 3; 98 = 1500; 205 = 0; 206 = 0; 131 = (Look 'SpectralScythe') }) },
    @{ Id = 98302; Clone = $bolt; Name = 'Faux spectrale'; IconPath = 'Interface\Icons\inv_archaeology_80_witch_bonescythe'; FallbackIconSpell = 49206; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 20 })
       Fields = (Carrier 0 0 $fScytheHit @{ 47 = 0; 213 = 1; 225 = 32; 131 = (Look 'ReapedSoul') }) },
    @{ Id = 98303; Clone = $selfBuff; Name = 'Gardien spectral'; IconPath = 'Interface\Icons\custom_T_SpiritualArmor_Border'; FallbackIconSpell = 48707; Cost = 200; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Invoquez un gardien spectral qui frappe votre cible pendant 15 s, et placez sur 5 alliés à 15 m un bouclier qui absorbe des dégâts. La menace qu'il génère vous revient."
       AuraDescription = 'Un gardien spectral combat à vos côtés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 226; TargetA = 1 })
       Fields = (Buff 0 0 $fWarden @{ 40 = 8; 49 = 3; 98 = 1500; 205 = 0; 206 = 0; 131 = (Look 'Veilwalk') }) },
    @{ Id = 98304; Clone = $absorb; Name = 'Gardien spectral'; IconPath = 'Interface\Icons\custom_T_SpiritualArmor_Border'; FallbackIconSpell = 48707; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe des dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 69; TargetA = 21; BasePoints = 0; Misc = 127 })
       Fields = (Carrier 0 0 $fWarden @{ 40 = 8; 46 = 13; 131 = (Look 'JailersBargain') }) },
    @{ Id = 98305; Clone = $strike; Name = "Sillage d'effroi"; IconPath = 'Interface\Icons\ability_argus_edgeofobliteration'; FallbackIconSpell = 49184; Cost = 300; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Génère 1 Âme moissonnée. Déchaînez une vague d'âmes sur les ennemis dans un cône de 10 m : 80% des dégâts de l'arme plus des dégâts de Givre."
       Effects = (Strike 5 80 24); Fields = (Own 0 0 $fDreadwake @{ 46 = 1; 92 = 13; 93 = 13; 212 = 0; 225 = 16; 131 = (Look 'Dreadwake') }) },
    @{ Id = 98306; Clone = $burst; Name = 'Requiem'; IconPath = 'Interface\Icons\inv_misc_supersoulash'; FallbackIconSpell = 49184; Cost = 0; Cooldown = 6000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Frappez les ennemis à 10 m dans une furie d'âmes : des dégâts d'Ombregivre, et leur vitesse d'attaque est réduite de 20% pendant 12 s. Génère 10 points de puissance runique."
       AuraDescription = "Vitesse d'attaque réduite de 20%."
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 25 },
           @{ Index = 1; Effect = 6; Aura = 192; TargetA = 22; Value = -20 },
           (Energize 2 10))
       Fields = (Buff 0 0 $fRequiem @{ 40 = 29; 89 = 15; 90 = 15; 92 = 13; 93 = 13; 212 = 0; 213 = 1; 225 = 48; 131 = (Look 'Requiem') }) },
    @{ Id = 98308; Clone = $selfBuff; Name = 'Forme renforcée'; IconPath = 'Interface\Icons\achievement_raid_torghast_shadowscourge_prisonofnerzhul'; FallbackIconSpell = 55233; Cost = 0; Cooldown = 60000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Adoptez une forme renforcée pendant 12 s : génère 30 points de puissance runique, et votre armure et vos chances de parer augmentent de 25%.'
       AuraDescription = 'Armure et chances de parer augmentées de 25%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 101; TargetA = 1; Value = 25; Misc = 1 },
           (Energize 1 30),
           @{ Index = 2; Effect = 6; Aura = 47; TargetA = 1; Value = 25 })
       Fields = (Buff 0 0 $fBolstered @{ 40 = 29; 205 = 0; 206 = 0; 131 = (Look 'BolsteredForm') }) },
    @{ Id = 98309; Clone = $strike; Name = 'Décimer'; IconPath = 'Interface\Icons\inv_mace_42'; FallbackIconSpell = 49184; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "117% des dégâts de l'arme dans un cône de 10 m."; Effects = (Strike 22 117 24)
       Fields = (Carrier 0 0 $fDecimate @{ 46 = 1; 68 = 2; 69 = 0x2A5F3; 92 = 13; 93 = 13; 212 = 0; 213 = 2; 131 = (Look 'Decimate') }) },
    @{ Id = 98310; Clone = 2983; Name = 'Décimation'; IconPath = 'Interface\Icons\inv_plate_raiddeathknight_o_01buckle'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 6
       Description = "Au 6e Faucher, votre prochaine Frappe d'âme devient Décimer."; AuraDescription = "Au 6e Faucher, votre prochaine Frappe d'âme devient Décimer."
       Fields = @{ 40 = 21; 208 = $family } },
    @{ Id = 98311; Clone = 2983; Name = 'Cotte de douleur'; IconPath = 'Interface\Icons\inv_shoulder_armor_maw_c_14'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 8
       Description = "À l'expiration, chaque application vous soigne et réduit la recharge de Forme renforcée."; AuraDescription = "À l'expiration, chaque application vous soigne et réduit la recharge de Forme renforcée."
       Fields = @{ 40 = 8; 208 = $family } },
    @{ Id = 98312; Clone = $selfBuff; Name = 'Intimidé'; IconPath = 'Interface\Icons\nhi_soulgrave_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts subis du Faucheur augmentés de 15%.'; AuraDescription = 'Dégâts subis du Faucheur augmentés de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 271; TargetA = 6; Value = 15; Misc = 127 })
       Fields = (Carrier 0 0 0 @{ 40 = 29; 213 = 0 }) },
    @{ Id = 98313; Clone = $selfBuff; Name = "Chevalier d'âme"; IconPath = 'Interface\Icons\inv_sword_2h_artifactashbringerlightning_d_03'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soins reçus augmentés de 2% et dégâts magiques subis réduits de 2% par application.'; AuraDescription = 'Soins reçus augmentés de 2% et dégâts magiques subis réduits de 2% par application.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 118; TargetA = 1; Value = 2; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 87; TargetA = 1; Value = -2; Misc = 126 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1; 49 = 5 }) },
    @{ Id = 98314; Clone = $selfBuff; Name = 'Scelle-destin'; IconPath = 'Interface\Icons\ability_deathknight_hungeringruneblade'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts subis réduits de 2% par application.'; AuraDescription = 'Dégâts subis réduits de 2% par application.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 87; TargetA = 1; Value = -2; Misc = 127 })
       Fields = (Carrier 0 0 0 @{ 40 = 31; 46 = 1; 49 = 3 }) },
    @{ Id = 98315; Clone = $selfBuff; Name = "Échardes d'âme"; IconPath = 'Interface\Icons\oshugun_crystalfragments'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Inflige des dégâts d'Ombre aux ennemis à 8 m."; AuraDescription = "Inflige des dégâts d'Ombre aux ennemis à 8 m toutes les 2 s."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 226; TargetA = 1 })
       Fields = (Carrier 0 0 $fSplinter @{ 40 = 32; 46 = 1; 98 = 2000; 131 = (Look 'SoulSplinter') }) },
    @{ Id = 98325; Clone = $burst; Name = "Échardes d'âme"; IconPath = 'Interface\Icons\oshugun_crystalfragments'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 22; BasePoints = 0 })
       Fields = (Carrier 0 0 $fSplinter @{ 46 = 1; 89 = 15; 92 = 14; 212 = 0; 213 = 1; 225 = 32 }) },
    @{ Id = 98316; Clone = $selfBuff; Name = "Dévoreur d'âmes"; IconPath = 'Interface\Icons\Spell_Shadow_DevouringPlague'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques subis réduits de 10%. Rend 2% des points de vie maximum toutes les 2 s.'; AuraDescription = 'Dégâts physiques subis réduits de 10%. Rend 2% des points de vie maximum toutes les 2 s.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 87; TargetA = 1; Value = -10; Misc = 1 },
           @{ Index = 1; Effect = 6; Aura = 20; TargetA = 1; Value = 2 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1; 99 = 2000 }) },
    @{ Id = 98317; Clone = $selfBuff; Name = "Liant d'essence"; IconPath = 'Interface\Icons\novart_magicspell_53_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Points de vie maximum et Force augmentés de 15%.'; AuraDescription = 'Points de vie maximum et Force augmentés de 15%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 133; TargetA = 1; Value = 15 },
           @{ Index = 1; Effect = 6; Aura = 137; TargetA = 1; Value = 15; Misc = 0 })
       Fields = (Carrier 0 0 0 @{ 40 = 29; 46 = 1 }) },
    @{ Id = 98319; Clone = $selfBuff; Name = "Siphon d'anima"; IconPath = 'Interface\Icons\spell_animabastion_debuff'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Vous récupérez 5% de vos dégâts infligés.'; AuraDescription = 'Vous récupérez 5% de vos dégâts infligés.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Carrier 0 0 0 @{ 40 = 1; 46 = 1 }) },
    @{ Id = 98320; Clone = $selfBuff; Name = 'Royaume des esprits'; IconPath = 'Interface\Icons\Spell_Shadow_ShadowEmbrace'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soins reçus augmentés de 10%.'; AuraDescription = 'Soins reçus augmentés de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 118; TargetA = 1; Value = 10; Misc = 127 })
       Fields = (Carrier 0 0 0 @{ 40 = 39; 46 = 1 }) },
    @{ Id = 98321; Clone = $selfBuff; Name = 'Marché difficile'; IconPath = 'Interface\Icons\inv_trinket_mawraid_04_blue'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Vitesse d'attaque en mêlée augmentée de 30%. Sillage d'effroi coûte 5 points de puissance runique de moins."; AuraDescription = "Vitesse d'attaque en mêlée augmentée de 30%. Sillage d'effroi coûte 5 points de puissance runique de moins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 138; TargetA = 1; Value = 30 },
           @{ Index = 1; Effect = 6; Aura = 107; TargetA = 1; Value = -50; Misc = 14 })
       Fields = (Carrier 0 0 0 @{ 40 = 85; 46 = 1; 125 = 0; 126 = 0; 127 = $fDreadwake }) },
    @{ Id = 98322; Clone = $selfBuff; Name = 'Décimer'; IconPath = 'Interface\Icons\inv_mace_42'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Votre prochaine Frappe d'âme fracasse aussi le sol devant vous."; AuraDescription = "Votre prochaine Frappe d'âme fracasse aussi le sol devant vous."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 })
       Fields = (Carrier 0 0 0 @{ 40 = 8; 46 = 1; 131 = (Look 'Decimate') }) },
    @{ Id = 98323; Clone = $selfBuff; Name = 'Ponction de vie'; IconPath = 'Interface\Icons\novart_physical_ability_41_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    @{ Id = 98324; Clone = $bolt; Name = 'Gardien spectral'; IconPath = 'Interface\Icons\custom_T_SpiritualArmor_Border'; FallbackIconSpell = 48707; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts d'Ombre."; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 20 })
       Fields = (Carrier 0 0 $fWardenHit @{ 47 = 0; 213 = 1; 225 = 32; 131 = (Look 'SoulSplinter') }) },
    @{ Id = 98326; Clone = $selfBuff; Name = "Siphon d'anima"; IconPath = 'Interface\Icons\spell_animabastion_debuff'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    @{ Id = 98328; Clone = $selfBuff; Name = "Frappe d'âme"; IconPath = 'Interface\Icons\spell_deathknight_frozenruneweapon'; FallbackIconSpell = 49998; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 1; BasePoints = 0 })
       Fields = (Carrier 0 0 0 @{ 46 = 1; 213 = 1; 225 = 32 }) },
    # Domination's taunt (Ascension's Reaper had only the lure; a tank in our dungeons needs one for a single enemy)
    @{ Id = 98327; Clone = $taunt; Name = 'Injonction des âmes'; IconPath = 'Interface\Icons\nhi_soulgrave_Border'; FallbackIconSpell = 355; Cost = 0; Cooldown = 8000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Ordonnez à un ennemi à 30 m de vous attaquer pendant 3 s."
       Fields = (Own 0 0 0 (@{ 46 = 4; 205 = 0; 206 = 0; 213 = 1; 131 = (Look 'ScytheRushMark') } + $noItem)) },

    # --- The specializations: what tells the scripts which one is on --------------------------------------------------
    @{ Id = 98900; Clone = $selfBuff; Name = 'Moissonneur'; IconPath = 'Interface\Icons\ability_revendreth_deathknight'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = 'Vous vous soignez de 10% de tous les dégâts que vous infligez.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 }); Fields = @{ 208 = $family } },
    @{ Id = 98901; Clone = $selfBuff; Name = 'Âmes affaiblies'; IconPath = 'Interface\Icons\nhi_souldarkening_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Déchirure d'âme applique aussi Âme affaiblie : la cible subit 10% de dégâts d'Ombre et de Givre supplémentaires de votre part pendant 15 s."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1 }); Fields = @{ 208 = $family } },
    # Domination: the tank's armor, parry and threat
    @{ Id = 98902; Clone = $selfBuff; Name = 'Tourmenteur'; IconPath = 'Interface\Icons\nhi_ghosthand_Border'; FallbackIconSpell = 2983; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Votre armure augmente de 30%, vos chances de parer de 5% et la menace que vous générez de 80%. Consommer vos Âmes moissonnées réduit de 3 s la recharge d'Âmes tourmentées."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 101; TargetA = 1; Value = 30; Misc = 1 },
           @{ Index = 1; Effect = 6; Aura = 47; TargetA = 1; Value = 5 },
           @{ Index = 2; Effect = 6; Aura = 10; TargetA = 1; Value = 80; Misc = 127 })
       Fields = @{ 208 = $family } }
)

# Talent ranks come from the talent trees (talentTree.json), which the server and the talent window read as well
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\reaper\talentTree.json') -Family $family

$spells
