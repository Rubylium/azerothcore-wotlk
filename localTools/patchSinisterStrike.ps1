$ErrorActionPreference = 'Stop'

# Generates the custom spell data shared by the server and the client:
# - Combat rogue rework ("Crimson Duelist", see .agents/plans/combat-rogue-rework): new and updated abilities, hidden
#   support spells, and the whole Combat talent tree rewritten on the WotLK talent spell ids
# - Victory Rush, Gladiator Stance and Quick Travel
# - Custom spell visuals (SpellVisual.dbc / SpellVisualKit.dbc, only read by the client)
# Always rebuilt from the untouched backups, so it is safe to run again after any change.
#
# Icons: an ability or talent uses its generated icon when
# modules/mod-stat-growth/client-assets/compiled/<Icon>.tga exists (see localTools/buildRogueClientAssets.ps1),
# otherwise it keeps a stock game icon, so the data works before the icons are generated.

$repoRoot = Split-Path -Parent $PSScriptRoot
$serverDbcRoot = Join-Path $repoRoot 'server\Data\dbc'
$clientDbcRoot = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK\Data\DBFilesClient'
$compiledIconRoot = Join-Path $repoRoot 'modules\mod-stat-growth\client-assets\compiled'
$pestifereCompiledIconRoot = Join-Path $repoRoot 'modules\mod-pestifere\client-assets\compiled\icons'
$necromancerCompiledIconRoot = Join-Path $repoRoot 'modules\mod-necromancer\client-assets\compiled\icons'
$warriorIconRoot = Join-Path $repoRoot 'modules\mod-warrior\client-assets\files\Interface\Icons'
$serverSpellPath = Join-Path $serverDbcRoot 'Spell.dbc'
$spellBackupPath = Join-Path $serverDbcRoot 'Spell.before-sinister-strike.dbc'
$serverSkillPath = Join-Path $serverDbcRoot 'SkillLineAbility.dbc'
$skillBackupPath = Join-Path $serverDbcRoot 'SkillLineAbility.before-rogue-momentum.dbc'
$serverIconPath = Join-Path $serverDbcRoot 'SpellIcon.dbc'
$iconBackupPath = Join-Path $serverDbcRoot 'SpellIcon.before-rogue-momentum.dbc'
$clientSpellPath = Join-Path $clientDbcRoot 'Spell.dbc'
$clientSkillPath = Join-Path $clientDbcRoot 'SkillLineAbility.dbc'
$clientIconPath = Join-Path $clientDbcRoot 'SpellIcon.dbc'
$serverVisualPath = Join-Path $serverDbcRoot 'SpellVisual.dbc'
$visualBackupPath = Join-Path $serverDbcRoot 'SpellVisual.before-combat-rogue.dbc'
$serverVisualKitPath = Join-Path $serverDbcRoot 'SpellVisualKit.dbc'
$visualKitBackupPath = Join-Path $serverDbcRoot 'SpellVisualKit.before-combat-rogue.dbc'
$clientVisualPath = Join-Path $clientDbcRoot 'SpellVisual.dbc'
$clientVisualKitPath = Join-Path $clientDbcRoot 'SpellVisualKit.dbc'
$serverSoundPath = Join-Path $serverDbcRoot 'SoundEntries.dbc'
$soundBackupPath = Join-Path $serverDbcRoot 'SoundEntries.before-combat-rogue.dbc'
$clientSoundPath = Join-Path $clientDbcRoot 'SoundEntries.dbc'
$serverEffectNamePath = Join-Path $serverDbcRoot 'SpellVisualEffectName.dbc'
$effectNameBackupPath = Join-Path $serverDbcRoot 'SpellVisualEffectName.before-oathblade.dbc'
$clientEffectNamePath = Join-Path $clientDbcRoot 'SpellVisualEffectName.dbc'
# Only the visuals imported from the Ascension client add to these two (localToolsscensionImport)
$serverAttachPath = Join-Path $serverDbcRoot 'SpellVisualKitModelAttach.dbc'
$attachBackupPath = Join-Path $serverDbcRoot 'SpellVisualKitModelAttach.before-ascension-import.dbc'
$clientAttachPath = Join-Path $clientDbcRoot 'SpellVisualKitModelAttach.dbc'
$serverMotionPath = Join-Path $serverDbcRoot 'SpellMissileMotion.dbc'
$motionBackupPath = Join-Path $serverDbcRoot 'SpellMissileMotion.before-ascension-import.dbc'
$clientMotionPath = Join-Path $clientDbcRoot 'SpellMissileMotion.dbc'

# Spell.dbc field indexes used below (3.3.5a, 234 fields)
$F_Attributes = 4; $F_AttributesEx = 5; $F_CasterAuraState = 20; $F_ProcFlags = 34; $F_ProcChance = 35
$F_DurationIndex = 40; $F_ManaCost = 42; $F_StackAmount = 49; $F_EquippedItemClass = 68
$F_EffectBasePoints = 80; $F_EffectRadius = 92; $F_SpellVisual = 131; $F_SpellIconID = 133
$F_Name = 136; $F_Description = 170; $F_AuraDescription = 187

$sinisterStrikeRanks = @(1752, 1757, 1758, 1759, 1760, 8621, 11293, 11294, 26861, 26862, 48637, 48638)
$eviscerateRanks = @(2098, 6760, 6761, 6762, 8623, 8624, 11299, 11300, 31016, 26865, 48667, 48668)
$sliceAndDiceRanks = @(5171, 6774)

# Power types (Spell.dbc field 41) and the rune cost field (226), which only Death Knight spells use
$POWER_RAGE = 1

# Aura type ids
$A_Dummy = 4; $A_ModIncreaseSpeed = 31; $A_ModParryPercent = 47; $A_ModDodgePercent = 49; $A_ModHitChance = 54
$A_ModDamagePercentTaken = 87; $A_AddFlatModifier = 107; $A_ModPowerRegenPercent = 110
$A_ModOffhandDamagePct = 122; $A_ModMeleeHaste = 138; $A_ModResistancePct = 142
$A_ModExpertise = 240; $A_ModCritPct = 290

# Spell modifier operations, the misc value of SPELL_AURA_ADD_FLAT_MODIFIER. A modifier only reaches a spell whose
# SpellFamilyName matches the modifier's (field 208) and whose SpellFamilyFlags (209-211) overlap its class mask.
# The class masks are three flag96, one per effect: effect 0 is fields 122-124 (122 = its first flag word).
$SPELLMOD_RADIUS = 6; $SPELLMOD_COST = 14
$F_ProcFlags = 34; $F_ProcChance = 35; $F_EffectClassMask0 = 122

# The Pestiféré is its own spell family. No stock spell uses 16, so its talents reach its abilities and nothing
# else, and no other class's talent or script can reach them. ChrClasses gives class 12 the same family
# (spellFamily in localTools/customClasses/classes.json): the client applies the modifiers to tooltips and costs.
$FAM_PESTIFERE = 16
$PF_FRAPPE_PUTRIDE = 0x1; $PF_CONTAGION = 0x2; $PF_DETONATION = 0x4; $PF_ODEUR = 0x8; $PF_CRACHAT = 0x10
$PF_POURRITURE = 0x20; $PF_INOCULATION = 0x40; $PF_SELF_PLAGUE = 0x80; $PF_ENEMY_PLAGUE = 0x100
$PF_SEPULCRE_PLAGUE = 0x200; $PF_CHARNIER = 0x400; $PF_PURGE = 0x800; $PF_SEPULCRE = 0x1000
$PF_MORSURE = 0x2000; $PF_RIPOSTE = 0x4000; $PF_FLAQUE = 0x8000; $PF_CARAPACE_SUINTANTE = 0x10000; $PF_BOND = 0x20000
$PF_PUANTEUR = 0x40000; $PF_PANDEMIE = 0x80000; $PF_FIEVRE = 0x100000; $PF_VOMISSURE = 0x200000; $PF_AVATAR = 0x400000
# The healer tree "Sangsue" (ids 90300-90399)
$PF_SANGSUE = 0x800000; $PF_SAIGNEE = 0x1000000; $PF_ABSORPTION = 0x2000000; $PF_DON_DE_SANG = 0x4000000
$PF_SYMBIOTE = 0x8000000; $PF_PESTILENCE_SALVATRICE = 0x10000000; $PF_HEALER_AURA = 0x20000000
# No talent modifies Brume pestilentielle: it shares the healer auras' flag (0x80000000 would be negative here)
$PF_SPORES = 0x40000000; $PF_BRUME = $PF_HEALER_AURA
$pestifereFamilyFlags = @{
    90200 = $PF_FRAPPE_PUTRIDE; 90217 = $PF_FRAPPE_PUTRIDE
    90201 = $PF_CONTAGION; 90202 = $PF_DETONATION; 90206 = $PF_DETONATION
    90203 = $PF_ODEUR; 90204 = $PF_CRACHAT; 90205 = $PF_POURRITURE
    90210 = $PF_INOCULATION; 90212 = $PF_INOCULATION; 90214 = $PF_INOCULATION
    90211 = $PF_SELF_PLAGUE; 90213 = $PF_SELF_PLAGUE; 90215 = $PF_SELF_PLAGUE
    90220 = $PF_ENEMY_PLAGUE; 90221 = $PF_ENEMY_PLAGUE; 90222 = $PF_ENEMY_PLAGUE
    90207 = $PF_SEPULCRE_PLAGUE; 90208 = $PF_SEPULCRE_PLAGUE
    90256 = $PF_CHARNIER; 90265 = $PF_PURGE; 90268 = $PF_SEPULCRE
    90223 = $PF_FLAQUE; 90224 = $PF_MORSURE; 90225 = $PF_RIPOSTE; 90226 = $PF_CARAPACE_SUINTANTE; 90227 = $PF_BOND
    90228 = $PF_PUANTEUR; 90229 = $PF_PANDEMIE; 90216 = $PF_FIEVRE; 90284 = $PF_VOMISSURE; 90287 = $PF_AVATAR
    90302 = $PF_SANGSUE; 90303 = $PF_SAIGNEE; 90304 = $PF_ABSORPTION; 90305 = $PF_DON_DE_SANG; 90306 = $PF_SYMBIOTE
    90308 = $PF_PESTILENCE_SALVATRICE
    90301 = $PF_HEALER_AURA; 90307 = $PF_HEALER_AURA; 90309 = $PF_HEALER_AURA; 90310 = $PF_HEALER_AURA; 90311 = $PF_HEALER_AURA
    90312 = $PF_SPORES; 90313 = $PF_HEALER_AURA; 90314 = $PF_HEALER_AURA; 90315 = $PF_BRUME; 90316 = $PF_BRUME
    90317 = $PF_HEALER_AURA; 90318 = $PF_HEALER_AURA
}
# More aura and modifier ids used by the Pestiféré rows
$A_ModThreat = 10; $A_ModTaunt = 11; $A_SchoolAbsorb = 69; $A_ModDamagePercentDone = 79; $A_ModScale = 61
$A_PeriodicDamage = 3; $A_AddPctModifier = 108
$A_ModIncreaseHealthPercent = 133; $A_ModTotalStatPercentage = 137
$SPELLMOD_DAMAGE = 0; $SPELLMOD_DURATION = 1; $SPELLMOD_COOLDOWN = 11
# The Pestiféré's retail looks (localTools/pestifere/ascensionVisuals.json): PestifereLook gives an imported look's id
# for field 131
. (Join-Path $repoRoot 'localTools\pestifere\Looks.ps1')

$customSpells = @(
    # --- Combat rogue: abilities ---
    @{ Id = 90010; Clone = 1752; Name = 'Quick Cut'; IconPath = 'Interface\Icons\RogueMomentum_QuickCut'; Cost = 20; Cooldown = 0; Level = 4; Spellbook = $true
       Description = 'Requires Opening. Carve through the target for 175% weapon damage and generate 2 combo points. Consumes Opening and resets the cooldown of Crimson Daggerfall. From level 30, your off-hand weapon strikes too, for half the damage.'
       Fields = @{ 20 = 9 }
       # Mutilate: twin stab animation, deep wound on the target
       Visual = @{ Clone = 7913 } },
    @{ Id = 90011; Clone = 36554; Name = 'Shadow Lunge'; IconPath = 'Interface\Icons\RogueMomentum_ShadowLunge'; Cost = 0; Cooldown = 12000; Level = 8; Spellbook = $true; ShadowLungeDamage = $true
       Description = 'Step through the shadows to your target and strike for 150% weapon damage, generating a combo point. The cooldown resets when you kill an enemy. From level 35 it grants Opening, and from level 60 it restores 10 Energy when used after a reset.'
       # Shadowstep smoke with a weapon swing, shadow slash on the target
       Visual = @{ Clone = 8262; Cast = 'ShadowLungeCast'; Impact = 6642 } },
    @{ Id = 90012; Clone = 14278; Name = 'Riposte'; IconPath = 'Interface\Icons\RogueMomentum_Riposte'; Cost = 10; Cooldown = 8000; Level = 10; Spellbook = $true
       Description = 'Counterattack for 162% weapon damage and a combo point. Within 5 sec after you dodge or parry, Riposte deals double damage and grants an additional combo point. From level 45, it also increases your dodge chance by 15% for 7 sec.'
       # Devastate's glowing blades, then Mortal Strike's heavy wound
       Visual = @{ Clone = 12295; Cast = 11383; Impact = 437; TargetImpact = 0 } },
    @{ Id = 90100; Clone = 51723; Name = 'Crescent Slash'; Icon = 'CombatRogue_CrescentSlash'; Cost = 20; Cooldown = 0; Level = 14; Spellbook = $true; NoEquipment = $true
       Description = 'Slash all enemies within 8 yards for 110% weapon damage (125% from level 42) and generate a combo point. Restores 2 Energy for each enemy hit, up to 8. From level 62, hitting 3 or more enemies generates 2 combo points.'
       Fields = @{ 80 = 109; 92 = 18 }
       # Cleave: ground crescent under the rogue, slash on every enemy hit
       Visual = @{ Clone = 219; Impact = 'CrescentSlashImpact' } },
    @{ Id = 90017; Clone = 51723; Name = 'Crimson Sweep'; IconPath = 'Interface\Icons\RogueMomentum_CrimsonSweep'; Cost = 30; Cooldown = 6000; Level = 18; Spellbook = $true; NoEquipment = $true
       Description = 'Sweep through all enemies within 8 yards for 60% weapon damage, apply Crimson Wounds for 6 sec and generate a combo point. Consumes Opening to make the bleed last 10 sec and generate 2 combo points. Crimson Wounds bleeds for 20% weapon damage every 2 sec, and each tick restores 2 Energy, up to 8 Energy every 2 sec. From level 40 it stacks up to 3 times, and from level 65 the cooldown is 4 sec.'
       Fields = @{ 80 = 59; 92 = 18 }
       # Blood-red glowing blades, blood burst on every enemy hit
       Visual = @{ Clone = 11117; Cast = 10971; Impact = 'CrimsonSweepImpact' } },

    # --- Combat rogue: finishers ---
    @{ Id = 90013; Clone = 5171; Name = 'Sanguine Veil'; IconPath = 'Interface\Icons\RogueMomentum_SanguineVeil'; Cost = 20; Cooldown = 0; Level = 16; Spellbook = $true
       Description = 'Finishing move that shrouds you in a sanguine veil, healing you for 15% of all damage you deal (20% from level 60). Lasts 6 sec per combo point. Consumes up to 30 extra Energy to last up to 50% longer.'
       AuraDescription = 'Healing for a share of damage dealt.'
       Effects = @(@{ Index = 0; Aura = $A_Dummy }, @{ Index = 1; Aura = $A_ModDamagePercentTaken; BasePoints = 0; Misc = 127 })
       Fields = @{ 40 = 21; 209 = 0; 210 = 0; 211 = 0 }
       # Slice and Dice flourish, blood tap burst, red glowing hands while the veil lasts
       Visual = @{ Clone = 254; Cast = 416; Impact = 10285; State = 108 } },
    @{ Id = 90101; Clone = 51723; Name = 'Blood Waltz'; Icon = 'CombatRogue_BloodWaltz'; FallbackIconSpell = 46924; Cost = 30; Cooldown = 0; Level = 26; Spellbook = $true; NoEquipment = $true
       Description = 'Finishing move that spins through all enemies within 8 yards, dealing 45% weapon damage per combo point to each, refreshing your Crimson Wounds on them and extending Battle Tempo by 1 sec per enemy hit. Consumes up to 30 extra Energy to deal up to 50% more damage. From level 50, grants Opening at 5 combo points, and from level 75 reaches 2 yards further.'
       Fields = @{ 5 = 0x00100010; 80 = 44; 92 = 18 }
       # Whirlwind spin, blood strike slash on every enemy hit
       Visual = @{ Clone = 223; Cast = 369; Impact = 'BloodWaltzImpact' } },
    @{ Id = 90105; Clone = 51723; Name = 'Crimson Daggerfall'; Icon = 'CombatRogue_CrimsonDaggerfall'; FallbackIconSpell = 51723; Cost = 30; Cooldown = 15000; Level = 20; Spellbook = $true; NoEquipment = $true
       Description = 'Finishing move that launches a storm of daggers at all enemies within 8 yards, dealing 60% weapon damage per combo point. Deals 30% more damage to enemies suffering from one of your damage-over-time effects. Consumes up to 30 extra Energy to deal up to 50% more damage. Its cooldown resets when it kills an enemy and when Quick Cut consumes Opening. Your other finishing moves reduce its cooldown by 1 sec per combo point, and at 5 combo points have a 20% chance to rain a free Crimson Daggerfall. From level 45 it deals 70% weapon damage per combo point, and from level 70 50% more damage to enemies suffering from your damage-over-time effects.'
       Fields = @{ 5 = 0x00100010; 80 = 59; 92 = 18 }
       # A physical dagger missile reaches every affected enemy; its custom impact kit owns the randomized sound.
       Visual = @{ Clone = 14261; Impact = 'DaggerfallImpact' } },

    # --- Combat rogue: buffs, bleed and hidden support spells ---
    @{ Id = 90014; Clone = 2983; Name = 'Opening'; IconPath = 'Interface\Icons\RogueMomentum_Opening'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Quick Cut is available. Stacks up to 2 times from level 55.'; AuraDescription = 'Quick Cut is available.' },
    @{ Id = 90015; Clone = 2983; Name = 'Battle Tempo'; IconPath = 'Interface\Icons\RogueMomentum_BattleTempo'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Eviscerate grants 3% attack speed per combo point for 10 sec.'; AuraDescription = 'Attack speed increased.'
       Effects = @(@{ Index = 0; Aura = $A_ModMeleeHaste; Value = 3 }) },
    @{ Id = 90016; Clone = 2983; Name = 'Killing Momentum'; IconPath = 'Interface\Icons\RogueMomentum_KillingMomentum'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Kills grant 5% movement speed and Energy regeneration per stack for 6 sec, stacking up to 5 times. At 5 stacks Shadow Lunge has no cooldown.'
       AuraDescription = 'Movement speed and Energy regeneration increased.'
       Effects = @(@{ Index = 0; Aura = $A_ModIncreaseSpeed; Value = 5 }, @{ Index = 1; Aura = $A_ModPowerRegenPercent; Value = 5; Misc = 3 }) },
    @{ Id = 90018; Clone = 1943; Name = 'Crimson Wounds'; IconPath = 'Interface\Icons\RogueMomentum_CrimsonSweep'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; BleedAura = $true
       Description = 'Bleeding.'; AuraDescription = 'Bleeding every 2 sec.'
       Fields = @{ 49 = 3 }
       # Rupture's blood burst on the target only: its cast animation would replay on the rogue for every enemy hit
       Visual = @{ Clone = 250; Cast = 0 } },
    @{ Id = 90102; Clone = 1752; Name = 'Blade Echo'; Icon = 'CombatRogue_TempoEcho'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'An echo of a previous strike.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = @{ 68 = -1; 69 = 0; 70 = 0; 205 = 0; 206 = 0 }
       # Killing Spree phantom strike: no caster animation, so delayed echoes never look like a stutter
       Visual = @{ Clone = 11824 } },
    @{ Id = 90103; Clone = 2983; Name = 'Crimson Frenzy'; Icon = 'CombatRogue_CrimsonFrenzy'; FallbackIconSpell = 51682; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Crimson Wounds ticks increase your attack speed.'; AuraDescription = 'Attack speed increased.'
       Effects = @(@{ Index = 0; Aura = $A_ModMeleeHaste; Value = 1 }) },
    @{ Id = 90104; Clone = 2983; Name = 'Crimson Duelist'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 1; DummyAura = $true; Spellbook = $false
       Description = 'Combat rogue passive.'
       Fields = @{ 4 = 0x1C0; 34 = 56; 35 = 100; 40 = 21 } },

    # --- Other classes and systems ---
    # Gear bonus Leech: never cast, it names the heal in the combat log, floating text and meters (Details)
    @{ Id = 90022; Clone = 2983; Name = 'Vol de vie'; FallbackIconSpell = 689; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Heals you for a share of the damage you deal.' },
    @{ Id = 90019; Clone = 8690; Name = 'Quick Travel'; IconPath = 'Interface\Icons\INV_Misc_Rune_01'; Description = 'Teleport to the selected flight master after a 3 sec cast.'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TravelCast = $true },
    @{ Id = 90021; Clone = 2565; Name = 'Gladiator Stance'; IconPath = 'Interface\Icons\Ability_Warrior_GladiatorStance'; Description = 'Fight as a gladiator while in Defensive Stance with a shield equipped. Doubles all damage you deal and all damage you take before other reductions. Cast again to leave.'; AuraDescription = 'Damage dealt and damage taken increased by 100%.'; Cost = 0; Cooldown = 0; Level = 10; Spellbook = $true; GladiatorStance = $true; SkillLine = 257; ClassMask = 1 },

    # --- Pestiféré (class 12): see .agents/plans/pestifere/pestifere.DESIGN.md ---
    # Skill line 900 is the class's own spellbook tab, class mask 2048 is class 12.
    # Behaviour lives in modules/mod-pestifere; the rows below are the data those scripts hang on.
    # Rage costs are stored tenfold (200 = 20 rage).
    @{ Id = 90200; Clone = 12294; Name = 'Frappe putride'; Icon = 'Pestifere_FrappePutride'; FallbackIconSpell = 45462; Cost = 0; Cooldown = 3000; Level = 1; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Frappe la cible pour 110% des dégâts de votre arme, applique deux charges de Pourriture et vous rend 10 points de rage. La Pourriture inflige toutes les 3 sec 1% des points de vie maximum de la cible par charge, jusqu''à 6 charges.'
       # Effect 2: apply a stack of Pourriture. Effect 3: the rage it pays back.
       Fields = @{ 131 = (PestifereLook 'FrappePutride'); 71 = 121; 74 = 0; 80 = 5; 86 = 6; 95 = 0;
                   72 = 64; 75 = 0; 87 = 6; 117 = 90205;
                   73 = 30; 76 = 0; 82 = 100; 88 = 1; 112 = 1 } },
    @{ Id = 90205; Clone = 55078; Name = 'Pourriture'; Icon = 'Pestifere_Pourriture'; FallbackIconSpell = 55078; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La chair de la cible se putréfie.'
       AuraDescription = 'Subit toutes les 3 sec des dégâts de Nature égaux à 1% de ses points de vie maximum par charge. Enfle à chaque charge.'
       # 20 sec, up to 6 stacks, and the target visibly swells: 2% model scale per stack
       Fields = @{ 2 = 0; 131 = (PestifereLook 'Pourriture'); 40 = 18; 49 = 6; 74 = 0; 72 = 6; 75 = 0; 81 = 2; 87 = 6; 96 = 61 } },

    @{ Id = 90201; Clone = 50842; Name = 'Contagion'; Icon = 'Pestifere_Contagion'; FallbackIconSpell = 50842; Cost = 200; Cooldown = 6000; Level = 6; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Transmet chaque fléau que vous portez aux ennemis dans un rayon de 10 mètres et prolonge la Pourriture qu''ils portent déjà. Génère de la menace pour chaque fléau transmis.'
       Fields = @{ 2 = 0; 131 = (PestifereLook 'Contagion') } },
    @{ Id = 90202; Clone = 6343; Name = 'Détonation'; Icon = 'Pestifere_Detonation'; FallbackIconSpell = 49158; Cost = 250; Cooldown = 0; Level = 10; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Fait exploser la moitié de la Pourriture des ennemis proches et consomme les fléaux qu''ils portent : plus un ennemi porte de charges et de fléaux, plus l''explosion est violente. Au-delà de 4 ennemis pourrissants, les dégâts de zone sont répartis. Vous rend 2% de vos points de vie maximum pour chaque ennemi touché ; ce que vous ne pouvez pas soigner devient une Excroissance qui absorbe les dégâts.'
       Fields = @{ 131 = (PestifereLook 'Detonation'); 72 = 0; 75 = 0; 81 = 0; 87 = 0; 96 = 0; 74 = 0; 80 = 1 } },
    @{ Id = 90203; Clone = 355; Name = 'Odeur de charogne'; Icon = 'Pestifere_OdeurCharogne'; FallbackIconSpell = 355; Cost = 0; Cooldown = 8000; Level = 14; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Force la cible à vous attaquer pendant 3 sec.'
       Fields = @{ 131 = (PestifereLook 'OdeurCharogne') } },
    @{ Id = 90204; Clone = 47476; Name = 'Crachat bilieux'; Icon = 'Pestifere_CrachatBilieux'; FallbackIconSpell = 47476; Cost = 100; Cooldown = 20000; Level = 20; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Crache de la bile sur un ennemi situé à 30 mètres au plus et le réduit au silence pendant 5 sec.'
       # The tank's interrupt: off the global cooldown (Strangulate, its clone, is on it)
       Fields = @{ 2 = 0; 131 = (PestifereLook 'CrachatBilieux'); 205 = 0; 206 = 0 } },
    # Détonation en chaîne: the blast jumping to an enemy outside it. mod-pestifere computes the damage (the same
    # formula as Détonation, on that enemy's own rot) and casts this with it; no damage class, so it cannot miss.
    @{ Id = 90206; Clone = 6343; Name = 'Détonation'; Icon = 'Pestifere_Detonation'; FallbackIconSpell = 49158; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La détonation se propage.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = @{ 131 = (PestifereLook 'DetonationChain'); 46 = 13; 213 = 0 } },

    # The three plagues: cast on yourself, then carried. Their strength scales with how many you carry,
    # which mod-pestifere recalculates; the values here are the single-plague baseline.
    @{ Id = 90210; Clone = 12975; Name = 'Inoculation : Carapace nécrosée'; Icon = 'Pestifere_CarapaceNecrosee'; FallbackIconSpell = 49222; Cost = 150; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Vous vous inoculez la Carapace nécrosée : vous subissez 4% de dégâts en moins et votre armure augmente de 10%, plus 3% de réduction et 10% d''armure pour chaque autre fléau que vous portez. Vous générez 150% de menace en plus, mais vous vous déplacez 10% plus lentement.'
       Fields = @{ 131 = (PestifereLook 'InoculationCarapace'); 29 = 0; 71 = 64; 86 = 1; 116 = 90211 } },
    @{ Id = 90211; Clone = 2983; CantCancel = $true; Name = 'Carapace nécrosée'; Icon = 'Pestifere_CarapaceNecrosee'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       AuraDescription = 'Dégâts subis réduits et armure augmentée. Menace générée augmentée de 150%. Vous vous déplacez plus lentement.'
       # Infinite while carried, removed when you leave combat
       Fields = @{ 40 = 21; 95 = 87; 74 = 0; 80 = -8; 110 = 127; 72 = 6; 75 = 0; 81 = -10; 87 = 1; 96 = 33;
                   73 = 6; 76 = 0; 82 = 10; 88 = 1; 97 = 101; 112 = 1 } },
    @{ Id = 90212; Clone = 12975; Name = 'Inoculation : Chair putride'; Icon = 'Pestifere_ChairPutride'; FallbackIconSpell = 50536; Cost = 150; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       # Taught by its talent, whose rank spell it is: the talent frame shows this tooltip
       Description = "Vous vous inoculez la Chair putride : vous récupérez 0,6% de vos points de vie maximum toutes les 3 sec, plus 0,4% pour chaque autre fléau que vous portez, mais les soins que les autres vous prodiguent sont réduits de 20%."
       Fields = @{ 131 = (PestifereLook 'InoculationChair'); 29 = 0; 71 = 64; 86 = 1; 116 = 90213 } },
    @{ Id = 90213; Clone = 2983; CantCancel = $true; Name = 'Chair putride'; Icon = 'Pestifere_ChairPutride'; FallbackIconSpell = 50536; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       AuraDescription = 'Vous récupérez des points de vie toutes les 3 sec. Les soins que les autres vous prodiguent sont réduits de 20%.'
       Fields = @{ 40 = 21; 95 = 8; 74 = 0; 80 = 20; 98 = 3000 } },
    @{ Id = 90214; Clone = 12975; Name = 'Inoculation : Peste virulente'; Icon = 'Pestifere_PesteVirulente'; FallbackIconSpell = 69674; Cost = 150; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       # Taught by its talent, whose rank spell it is: the talent frame shows this tooltip
       Description = "Vous vous inoculez la Peste virulente : vos dégâts augmentent de 6%, plus 4% pour chaque autre fléau que vous portez, mais elle vous inflige 0,5% de vos points de vie maximum toutes les 3 sec. Si c'est le seul fléau que vous portez, elle vous ronge de plus en plus fort."
       Fields = @{ 131 = (PestifereLook 'InoculationPeste'); 29 = 0; 71 = 64; 86 = 1; 116 = 90215 } },
    @{ Id = 90215; Clone = 2983; CantCancel = $true; Name = 'Peste virulente'; Icon = 'Pestifere_PesteVirulente'; FallbackIconSpell = 69674; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       AuraDescription = 'Dégâts infligés augmentés. Vous subissez des dégâts toutes les 3 sec, de plus en plus forts si la Peste virulente est le seul fléau que vous portez.'
       Fields = @{ 40 = 21; 95 = 79; 74 = 0; 80 = 10; 110 = 127; 72 = 6; 75 = 0; 81 = 15; 87 = 1; 96 = 3;
                   99 = 3000 } },

    # The enemy versions, handed out by Contagion and consumed by Détonation. Like every debuff of the class (field 2,
    # dispel type 0), nothing can dispel them: their clone's disease type let other players' cures take them off.
    @{ Id = 90220; Clone = 55078; Name = 'Carapace nécrosée'; Icon = 'Pestifere_CarapaceNecrosee'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       AuraDescription = 'Dégâts infligés réduits de 5%.'
       Fields = @{ 2 = 0; 40 = 18; 95 = 79; 74 = 0; 80 = -5; 110 = 127 } },
    @{ Id = 90221; Clone = 55078; Name = 'Chair putride'; Icon = 'Pestifere_ChairPutride'; FallbackIconSpell = 50536; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       AuraDescription = 'Soins reçus réduits de 20%.'
       Fields = @{ 2 = 0; 40 = 18 } },
    @{ Id = 90222; Clone = 55078; Name = 'Peste virulente'; Icon = 'Pestifere_PesteVirulente'; FallbackIconSpell = 69674; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       AuraDescription = 'Subit toutes les 3 sec des dégâts de Nature égaux à 1% de ses points de vie maximum.'
       Fields = @{ 2 = 0; 40 = 18 } },

    # Sépulcre's stored plague: the damage it held back, dealt over 12 sec. mod-pestifere deals every tick itself
    # (the amount is exact: it was already mitigated once), and Contagion hands the enemy version out. Physical,
    # which no resistance can partly resist, and not dispellable: grave dirt, not a disease.
    @{ Id = 90207; Clone = 55078; CantCancel = $true; Name = 'Sépulcre'; Icon = 'Pestifere_Sepulcre'; FallbackIconSpell = 43265; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Les dégâts retenus par le Sépulcre.'
       AuraDescription = 'Les dégâts retenus par le Sépulcre vous sont infligés toutes les 3 sec.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 3; BasePoints = 0 })
       Fields = @{ 2 = 0; 40 = 29; 46 = 1; 49 = 1; 98 = 3000; 213 = 0; 225 = 1 } },
    @{ Id = 90208; Clone = 55078; Name = 'Sépulcre'; Icon = 'Pestifere_Sepulcre'; FallbackIconSpell = 43265; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Les dégâts retenus par un Sépulcre.'
       AuraDescription = 'Subit toutes les 3 sec les dégâts retenus par un Sépulcre.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 6; Aura = 3; BasePoints = 0 })
       Fields = @{ 2 = 0; 40 = 29; 46 = 13; 49 = 1; 98 = 3000; 213 = 0; 225 = 1 } },

    # Active spells taught by talents: each is its talent's rank spell, so the talent frame shows this tooltip and
    # learning the talent puts the spell in the class tab (like Chair putride and Peste virulente above)
    @{ Id = 90256; Clone = 6343; Name = 'Charnier ambulant'; Icon = 'PestifereTalent_CharnierAmbulant'; FallbackIconSpell = 69195; Cost = 0; Cooldown = 60000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Libère une onde qui transmet chaque fléau que vous portez à tous les ennemis dans un rayon de 10 mètres et monte leur Pourriture à 6 charges. Génère une menace importante.'
       # Caster-centred enemy area (Thunder Clap layout), a dummy effect per enemy hit, no damage class: it cannot miss
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 22 })
       Fields = @{ 131 = (PestifereLook 'CharnierAmbulant'); 89 = 15; 92 = 13; 213 = 0 } },
    @{ Id = 90265; Clone = 12975; Name = 'Purge cathartique'; Icon = 'PestifereTalent_PurgeCathartique'; FallbackIconSpell = 48743; Cost = 0; Cooldown = 45000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = "Consomme les fléaux que vous portez et vous rend 12% de vos points de vie maximum pour chacun d'eux. Vous perdez leurs effets jusqu'à ce que vous vous les inoculiez de nouveau."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       # Death Pact's heal burst
       Fields = @{ 131 = (PestifereLook 'PurgeCathartique') } },
    @{ Id = 90268; Clone = 12975; Name = 'Sépulcre'; Icon = 'PestifereTalent_Sepulcre'; FallbackIconSpell = 43265; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Pendant 8 sec, la moitié des dégâts que vous subissez est différée : le Sépulcre la retient, puis vous l''inflige sur les 12 sec suivantes. Ce fléau se transmet avec Contagion et explose avec Détonation, comme les autres.'
       AuraDescription = 'La moitié des dégâts subis est retenue par le Sépulcre.'
       # An all-school absorb whose amount mod-pestifere makes unlimited: it takes half of every hit, 8 sec
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 69; BasePoints = 0; Misc = 127 })
       Fields = @{ 131 = (PestifereLook 'Sepulcre'); 40 = 31 } },

    # --- Pestiféré: the kit from level 24 to 70 ---
    # Flaque de bile: a pool at your feet (Consecration's layout: a dynamic object on the caster, applying its aura to
    # every enemy inside). Effect 0 ticks damage and sows Pourriture, effect 1 is Bile corrosive's damage reduction
    # (0 without the talent); mod-pestifere computes both.
    @{ Id = 90223; Clone = 26573; Name = 'Flaque de bile'; Icon = 'Pestifere_FlaqueDeBile'; FallbackIconSpell = 43265; Cost = 150; Cooldown = 15000; Level = 24; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Déverse une flaque de bile à vos pieds pendant 10 sec. Les ennemis qui s''y trouvent subissent des dégâts de Nature toutes les 2 sec et reçoivent une charge de Pourriture à chaque fois.'
       AuraDescription = 'Subit des dégâts de Nature toutes les 2 sec et pourrit.'
       Effects = @(@{ Index = 0; Effect = 27; TargetA = 18; Aura = $A_PeriodicDamage; BasePoints = 0 },
                   @{ Index = 1; Effect = 27; TargetA = 18; Aura = $A_ModDamagePercentDone; BasePoints = 0; Misc = 127 })
       Fields = @{ 131 = (PestifereLook 'FlaqueDeBile'); 89 = 16; 90 = 16; 92 = 14; 93 = 14; 98 = 2000; 40 = 1; 213 = 0; 225 = 8 } },
    # Morsure fétide: the single-target threat strike, stronger for every Pourriture on the target (spell_threat adds
    # its bonus threat)
    @{ Id = 90224; Clone = 12294; Name = 'Morsure fétide'; Icon = 'Pestifere_MorsureFetide'; FallbackIconSpell = 55090; Cost = 140; Cooldown = 4500; Level = 30; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Mord la cible pour 230% des dégâts de votre arme, augmentés de 18% par charge de Pourriture qu''elle porte, sans les consommer. Génère une menace importante.'
       Effects = @(@{ Index = 0; Effect = 31; TargetA = 6; Value = 230 })
       Fields = @{ 131 = (PestifereLook 'MorsureFetide') } },
    # Riposte purulente: usable for a few seconds after you dodge, parry or block (Revenge's aura state)
    @{ Id = 90225; Clone = 57823; Name = 'Riposte purulente'; Icon = 'Pestifere_RipostePurulente'; FallbackIconSpell = 57823; Cost = 0; Cooldown = 3000; Level = 36; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Utilisable après avoir esquivé, paré ou bloqué une attaque. Frappe la cible pour 180% des dégâts de votre arme et applique une charge de Pourriture à la cible et à 2 ennemis proches.'
       Effects = @(@{ Index = 0; Effect = 31; TargetA = 6; Value = 180 })
       Fields = @{ 131 = (PestifereLook 'RipostePurulente'); 20 = 1 } },
    # Carapace suintante: the short defensive, an absorb that grows with Virulence
    @{ Id = 90226; Clone = 48707; Name = 'Carapace suintante'; Icon = 'Pestifere_CarapaceSuintante'; FallbackIconSpell = 48707; Cost = 100; Cooldown = 30000; Level = 44; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Une carapace de pus vous entoure pendant 10 sec et absorbe 4% de vos points de vie maximum, plus 3% par fléau que vous portez.'
       AuraDescription = 'Absorbe les dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_SchoolAbsorb; BasePoints = 0; Misc = 127 })
       Fields = @{ 131 = (PestifereLook 'CarapaceSuintante'); 40 = 1 } },
    # Bond putride: a leap onto an enemy (Intercept's charge, its stun replaced by rage)
    @{ Id = 90227; Clone = 20252; Name = 'Bond putride'; Icon = 'Pestifere_BondPutride'; FallbackIconSpell = 20252; Cost = 0; Cooldown = 20000; Level = 50; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Bondit sur un ennemi situé entre 8 et 25 mètres, lui applique une charge de Pourriture et vous rend 15 points de rage.'
       Effects = @(@{ Index = 0; Effect = 96; TargetA = 6 },
                   @{ Index = 1; Effect = 30; TargetA = 1; BasePoints = 150; Misc = 1 })
       Fields = @{ 131 = (PestifereLook 'BondPutride') } },
    # Puanteur insoutenable: AoE taunt (Challenging Shout)
    @{ Id = 90228; Clone = 1161; Name = 'Puanteur insoutenable'; Icon = 'Pestifere_Puanteur'; FallbackIconSpell = 1161; Cost = 0; Cooldown = 180000; Level = 60; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Une puanteur insoutenable force tous les ennemis dans un rayon de 10 mètres à vous attaquer pendant 6 sec.'
       AuraDescription = 'Forcé d''attaquer le Pestiféré.'
       Fields = @{ 131 = (PestifereLook 'Puanteur') } },
    # Pandémie: the big AoE cooldown. Effect 0 removes Contagion's cooldown (a -100% cooldown modifier), effect 1 is
    # the marker Frappe putride reads to strike 3 more enemies.
    @{ Id = 90229; Clone = 12975; Name = 'Pandémie'; Icon = 'Pestifere_Pandemie'; FallbackIconSpell = 50536; Cost = 0; Cooldown = 120000; Level = 70; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Pendant 15 sec, Contagion n''a plus de temps de recharge et Frappe putride touche aussi 3 ennemis proches.'
       AuraDescription = 'Contagion sans temps de recharge. Frappe putride touche 3 ennemis de plus.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_AddPctModifier; Value = -100; Misc = $SPELLMOD_COOLDOWN },
                   @{ Index = 1; Effect = 6; TargetA = 1; Aura = $A_Dummy })
       Fields = @{ 131 = (PestifereLook 'Pandemie'); 40 = 8; 122 = $PF_CONTAGION } },

    # Excroissance: never cast by the player. PestifereScripts.cpp stores overhealing and 20% of self-healing
    # in it and sets the amount itself, so the base points here are only a placeholder.
    @{ Id = 90230; Clone = 48707; Name = 'Excroissance'; Icon = 'PestifereTalent_CrouteNecrosee'; FallbackIconSpell = 48707; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Les soins excédentaires et 20% des soins que vous vous prodiguez s''accumulent sous votre peau et absorbent les dégâts, jusqu''à 75% de vos points de vie maximum.'
       AuraDescription = 'Absorbe les dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_SchoolAbsorb; BasePoints = 0; Misc = 127 })
       # 20 sec: long enough to be worth banking, short enough that it is never a second health bar
       Fields = @{ 131 = (PestifereLook 'Excroissance'); 40 = 18 } },

    # Support spells: never in the spellbook
    # Inébranlable: the Mythic+ tank's footing. Only the icon and the text live here - MythicDungeonSystem.cpp
    # applies the mechanic immunities itself, because the aura that carries a whole mask of them resolves that
    # mask from a hardcoded list of spell ids in the core and a custom spell cannot join it.
    @{ Id = 90600; Clone = 12975; Name = 'Inébranlable'; FallbackIconSpell = 42292; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le tank d''un donjon Mythique+ ne peut pas être privé du contrôle de lui-même.'
       AuraDescription = 'Immunisé aux étourdissements, peurs, charmes et autres pertes de contrôle.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_Dummy })
       # No duration: it lasts as long as the tank is in the dungeon, and the module takes it away on the way out
       Fields = @{ 40 = 21 } },
    # The paragon board's procs (ParagonSystem.cpp), 90650-90699 (90664-90665 are MythicDungeonSystem.cpp's). The damage and heal ones are never cast: they name
    # what the module deals or heals, so it shows in the combat log, floating text and meters such as Details. The
    # buffs are markers of what is running, applied with the proc's own duration; the effect itself is the module's.
    @{ Id = 90650; Clone = 2983; Name = 'Explosion de parangon'; Icon = 'ParagonNode_ExplodingCarcass'; FallbackIconSpell = 11113; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Le cadavre explose et blesse les ennemis proches.' },
    @{ Id = 90651; Clone = 2983; Name = 'Représailles de parangon'; Icon = 'ParagonNode_Riposte'; FallbackIconSpell = 6572; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Renvoie une partie des dégâts subis à l''attaquant.' },
    @{ Id = 90652; Clone = 2983; Name = 'Double frappe'; Icon = 'ParagonNode_DoubleStrike'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Le coup frappe une seconde fois.' },
    @{ Id = 90653; Clone = 2983; Name = 'Coup de grâce'; Icon = 'ParagonNode_Executioner'; FallbackIconSpell = 20647; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts supplémentaires contre une cible affaiblie.' },
    @{ Id = 90654; Clone = 2983; Name = 'Vol de vie de parangon'; Icon = 'ParagonNode_Bloodthirst'; FallbackIconSpell = 689; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Rend une partie des dégâts infligés sous forme de points de vie.' },
    @{ Id = 90655; Clone = 2983; CantCancel = $true; Name = 'Furie du parangon'; Icon = 'ParagonNode_Fury'; FallbackIconSpell = 12292; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts infligés augmentés.'; AuraDescription = 'Dégâts infligés augmentés.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90656; Clone = 2983; CantCancel = $true; Name = 'Élan du parangon'; Icon = 'ParagonNode_Momentum'; FallbackIconSpell = 33697; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Puissance d''attaque et des sorts augmentées.'; AuraDescription = 'Puissance d''attaque et des sorts augmentées.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90657; Clone = 2983; CantCancel = $true; Name = 'Écaille de pierre'; Icon = 'ParagonNode_Stonescale'; FallbackIconSpell = 71; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Armure augmentée.'; AuraDescription = 'Armure augmentée.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90658; Clone = 2983; CantCancel = $true; Name = 'Rempart du parangon'; Icon = 'ParagonNode_Bulwark'; FallbackIconSpell = 871; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits.'; AuraDescription = 'Dégâts subis réduits.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90659; Clone = 2983; CantCancel = $true; Name = 'Immortel'; Icon = 'ParagonNode_ApotheosisImmortal'; FallbackIconSpell = 642; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'La mort vous refuse : aucun dégât ne vous atteint.'; AuraDescription = 'La mort vous refuse : aucun dégât ne vous atteint.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90660; Clone = 2983; CantCancel = $true; Name = 'Immortel épuisé'; Icon = 'ParagonNode_ApotheosisImmortal'; FallbackIconSpell = 25771; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Immortel ne peut pas se déclencher à nouveau pour l''instant.'; AuraDescription = 'Immortel ne peut pas se déclencher à nouveau pour l''instant.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90661; Clone = 2983; CantCancel = $true; Name = 'Carnage'; Icon = 'ParagonNode_Carnage'; FallbackIconSpell = 48721; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Dégâts infligés augmentés à chaque victime.'; AuraDescription = 'Dégâts infligés augmentés à chaque victime.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90662; Clone = 2983; Name = 'Cataclysme'; Icon = 'ParagonNode_ApotheosisCataclysm'; FallbackIconSpell = 42940; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Une partie de vos coups frappe aussi les ennemis proches de votre cible.' },
    @{ Id = 90663; Clone = 2983; CantCancel = $true; Name = 'Rancune'; Icon = 'ParagonNode_Bastion'; FallbackIconSpell = 25228; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Les coups subis vous emplissent de rancune.'; AuraDescription = 'Puissance d''attaque et des sorts augmentées par les coups subis.'
       Fields = @{ 40 = 21 } },
    # The caster side of the board: set off by spells alone (ParagonSystem.cpp, 90666-90671)
    @{ Id = 90666; Clone = 2983; Name = 'Écho du parangon'; Icon = 'ParagonNode_Echo'; FallbackIconSpell = 30451; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Le coup critique d''un sort résonne et frappe une seconde fois.' },
    @{ Id = 90667; Clone = 2983; Name = 'Arc du parangon'; Icon = 'ParagonNode_Overload'; FallbackIconSpell = 421; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Une partie des dégâts d''un sort se propage aux ennemis proches de la cible.' },
    @{ Id = 90668; Clone = 2983; CantCancel = $true; Name = 'Célérité du parangon'; Icon = 'ParagonNode_Influx'; FallbackIconSpell = 12042; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vitesse d''incantation augmentée.'; AuraDescription = 'Vitesse d''incantation augmentée.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90669; Clone = 2983; CantCancel = $true; Name = 'Égide du parangon'; Icon = 'ParagonNode_FortifiedMind'; FallbackIconSpell = 17; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Une égide absorbe les dégâts subis.'; AuraDescription = 'Absorbe les dégâts subis.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90670; Clone = 2983; CantCancel = $true; Name = 'Clairvoyance du parangon'; Icon = 'ParagonNode_Clairvoyance'; FallbackIconSpell = 12043; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Puissance des sorts augmentée.'; AuraDescription = 'Puissance des sorts augmentée.'
       Fields = @{ 40 = 21 } },
    @{ Id = 90671; Clone = 2983; Name = 'Illumination du parangon'; Icon = 'ParagonNode_Illumination'; FallbackIconSpell = 20215; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Rend une partie du mana maximum.' },
    # Imprudence (mod-stat-growth MythicTuning.cpp OnHitTaken): hit in a mythic dungeon while standing in an enemy's red
    # area - 1% of the maximum health a second per stack (SPELL_AURA_PERIODIC_DAMAGE_PERCENT), 6 s, up to 10
    @{ Id = 90672; Clone = 2983; Name = 'Imprudence'; IconPath = 'Interface\Icons\Ability_Creature_Disease_02'; FallbackIconSpell = 172; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = "Touché dans une zone dangereuse : vous perdez 1% de vos points de vie maximum par seconde et par charge."
       AuraDescription = 'Perd 1% de ses points de vie maximum par seconde et par charge. Sortez des zones rouges !'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 89; TargetA = 1; Value = 1 }); Fields = @{ 40 = 32; 98 = 1000 } },
    # Threat in dungeons and raids (mod-stat-growth MythicDungeonSystem.cpp): the tank's presence and everyone else's
    # discretion. The module sets the amount; the paragon board's tank nodes add to the presence.
    @{ Id = 90664; Clone = 2983; CantCancel = $true; Name = 'Présence du gardien'; FallbackIconSpell = 71; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le tank du groupe génère davantage de menace.'; AuraDescription = 'Menace générée augmentée.'
       Effects = @(@{ Index = 0; Aura = $A_ModThreat; Value = 100; Misc = 127 })
       Fields = @{ 40 = 21 } },
    @{ Id = 90665; Clone = 2983; CantCancel = $true; Name = 'Discrétion'; FallbackIconSpell = 1038; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Laissez le tank tenir les ennemis.'; AuraDescription = 'Menace générée réduite : le tank de votre groupe tient les ennemis.'
       Effects = @(@{ Index = 0; Aura = $A_ModThreat; Value = -40; Misc = 127 })
       Fields = @{ 40 = 21 } },
    # Carapace nécrosée's threat: the tank's presence, carried with the plague (x2.5 threat)
    @{ Id = 90209; Clone = 2983; Name = 'Carapace nécrosée'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = 'Menace générée augmentée.'
       Effects = @(@{ Index = 0; Aura = $A_ModThreat; Value = 150; Misc = 127 }) },
    # Fièvre: the talent's proc, your next Morsure fétide free and off cooldown
    @{ Id = 90216; Clone = 12975; Name = 'Fièvre'; Icon = 'PestifereTalent_Fievre'; FallbackIconSpell = 55090; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre prochaine Morsure fétide ne coûte pas de rage.'
       AuraDescription = 'Votre prochaine Morsure fétide ne coûte pas de rage.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_AddPctModifier; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 131 = (PestifereLook 'Fievre'); 40 = 1; 122 = $PF_MORSURE } },
    # Pandémie's extra strikes: Frappe putride on a nearby enemy, without its cost or cooldown
    @{ Id = 90217; Clone = 12294; Name = 'Frappe putride'; Icon = 'Pestifere_FrappePutride'; FallbackIconSpell = 45462; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe putride, portée par la Pandémie.'
       Effects = @(@{ Index = 0; Effect = 121; TargetA = 6; BasePoints = 5 },
                   @{ Index = 1; Effect = 64; TargetA = 6 })
       Fields = @{ 131 = (PestifereLook 'FrappePutride'); 117 = 90205 } },
    # Rigor mortis: the cooldown after it saved you (3 min)
    @{ Id = 90286; Clone = 2983; Name = 'Rigor mortis'; Icon = 'PestifereTalent_RigorMortis'; FallbackIconSpell = 48743; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Rigor mortis ne peut plus vous sauver pour le moment.'
       AuraDescription = 'Rigor mortis ne peut plus vous sauver.'
       Fields = @{ 40 = 25 } },

    # Talent-taught actives of the rebuilt tree (each is its talent's rank spell)
    @{ Id = 90284; Clone = 120; Name = 'Vomissure'; Icon = 'PestifereTalent_Vomissure'; FallbackIconSpell = 69195; Cost = 150; Cooldown = 9000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Vomit un flot de bile devant vous : inflige de lourds dégâts de Nature aux ennemis dans un cône de 10 mètres et leur applique 2 charges de Pourriture.'
       # Cone of Cold's cone, one damage effect, no damage class: it cannot miss
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 104; BasePoints = 0 })
       Fields = @{ 2 = 0; 131 = (PestifereLook 'Vomissure'); 92 = 13; 213 = 0; 225 = 8 } },
    @{ Id = 90287; Clone = 12975; Name = 'Avatar de la peste'; Icon = 'PestifereTalent_AvatarPeste'; FallbackIconSpell = 49206; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Vous devenez un avatar de la peste pendant 20 sec : vous grandissez, vos fléaux comptent pour un fléau de plus et Détonation ne consomme plus les charges de Pourriture.'
       AuraDescription = 'Vos fléaux comptent pour un fléau de plus. Détonation ne consomme plus la Pourriture.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_ModScale; Value = 30 },
                   @{ Index = 1; Effect = 6; TargetA = 1; Aura = $A_Dummy })
       Fields = @{ 131 = (PestifereLook 'AvatarPeste'); 40 = 18 } },
    # --- Pestiféré: the healer tree "Sangsue" (.agents/plans/pestifere/pestifere-healer.DESIGN.md) ---
    # Everything is instant and needs no friendly target: mod-pestifere (PestifereHealer.cpp) picks the ally.
    # Transfusion itself is a talent rank (below); these are the actives its talents teach and the rows that name
    # the healing in the combat log.
    # Transfusion's heal: never cast, it names the healing that melee hits trade for
    @{ Id = 90301; Clone = 2983; Name = 'Transfusion'; Icon = 'PestifereHealer_Transfusion'; FallbackIconSpell = 689; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Les dégâts de vos coups de mêlée soignent vos alliés blessés.' },
    # Sangsue: a Nature strike that attaches a leech (effect 1, a periodic aura on the enemy). mod-pestifere sets
    # each drain from attack power and heals the most injured ally with it.
    @{ Id = 90302; Clone = 12294; Name = 'Sangsue'; Icon = 'PestifereHealer_Sangsue'; FallbackIconSpell = 5138; Cost = 150; Cooldown = 8000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Frappe la cible pour 80% des dégâts de votre arme en dégâts de Nature et y attache une sangsue pendant 12 sec. Toutes les 2 sec, la sangsue draine l''ennemi et soigne l''allié le plus blessé pour 150% des dégâts drainés.'
       AuraDescription = 'Une sangsue draine la vie de la cible toutes les 2 sec.'
       Effects = @(@{ Index = 0; Effect = 31; TargetA = 6; Value = 80 },
                   @{ Index = 1; Effect = 6; TargetA = 6; Aura = $A_PeriodicDamage; BasePoints = 0 })
       Fields = @{ 131 = (PestifereLook 'Sangsue'); 40 = 29; 99 = 2000; 225 = 8 } },
    @{ Id = 90303; Clone = 12294; Name = 'Saignée'; Icon = 'PestifereHealer_Saignee'; FallbackIconSpell = 49998; Cost = 200; Cooldown = 6000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Frappe la cible pour 140% des dégâts de votre arme. Transfusion peut échanger ce coup contre des soins sur 3 alliés blessés au lieu d''un seul.'
       Effects = @(@{ Index = 0; Effect = 31; TargetA = 6; Value = 140 })
       # Death Strike's swing and sound, then a burst of blood with Mark of Blood's sound
       Fields = @{ 131 = (PestifereLook 'Saignee') } },
    # The self-cast healer actives: a dummy on the caster, mod-pestifere finds the ally (Purge cathartique's layout)
    @{ Id = 90304; Clone = 12975; Name = 'Absorption morbide'; Icon = 'PestifereHealer_AbsorptionMorbide'; FallbackIconSpell = 528; Cost = 100; Cooldown = 8000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Aspire une maladie et un poison de l''allié le plus blessé qui en porte, à 40 mètres au plus, et lui rend 5% de ses points de vie maximum pour chaque effet aspiré.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 131 = (PestifereLook 'AbsorptionMorbide'); 205 = 133; 206 = 1500 } },
    @{ Id = 90305; Clone = 12975; Name = 'Don de sang'; Icon = 'PestifereHealer_DonDeSang'; FallbackIconSpell = 48743; Cost = 0; Cooldown = 45000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Vous perdez 8% de vos points de vie maximum et l''allié le plus blessé, à 40 mètres au plus, récupère 25% de ses points de vie maximum, ou 40% s''il est sous 35% de ses points de vie. Ne déclenche pas le temps de recharge global.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 131 = (PestifereLook 'DonDeSang') } },
    @{ Id = 90306; Clone = 12975; Name = 'Symbiote'; Icon = 'PestifereHealer_Symbiote'; FallbackIconSpell = 53563; Cost = 100; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Lie un symbiote à votre cible amicale ou, à défaut, à l''allié qui subit le plus d''attaques, pendant 60 sec. Il reçoit 25% des soins de Transfusion que vous prodiguez aux autres, et 20% de tous vos soins forment sur lui un Caillot qui absorbe les dégâts, jusqu''à 15% de ses points de vie maximum. Un seul symbiote à la fois.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 131 = (PestifereLook 'Symbiote'); 205 = 133; 206 = 1500 } },
    # The symbiote on its bearer: effect 1 is Sang de l'hôte's damage reduction (0 without the talent)
    @{ Id = 90307; Clone = 2983; Name = 'Symbiote'; Icon = 'PestifereHealer_Symbiote'; FallbackIconSpell = 53563; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Un symbiote partage les soins du Pestiféré.'
       AuraDescription = 'Reçoit une part des soins de Transfusion du Pestiféré.'
       Effects = @(@{ Index = 0; Aura = $A_Dummy }, @{ Index = 1; Aura = $A_ModDamagePercentTaken; BasePoints = 0; Misc = 127 })
       Fields = @{ 131 = (PestifereLook 'SymbioteAura'); 40 = 3 } },
    # Jet de sang (talent, healer tree): the healer's reach. A spit at an enemy 30 yards off; mod-pestifere deals its
    # damage (120% weapon, Nature) and turns 150% of it into healing on the most injured ally
    @{ Id = 90296; Clone = 12975; Name = 'Jet de sang'; IconPath = 'Interface\Icons\Spell_Shadow_LifeDrain'; FallbackIconSpell = 47541; Cost = 150; Cooldown = 6000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Crache un jet de sang sur un ennemi à 30 mètres au plus : il subit 120% des dégâts de votre arme en dégâts de Nature, et l''allié le plus blessé récupère 150% de ces dégâts.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = @{ 131 = (PestifereLook 'JetDeSang'); 46 = 4; 205 = 133; 206 = 1500; 213 = 0; 225 = 8 } },
    # Poussée de sang (talent, healer tree): the burst. An instant heal on the most injured ally, then three hits that
    # heal three times as much (Hémostase). Off the global cooldown, like Don de sang
    @{ Id = 90297; Clone = 12975; Name = 'Poussée de sang'; IconPath = 'Interface\Icons\Spell_DeathKnight_BloodTap'; FallbackIconSpell = 45529; Cost = 0; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Rend aussitôt 20% de ses points de vie maximum à l''allié le plus blessé, et vos 3 prochains coups soignent trois fois plus. Ne déclenche pas le temps de recharge global.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 131 = (PestifereLook 'PousseeDeSang') } },
    # Cal putride: Carapace réactive's hardened skin, 25% more armor for 8 sec
    @{ Id = 90299; Clone = 12975; Name = 'Cal putride'; IconPath = 'Interface\Icons\Ability_Warrior_ShieldMastery'; FallbackIconSpell = 12975; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre peau durcit : armure augmentée de 25% pendant 8 sec.'
       AuraDescription = 'Armure augmentée de 25%.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_ModResistancePct; Value = 25; Misc = 1 })
       Fields = @{ 40 = 31 } },
    # Miasme suffocant (talent, class tree): enemies within 10 yards deal 10% less damage for 8 sec. Plain data: an
    # area aura on enemies around the caster, like Demoralizing Shout
    @{ Id = 90298; Clone = 1160; Name = 'Miasme suffocant'; IconPath = 'Interface\Icons\Ability_Creature_Disease_03'; FallbackIconSpell = 1160; Cost = 0; Cooldown = 45000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Un miasme suffocant s''échappe de vous : les ennemis à 10 mètres au plus infligent 10% de dégâts en moins pendant 8 sec.'
       AuraDescription = 'Dégâts infligés réduits de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 22; Aura = $A_ModDamagePercentDone; Value = -10; Misc = 127 })
       Fields = @{ 131 = (PestifereLook 'Miasme'); 40 = 31; 89 = 15; 92 = 13 } },
    @{ Id = 90308; Clone = 12975; Name = 'Pestilence salvatrice'; Icon = 'PestifereHealer_PestilenceSalvatrice'; FallbackIconSpell = 49194; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Pendant 15 sec, Transfusion soigne jusqu''à 5 alliés blessés et ses soins sont doublés. Ne déclenche pas le temps de recharge global.'
       AuraDescription = 'Transfusion soigne jusqu''à 5 alliés et ses soins sont doublés.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = $A_Dummy })
       Fields = @{ 131 = (PestifereLook 'PestilenceSalvatrice'); 40 = 8 } },
    # Coagulation on the ally just healed: effect 0 is the talent's damage reduction
    @{ Id = 90309; Clone = 2983; Name = 'Coagulation'; Icon = 'PestifereHealer_Coagulation'; FallbackIconSpell = 48982; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits.'
       AuraDescription = 'Dégâts subis réduits.'
       Effects = @(@{ Index = 0; Aura = $A_ModDamagePercentTaken; BasePoints = 0; Misc = 127 })
       Fields = @{ 40 = 32 } },
    # Réserve de sang: shown while hits are banked for the next trade
    @{ Id = 90310; Clone = 2983; Name = 'Réserve de sang'; Icon = 'PestifereHealer_ReserveDeSang'; FallbackIconSpell = 55233; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Des soins en réserve.'
       AuraDescription = 'Votre prochain échange de Transfusion soigne en plus ce que vous avez mis en réserve.'
       Fields = @{ 40 = 21 } },
    # Contagion bénigne's heal: never cast, it names the healing Contagion gives
    @{ Id = 90311; Clone = 2983; Name = 'Contagion bénigne'; Icon = 'PestifereHealer_ContagionBenigne'; FallbackIconSpell = 50842; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Contagion soigne les alliés proches.' },
    # Transfusion's procs (mod-pestifere rolls them on every melee hit while someone is injured)
    # Spores: a heal over time on an injured ally, 10 sec, a tick every 2 sec sized from attack power
    @{ Id = 90312; Clone = 774; Name = 'Spores'; Icon = 'PestifereHealer_Spores'; FallbackIconSpell = 774; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des spores bienfaisantes soignent la cible.'
       AuraDescription = 'Récupère des points de vie toutes les 2 sec.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 8; BasePoints = 0 })
       Fields = @{ 40 = 1; 98 = 2000 }
       # Blood Plague's disease glow instead of Rejuvenation's, silent
       Visual = @{ Clone = 14315 } },
    # Pustule éclatante and Essaim: never cast, they name the healing of the two burst procs
    @{ Id = 90313; Clone = 2983; Name = 'Pustule éclatante'; Icon = 'PestifereHealer_Pustule'; FallbackIconSpell = 49005; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Une pustule éclate et soigne l''allié le plus blessé et ceux qui l''entourent.' },
    @{ Id = 90314; Clone = 2983; Name = 'Essaim'; Icon = 'PestifereHealer_Essaim'; FallbackIconSpell = 33076; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Un essaim de mouches soigne un allié blessé puis rebondit sur le suivant.' },
    # Brume pestilentielle: the group heal over time, taught by its talent
    @{ Id = 90315; Clone = 12975; Name = 'Brume pestilentielle'; Icon = 'PestifereHealer_Brume'; FallbackIconSpell = 48438; Cost = 200; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = 900; ClassMask = 2048
       Description = 'Une brume bienfaisante enveloppe les membres de votre groupe à 30 mètres au plus : elle leur rend aussitôt 10% de leurs points de vie maximum, puis 12% de plus en 12 sec.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 131 = (PestifereLook 'Brume'); 205 = 133; 206 = 1500 } },
    @{ Id = 90316; Clone = 774; Name = 'Brume pestilentielle'; Icon = 'PestifereHealer_Brume'; FallbackIconSpell = 48438; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une brume bienfaisante soigne la cible.'
       AuraDescription = 'Récupère 2% de ses points de vie maximum toutes les 2 sec.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 8; BasePoints = 0 })
       Fields = @{ 40 = 29; 98 = 2000 }
       Visual = @{ Clone = 14315 } },
    # Caillot: the absorb Symbiote builds on its bearer from 20% of the healer's healing (mod-pestifere sets the amount)
    @{ Id = 90317; Clone = 2983; Name = 'Caillot'; Icon = 'PestifereHealer_Coagulation'; FallbackIconSpell = 48982; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Un caillot de sang absorbe les dégâts.'
       AuraDescription = 'Absorbe les dégâts.'
       Effects = @(@{ Index = 0; Aura = $A_SchoolAbsorb; BasePoints = 0; Misc = 127 })
       Fields = @{ 131 = (PestifereLook 'Caillot'); 40 = 3 } },
    # Hémostase: the next 3 melee hits heal three times as much (a stack per hit, mod-pestifere consumes them)
    @{ Id = 90318; Clone = 2983; Name = 'Hémostase'; Icon = 'PestifereHealer_Hemophagie'; FallbackIconSpell = 55233; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Vos prochains coups soignent trois fois plus.'
       AuraDescription = 'Vos prochains coups de mêlée soignent trois fois plus.'
       Fields = @{ 131 = (PestifereLook 'Hemostase'); 40 = 32 } }
)

# --- Pestiféré: the "Charnier" talent tree (pestifere.DESIGN.md section 6) ---
# One Spell.dbc row per talent rank, ids 90230-90299, none of them in the spellbook: the talent frame learns
# them and they stay passive and hidden. A talent with Auras is a real passive the core applies on its own;
# every other one is a dummy marker aura mod-pestifere reads with HasAura, because what it changes lives in
# the module. Description placeholders {0}, {1} take V0/V1 per rank.
# The grid itself - tier, column, prerequisites - is in localTools/customClasses/classes.json, which names the
# same rank ids and is checked against Spell.dbc when it builds Talent.dbc.
$pestifereTalents = @(
    # Tier 1
    @{ Name = 'Peau coriace'; Icon = 'PestifereTalent_PeauCoriace'; FallbackIconSpell = 12299
       Ids = @(90231, 90232, 90233); V0 = @(2, 4, 6)
       Description = 'Augmente votre armure de {0}%.'
       Auras = @(@{ Aura = $A_ModResistancePct; Values = @(2, 4, 6); Misc = 1 }) },
    @{ Name = 'Rage fielleuse'; Icon = 'PestifereTalent_RageFielleuse'; FallbackIconSpell = 29131
       Ids = @(90234, 90235, 90236); V0 = @(10, 20, 30)
       Description = 'Les dégâts que vous subissez vous rapportent {0}% de rage en plus.' },

    # Tier 2
    @{ Name = 'Inoculation rapide'; Icon = 'PestifereTalent_InoculationRapide'; FallbackIconSpell = 12975
       Ids = @(90238, 90239); V0 = @(3, 6)
       Description = 'Vos Inoculations coûtent {0} points de rage de moins.'
       # Rage costs are stored tenfold, so 3 rage is -30
       Auras = @(@{ Aura = $A_AddFlatModifier; Values = @(-30, -60); Misc = $SPELLMOD_COST })
       Fields = @{ 122 = $PF_INOCULATION } },
    @{ Name = 'Miasme'; Icon = 'PestifereTalent_Miasme'; FallbackIconSpell = 50842
       Ids = @(90240, 90241, 90242); V0 = @(1, 2, 3); V1 = @('mètre', 'mètres', 'mètres')
       Description = 'Augmente le rayon de Contagion de {0} {1}.'
       Auras = @(@{ Aura = $A_AddFlatModifier; Values = @(1, 2, 3); Misc = $SPELLMOD_RADIUS })
       Fields = @{ 122 = $PF_CONTAGION } },

    # Tier 3
    @{ Name = 'Mains putrides'; Icon = 'PestifereTalent_MainsPutrides'; FallbackIconSpell = 674
       Ids = @(90243, 90244, 90245); V0 = @(10, 20, 30)
       Description = 'Vos attaques de main gauche ont {0}% de chances d''appliquer une charge de Pourriture.'
       # A proc aura: off-hand hits (auto attacks and off-hand strikes) roll the rank's chance
       ProcFlags = 0x00800000; ProcChance = @(10, 20, 30) },
    @{ Name = 'Fossoyeur'; Icon = 'PestifereTalent_Fossoyeur'; FallbackIconSpell = 12163
       Ids = @(90246, 90247, 90248); V0 = @(8, 16, 24)
       Description = 'Avec une arme à deux mains, Frappe putride inflige {0}% de dégâts supplémentaires et applique 2 charges de Pourriture.' },
    @{ Name = 'Symbiose morbide'; Icon = 'PestifereTalent_SymbioseMorbide'; FallbackIconSpell = 50536
       Ids = @(90249, 90250); V0 = @(15, 30)
       Description = 'Chair putride vous soigne {0}% de plus par fléau que vous portez.' },

    # Tier 4
    @{ Name = 'Métabolisme nécrotique'; Icon = 'PestifereTalent_MetabolismeNecrotique'; FallbackIconSpell = 29131
       Ids = @(90251, 90252, 90253); V0 = @(33, 66, 100)
       Description = 'Les dégâts que vous infligent vos propres fléaux vous rapportent de la rage : {0}% de ce qu''un coup ennemi de même force vous donnerait.' },
    @{ Name = 'Détonation en chaîne'; Icon = 'PestifereTalent_DetonationChaine'; FallbackIconSpell = 49158
       Ids = @(90254, 90255); V0 = @(1, 2)
       V1 = @('ennemi supplémentaire', 'ennemis supplémentaires')
       Description = 'Détonation touche aussi {0} {1} hors de sa zone, à 8 mètres au plus d''un ennemi qui explose. Chacun explose selon sa propre Pourriture.' },

    # Tier 5
    @{ Name = 'Croûte nécrosée'; Icon = 'PestifereTalent_CrouteNecrosee'; FallbackIconSpell = 49222
       Ids = @(90257, 90258, 90259); V0 = @(2, 4, 6)
       Description = 'Carapace nécrosée réduit en plus les dégâts subis de {0}% par fléau que vous portez.' },
    @{ Name = 'Menace contagieuse'; Icon = 'PestifereTalent_MenaceContagieuse'; FallbackIconSpell = 50842
       Ids = @(90260, 90261, 90262); V0 = @(30, 60, 90)
       Description = 'Les dégâts infligés par vos fléaux génèrent {0}% de menace supplémentaire.' },
    @{ Name = 'Porteur endurci'; Icon = 'PestifereTalent_PorteurEndurci'; FallbackIconSpell = 69674
       Ids = @(90263, 90264); V0 = @(15, 30)
       Description = 'Réduit de {0}% les dégâts que la Peste virulente vous inflige.' },

    # Tier 6
    @{ Name = 'Résilience du porteur'; Icon = 'PestifereTalent_ResiliencePorteur'; FallbackIconSpell = 49222
       Ids = @(90266, 90267); V0 = @(3, 6)
       Description = 'Tant que vous portez les trois fléaux, vous subissez {0}% de dégâts en moins.' },

    # The rebuilt tree: procs and choices
    @{ Name = 'Contagion galopante'; Icon = 'PestifereTalent_ContagionGalopante'; FallbackIconSpell = 50842
       Ids = @(90269, 90270); V0 = @(15, 30)
       Description = 'Frappe putride a {0}% de chances de réinitialiser le temps de recharge de Contagion.' },
    @{ Name = 'Fièvre'; Icon = 'PestifereTalent_Fievre'; FallbackIconSpell = 55090
       Ids = @(90271, 90272, 90273); V0 = @(3, 6, 9)
       Description = 'Chaque fois que votre Pourriture inflige des dégâts, vous avez {0}% de chances d''être pris de Fièvre : le temps de recharge de Morsure fétide est réinitialisé et sa prochaine utilisation ne coûte pas de rage.' },
    @{ Name = 'Charognard'; Icon = 'PestifereTalent_Charognard'; FallbackIconSpell = 49206
       Ids = @(90274, 90275); V0 = @(50, 100)
       Description = 'Quand un ennemi portant votre Pourriture meurt, {0}% de ses charges passent à l''ennemi le plus proche.' },
    @{ Name = 'Riposte fétide'; Icon = 'PestifereTalent_RiposteFetide'; FallbackIconSpell = 57823
       Ids = @(90276, 90277); V0 = @('1 ennemi', '2 ennemis'); V1 = @(10, 20)
       Description = 'Riposte purulente applique aussi la Pourriture à {0} de plus et inflige {1}% de dégâts supplémentaires.' },
    @{ Name = 'Bile corrosive'; Icon = 'PestifereTalent_BileCorrosive'; FallbackIconSpell = 43265
       Ids = @(90278, 90279, 90280); V0 = @(2, 4, 6)
       Description = 'Les ennemis dans votre Flaque de bile infligent {0}% de dégâts en moins.' },
    @{ Name = 'Hôte parfait'; Icon = 'PestifereTalent_HoteParfait'; FallbackIconSpell = 49222
       Ids = @(90281, 90282, 90283); V0 = @(1, 2, 3)
       Description = 'Augmente vos chances de parer de {0}% par fléau que vous portez.' },
    @{ Name = 'Crocs infectés'; Icon = 'PestifereTalent_CrocsInfectes'; FallbackIconSpell = 55090
       Ids = @(90288, 90289, 90290); V0 = @(10, 20, 30)
       Description = 'Augmente les dégâts de Morsure fétide de {0}%.'
       Auras = @(@{ Aura = $A_AddPctModifier; Values = @(10, 20, 30); Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = $PF_MORSURE } },
    @{ Name = 'Pandémie prolongée'; Icon = 'PestifereTalent_PandemieProlongee'; FallbackIconSpell = 50536
       Ids = @(90291, 90292); V0 = @(3, 6)
       Description = 'Pandémie dure {0} sec de plus.'
       Auras = @(@{ Aura = $A_AddFlatModifier; Values = @(3000, 6000); Misc = $SPELLMOD_DURATION })
       Fields = @{ 122 = $PF_PANDEMIE } },
    @{ Name = 'Pus épais'; Icon = 'PestifereTalent_PusEpais'; FallbackIconSpell = 48707
       Ids = @(90293, 90294, 90295); V0 = @(10, 20, 30)
       Description = 'Carapace suintante absorbe {0}% de dégâts de plus.' },
    @{ Name = 'Rigor mortis'; Icon = 'PestifereTalent_RigorMortis'; FallbackIconSpell = 48743
       Ids = @(90285)
       Description = 'Un coup qui devrait vous tuer vous laisse à 1 point de vie et dévore les fléaux que vous portez. Ne peut se produire qu''une fois toutes les 3 min.' },
    # Charnier ambulant (90256), Purge cathartique (90265), Sépulcre (90268), Vomissure (90284) and Avatar de la peste
    # (90287) teach an active spell: their rank spell is that spell, defined with the abilities above

    # --- The healer tree "Sangsue" (TalentTab 901), ranks 90300-90399 ---
    @{ Name = 'Humeurs noires'; Icon = 'PestifereHealer_HumeursNoires'; FallbackIconSpell = 49004
       Ids = @(90320, 90321, 90322); V0 = @(1, 2, 3)
       Description = 'Augmente vos chances de coup critique de {0}%.'
       Auras = @(@{ Aura = $A_ModCritPct; Values = @(1, 2, 3) }) },
    @{ Name = 'Transfusion'; Icon = 'PestifereHealer_Transfusion'; FallbackIconSpell = 689
       Ids = @(90300)
       Description = 'Vos coups de mêlée échangent leurs dégâts contre des soins : quand un allié à 40 mètres au plus est blessé, la part d''un coup qui couvre ce qui lui manque soigne l''allié le plus blessé au lieu d''être infligée. Sans blessé, vos coups infligent tous leurs dégâts. Tant qu''un allié est blessé, chaque coup peut aussi déposer des Spores (soin sur la durée, 25% de chances), faire éclater une Pustule (soin de zone, 10%) ou libérer un Essaim qui rebondit sur 3 alliés (10%). Quand un allié tombe sous 35% de ses points de vie, Hémostase triple les soins de vos 3 coups suivants (au plus une fois toutes les 20 sec). Sans effet tant que vous portez la Carapace nécrosée.' },
    @{ Name = 'Veines gonflées'; Icon = 'PestifereHealer_VeinesGonflees'; FallbackIconSpell = 49005
       Ids = @(90323, 90324, 90325); V0 = @(3, 6, 9)
       Description = 'Augmente vos points de vie maximum de {0}%.'
       Auras = @(@{ Aura = $A_ModIncreaseHealthPercent; Values = @(3, 6, 9) }) },
    @{ Name = 'Transfusion vigoureuse'; Icon = 'PestifereHealer_TransfusionVigoureuse'; FallbackIconSpell = 689
       Ids = @(90326, 90327, 90328); V0 = @(5, 10, 15)
       Description = 'Les soins de Transfusion sont augmentés de {0}%.' },
    @{ Name = 'Coagulation'; Icon = 'PestifereHealer_Coagulation'; FallbackIconSpell = 48982
       Ids = @(90329, 90330, 90331); V0 = @(2, 4, 6)
       Description = 'Un allié que Transfusion soigne subit {0}% de dégâts en moins pendant 6 sec.' },
    @{ Name = 'Sangsue vorace'; Icon = 'PestifereHealer_SangsueVorace'; FallbackIconSpell = 5138
       Ids = @(90332, 90333); V0 = @(3, 6)
       Description = 'La sangsue de Sangsue reste attachée {0} sec de plus.'
       Auras = @(@{ Aura = $A_AddFlatModifier; Values = @(3000, 6000); Misc = $SPELLMOD_DURATION })
       Fields = @{ 122 = $PF_SANGSUE } },
    @{ Name = 'Carapace partagée'; Icon = 'PestifereHealer_CarapacePartagee'; FallbackIconSpell = 48707
       Ids = @(90334)
       Description = 'Carapace suintante protège aussi l''allié le plus blessé, pour le même montant.' },
    @{ Name = 'Triage'; Icon = 'PestifereHealer_Triage'; FallbackIconSpell = 48438
       Ids = @(90335, 90336); V0 = @(10, 20)
       Description = 'Vos soins sur un allié à moins de 35% de ses points de vie sont augmentés de {0}%.' },
    @{ Name = 'Saignée profonde'; Icon = 'PestifereHealer_SaigneeProfonde'; FallbackIconSpell = 49998
       Ids = @(90337, 90338, 90339); V0 = @(10, 20, 30)
       Description = 'Saignée inflige {0}% de dégâts supplémentaires, et peut donc en échanger davantage.'
       Auras = @(@{ Aura = $A_AddPctModifier; Values = @(10, 20, 30); Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = $PF_SAIGNEE } },
    @{ Name = 'Anticorps'; Icon = 'PestifereHealer_Anticorps'; FallbackIconSpell = 51052
       Ids = @(90340, 90341, 90342); V0 = @(2, 4, 6)
       Description = 'Réduit de {0}% les dégâts magiques que vous subissez.'
       Auras = @(@{ Aura = $A_ModDamagePercentTaken; Values = @(-2, -4, -6); Misc = 126 }) },
    @{ Name = 'Réserve de sang'; Icon = 'PestifereHealer_ReserveDeSang'; FallbackIconSpell = 55233
       Ids = @(90343, 90344, 90345); V0 = @(10, 20, 30)
       Description = 'Tant qu''aucun allié n''est blessé, {0}% des dégâts de vos coups de mêlée sont mis en réserve, jusqu''à 20% de vos points de vie maximum. Le prochain échange de Transfusion soigne en plus toute la réserve.' },
    @{ Name = 'Sangsue prolifère'; Icon = 'PestifereHealer_SangsueProlifere'; FallbackIconSpell = 5138
       Ids = @(90346, 90347); V0 = @(50, 100)
       Description = 'Quand un ennemi porteur de votre sangsue meurt, elle a {0}% de chances de passer à l''ennemi le plus proche avec sa durée restante.' },
    @{ Name = 'Détonation salvatrice'; Icon = 'PestifereHealer_DetonationSalvatrice'; FallbackIconSpell = 49158
       Ids = @(90348, 90349); V0 = @(50, 100)
       Description = 'Les soins de Détonation vont à vos alliés blessés au lieu de vous et sont augmentés de {0}%.' },
    @{ Name = 'Circulation'; Icon = 'PestifereHealer_Circulation'; FallbackIconSpell = 49016
       Ids = @(90350, 90351, 90352); V0 = @(2, 4, 6)
       Description = 'Augmente votre vitesse d''attaque de {0}%.'
       Auras = @(@{ Aura = $A_ModMeleeHaste; Values = @(2, 4, 6) }) },
    @{ Name = 'Donneur universel'; Icon = 'PestifereHealer_DonneurUniversel'; FallbackIconSpell = 48743
       Ids = @(90353, 90354); V0 = @(50, 100)
       Description = 'Don de sang vous coûte {0}% de points de vie en moins.' },
    @{ Name = 'Contagion bénigne'; Icon = 'PestifereHealer_ContagionBenigne'; FallbackIconSpell = 50842
       Ids = @(90355, 90356, 90357); V0 = @(2, 4, 6)
       Description = 'Contagion soigne aussi les membres de votre groupe à sa portée de {0}% de leurs points de vie maximum, même si vous ne portez aucun fléau.' },
    @{ Name = 'Sang partagé'; Icon = 'PestifereHealer_SangPartage'; FallbackIconSpell = 689
       Ids = @(90358, 90359); V0 = @(10, 20)
       Description = '{0}% des soins de Transfusion que vous prodiguez aux autres vous soignent aussi.' },
    @{ Name = 'Sangsue géante'; Icon = 'PestifereHealer_SangsueGeante'; FallbackIconSpell = 5138
       Ids = @(90363, 90364); V0 = @(15, 30)
       Description = 'Les soins de Sangsue sont augmentés de {0}%.' },
    @{ Name = 'Force vitale'; Icon = 'PestifereHealer_ForceVitale'; FallbackIconSpell = 57330
       Ids = @(90360, 90361, 90362); V0 = @(2, 4, 6)
       Description = 'Augmente votre Force de {0}%.'
       Auras = @(@{ Aura = $A_ModTotalStatPercentage; Values = @(2, 4, 6); Misc = 0 }) },
    @{ Name = 'Symbiose parfaite'; Icon = 'PestifereHealer_SymbioseParfaite'; FallbackIconSpell = 53563
       Ids = @(90365, 90366); V0 = @(35, 50)
       Description = 'Votre Symbiote reçoit {0}% des soins de Transfusion au lieu de 25%.' },
    @{ Name = 'Sang de l''hôte'; Icon = 'PestifereHealer_SangDeLHote'; FallbackIconSpell = 53563
       Ids = @(90367, 90368, 90369); V0 = @(3, 6, 9)
       Description = 'Le porteur de votre Symbiote subit {0}% de dégâts en moins.' },
    @{ Name = 'Hémophagie'; Icon = 'PestifereHealer_Hemophagie'; FallbackIconSpell = 45462
       Ids = @(90370, 90371, 90372); V0 = @(20, 40, 60)
       Description = 'Les soins de Transfusion obtenus par Frappe putride sont augmentés de {0}%.' },
    @{ Name = 'Cœur battant'; Icon = 'PestifereHealer_CoeurBattant'; FallbackIconSpell = 48982
       Ids = @(90375)
       Description = 'Chaque soin de Transfusion soigne aussi l''allié blessé suivant pour 30% de son montant.' },
    @{ Name = 'Spores fertiles'; Icon = 'PestifereHealer_SporesFertiles'; FallbackIconSpell = 774
       Ids = @(90376, 90377, 90378); V0 = @(5, 10, 15)
       Description = 'Vos coups ont {0}% de chances en plus de déposer des Spores.' },
    @{ Name = 'Pustules multiples'; Icon = 'PestifereHealer_PustulesMultiples'; FallbackIconSpell = 49005
       Ids = @(90379, 90380); V0 = @(4, 8); V1 = @(25, 50)
       Description = 'Vos coups ont {0}% de chances en plus de faire éclater une Pustule, et ses éclaboussures soignent {1}% de plus.' },
    @{ Name = 'Essaim vorace'; Icon = 'PestifereHealer_EssaimVorace'; FallbackIconSpell = 33076
       Ids = @(90381, 90382); V0 = @('1 rebond', '2 rebonds')
       Description = 'Votre Essaim fait {0} de plus.' }
    # Sangsue (90302), Saignée (90303), Absorption morbide (90304), Don de sang (90305), Symbiote (90306) and
    # Pestilence salvatrice (90308) teach an active spell: their rank spell is that spell, defined above
)

foreach ($talent in $pestifereTalents) {
    for ($rank = 0; $rank -lt $talent.Ids.Count; ++$rank) {
        $arguments = [object[]]@(
            $(if ($talent.V0) { $talent.V0[$rank] } else { '' }),
            $(if ($talent.V1) { $talent.V1[$rank] } else { '' })
        )
        $effects = @()
        if ($talent.Auras) {
            for ($auraIndex = 0; $auraIndex -lt $talent.Auras.Count; ++$auraIndex) {
                $aura = $talent.Auras[$auraIndex]
                $effects += @{ Index = $auraIndex; Aura = $aura.Aura; Value = $aura.Values[$rank]; Misc = $aura.Misc }
            }
        }
        else {
            $effects += @{ Index = 0; Aura = $A_Dummy }
        }
        $fields = @{}
        if ($talent.Fields) {
            foreach ($field in $talent.Fields.Keys) { $fields[$field] = $talent.Fields[$field] }
        }
        if ($talent.ProcFlags) {
            $fields[$F_ProcFlags] = $talent.ProcFlags
            $fields[$F_ProcChance] = $talent.ProcChance[$rank]
        }
        $customSpells += @{
            Id = $talent.Ids[$rank]; Clone = 2983; Name = $talent.Name; Icon = $talent.Icon
            FallbackIconSpell = $talent.FallbackIconSpell
            Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
            Description = [string]::Format($talent.Description, $arguments)
            Effects = $effects
            Fields = $fields
        }
    }
}

# The Pestiféré's retail-style talent trees (localTools/pestifere/talentTree.json): the rank spells of its new
# talents. The older ones, marked existing there, keep their rows above.
$customSpells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\pestifere\talentTree.json') -Family $FAM_PESTIFERE

$necromancerSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\necromancer\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($necromancerSpellSource))
$oathbladeSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\oathblade\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($oathbladeSpellSource))
# The Mage on its retail-style talent trees (localTools/mage/talentTree.json): abilities, auras and talent ranks
$mageSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\mage\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($mageSpellSource))
# The Rogue on its retail-style talent trees (localTools/rogue/talentTree.json): abilities, auras and talent ranks
$rogueSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\rogue\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($rogueSpellSource))
# The Death Knight on its retail-style talent trees (localTools/deathknight/talentTree.json): abilities, auras and
# talent ranks
$deathKnightSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\deathknight\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($deathKnightSpellSource))
# The Paladin on its retail-style talent trees (localTools/paladin/talentTree.json): Holy Power, abilities, auras and
# talent ranks
$paladinSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\paladin\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($paladinSpellSource))
# The Hunter on its retail-style talent trees (localTools/hunter/talentTree.json): abilities, auras and talent ranks
$hunterSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\hunter\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($hunterSpellSource))
# The Priest on its retail-style talent trees (localTools/priest/talentTree.json): Insanity, abilities, auras and
# talent ranks
$priestSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\priest\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($priestSpellSource))
# The Warrior on its retail-style talent trees (localTools/warrior/talentTree.json): abilities, auras and talent ranks
$warriorSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\warrior\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($warriorSpellSource))
# The Shaman on its retail-style talent trees (localTools/shaman/talentTree.json): abilities, auras and talent ranks
$shamanSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\shaman\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($shamanSpellSource))
# The Warlock on its retail-style talent trees (localTools/warlock/talentTree.json): abilities, auras, hits and talent
# ranks
$warlockSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\warlock\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($warlockSpellSource))
# The Druid on its retail-style talent trees (localTools/druid/talentTree.json): abilities, auras, hits and talent ranks
$druidSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\druid\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($druidSpellSource))
# The Barbarian (class 14, modules/mod-barbarian): its abilities, auras and talent ranks, its looks imported from the
# Ascension client (localTools/barbarian/ascensionVisuals.json)
$barbarianSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\barbarian\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($barbarianSpellSource))
# The Faucheur (class 15, modules/mod-reaper): its abilities, soul resource, auras and talent ranks, its looks imported
# from the Ascension client (localTools/reaper/ascensionVisuals.json)
$reaperSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\reaper\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($reaperSpellSource))
# The Forge (modules/mod-forge): the embers of forged gear and the master smith's hammer
$forgeSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\forge\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($forgeSpellSource))
# The legendaries (modules/mod-legendary): their powers' spells
$legendarySpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\legendary\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($legendarySpellSource))
# L'Infini, the Défi board's god fight (modules/mod-stat-growth/src/InfiniteGod.cpp): its abilities' names in the log
# and its two debuffs
$infiniteBossSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\infiniteBoss\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($infiniteBossSpellSource))
# The Hollow Voice, the Défi board's pinnacle (modules/mod-stat-growth/src/HollowVoice.cpp): its abilities' names in
# the log, its debuffs and the looks its bosses wear
$hollowVoiceSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\hollowVoice\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($hollowVoiceSpellSource))
# Gardien-chef Vorhan, the Défi board's prison warden (modules/mod-stat-growth/src/WardenVorhan.cpp): its hits, its
# rules' debuffs and its cast bars
$wardenVorhanSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\wardenVorhan\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($wardenVorhanSpellSource))
# Le Front du Nord, the open-world content at level 80 (modules/mod-stat-growth/src/frontier): the tier phase and its
# abilities' names in the log
$frontierSpellSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\frontier\Spells.ps1') -Raw -Encoding UTF8
$customSpells += & ([ScriptBlock]::Create($frontierSpellSource))

# The Mage's reagents, gone: Arcane Powder (Arcane Brilliance, Dalaran Brilliance, Ritual of Refreshment), the Runes
# of Teleportation and of Portals, Light Feather (Slow Fall). Chores rather than choices, and the rest of the class
# already asks for none.
$mageFreeReagents = @(17020, 17031, 17032, 17056)

# Stock spells changed in place, by family and English name (every rank of a trainer's chain; only the ranks a skill
# line teaches and that had a cost, so the creatures' and the triggered spells sharing a name are left alone) or by id: fields, and optionally new effects and
# texts. The Hunter's Focus costs, Kill Command, Aimed Shot and Raptor Strike (localTools/hunter/StockSpells.ps1); the
# Priest's snappier casts and Devouring Plague on Insanity (localTools/priest/StockSpells.ps1); the Warrior's abilities
# in any stance, Overpower, Revenge and Execute as retail plays them (localTools/warrior/StockSpells.ps1); the Shaman's
# shocks without a shared cooldown and Chain Lightning without one (localTools/shaman/StockSpells.ps1); the Warlock's
# summons and spells without a Soul Shard item, Chaos Bolt and Shadowburn on Soul Shards (localTools/warlock/StockSpells.ps1);
# the Druid's Shred from any side, Tiger's Fury in percent, Force of Nature and Tranquility on retail cooldowns
# (localTools/druid/StockSpells.ps1); the Rogue's retail looks on Backstab, Ambush, Eviscerate, Hemorrhage and Shadow
# Dance (localTools/rogue/StockSpells.ps1).
$hunterStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\hunter\StockSpells.ps1') -Raw -Encoding UTF8
$priestStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\priest\StockSpells.ps1') -Raw -Encoding UTF8
$warriorStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\warrior\StockSpells.ps1') -Raw -Encoding UTF8
$shamanStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\shaman\StockSpells.ps1') -Raw -Encoding UTF8
$warlockStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\warlock\StockSpells.ps1') -Raw -Encoding UTF8
$druidStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\druid\StockSpells.ps1') -Raw -Encoding UTF8
$rogueStockSource = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\rogue\StockSpells.ps1') -Raw -Encoding UTF8
$stockSpellEdits = @(& ([ScriptBlock]::Create($hunterStockSource))) + @(& ([ScriptBlock]::Create($priestStockSource))) +
    @(& ([ScriptBlock]::Create($warriorStockSource))) + @(& ([ScriptBlock]::Create($shamanStockSource))) +
    @(& ([ScriptBlock]::Create($warlockStockSource))) + @(& ([ScriptBlock]::Create($druidStockSource))) +
    @(& ([ScriptBlock]::Create($rogueStockSource)))
$stockEditsById = @{}
$stockEditsByName = @{}
$stockEditFamilies = [Collections.Generic.HashSet[int]]::new()
# Range entries swapped on every spell of a family that a skill line teaches (the Hunter's shots lose the ranged
# weapon's dead zone), before the named edits so their own range still wins
$stockRangeRemaps = @{}
$stockRangeRemapCount = 0
foreach ($stockEdit in $stockSpellEdits) {
    if ($stockEdit.Ranges) { $stockRangeRemaps[[int]$stockEdit.Family] = $stockEdit.Ranges; continue }
    if ($stockEdit.Id) { $stockEditsById[[int]$stockEdit.Id] = $stockEdit; continue }
    $stockEditsByName["$([int]$stockEdit.Family)|$($stockEdit.Name)"] = $stockEdit
    [void]$stockEditFamilies.Add([int]$stockEdit.Family)
}
$stockEditTexts = @{}
$stockEditCount = 0
# A name reaches the players' spells only: those a skill line teaches (SkillLineAbility), not the creatures' spells
# of the same name and family (an archer's Aimed Shot) that would then ask for a power they do not have
$stockSkillSpells = [Collections.Generic.HashSet[uint32]]::new()
$stockSkillSource = [IO.File]::ReadAllBytes($(if (Test-Path -LiteralPath $skillBackupPath) { $skillBackupPath } else { $serverSkillPath }))
$stockSkillCount = [BitConverter]::ToInt32($stockSkillSource, 4)
$stockSkillRecordSize = [BitConverter]::ToInt32($stockSkillSource, 12)
for ($index = 0; $index -lt $stockSkillCount; ++$index) {
    [void]$stockSkillSpells.Add([BitConverter]::ToUInt32($stockSkillSource, 20 + $index * $stockSkillRecordSize + 2 * 4))
}

& python (Join-Path $repoRoot 'localTools\oathblade\buildSounds.py')
if ($LASTEXITCODE -ne 0) { throw 'Oathblade sound compilation failed.' }

$spellbookSpells = @($customSpells | Where-Object { $_.Spellbook })

$customSounds = @(
    @{ Key = 'DaggerfallImpact'; Clone = 13269; Name = 'Crimson_Daggerfall_Impact'
       Directory = 'Sound\Spells\Custom\CombatRogue'
       Files = @('DaggerfallImpact01.ogg', 'DaggerfallImpact02.ogg', 'DaggerfallImpact03.ogg', 'DaggerfallImpact04.ogg', 'DaggerfallImpact05.ogg')
       # Near normal combat-effect volume while retaining headroom when several target impacts overlap.
       Volume = 0.85 }
    @{ Key = 'OathbladeQuickHit'; Clone = 13269; Name = 'Oathblade_Quick_Hit'
       Directory = 'Sound\Spells\Custom\Oathblade'
       Format = 'wav'
       Files = @('1H_Sword_NPC_Hit_Flesh_01.ogg', '1H_Sword_NPC_Hit_Flesh_02.ogg', '1H_Sword_NPC_Hit_Flesh_03.ogg',
           '1H_Sword_NPC_Hit_Flesh_04.ogg', '1H_Sword_NPC_Hit_Flesh_05.ogg', '1H_Sword_NPC_Hit_Flesh_06.ogg',
           '1H_Sword_NPC_Hit_Flesh_07.ogg', '1H_Sword_NPC_Hit_Flesh_08.ogg', '1H_Sword_NPC_Hit_Flesh_09.ogg',
           '1H_Sword_NPC_Hit_Flesh_10.ogg')
       Volume = 0.82 }
    @{ Key = 'OathbladeParryHit'; Clone = 13269; Name = 'Oathblade_Parry_Hit'
       Directory = 'Sound\Spells\Custom\Oathblade'
       Format = 'wav'
       Files = @('1h_Sword_CritHit_Metal_Parry_02.ogg', '1h_Sword_CritHit_Metal_Parry_03.ogg',
           '1h_Sword_CritHit_Metal_Parry_04.ogg', '1h_Sword_CritHit_Metal_Parry_05.ogg')
       Volume = 0.84 }
    @{ Key = 'OathbladeHeavyHit'; Clone = 13269; Name = 'Oathblade_Heavy_Hit'
       Directory = 'Sound\Spells\Custom\Oathblade'
       Format = 'wav'
       Files = @('2h_Sword_Hit_Flesh_01.ogg', '2h_Sword_Hit_Flesh_02.ogg', '2h_Sword_Hit_Flesh_03.ogg',
           '2h_Sword_Hit_Flesh_04.ogg', '2h_Sword_Hit_Flesh_05.ogg', '2h_Sword_Hit_Flesh_06.ogg',
           '2h_Sword_Hit_Flesh_07.ogg', '2h_Sword_Hit_Flesh_08.ogg', '2h_Sword_Hit_Flesh_09.ogg',
           '2h_Sword_Hit_Flesh_10.ogg')
       Volume = 0.90 }
    @{ Key = 'OathbladeAoE'; Clone = 13269; Name = 'Oathblade_AoE'
       Directory = 'Sound\Spells\Custom\Oathblade'
       Format = 'wav'
       Files = @('sword_aoe1.ogg', 'sword_aoe2.ogg')
       Volume = 0.80 }
    @{ Key = 'OathbladeFinisher'; Clone = 13269; Name = 'Oathblade_Finisher'
       Directory = 'Sound\Spells\Custom\Oathblade\finished_big_hit'
       Format = 'wav'
       Files = @('2H_Sword_CritHit_Stone_Body_01.ogg', '2H_Sword_CritHit_Stone_Body_02.ogg',
           '2H_Sword_CritHit_Stone_Body_03.ogg', '2H_Sword_CritHit_Stone_Body_04.ogg',
           '2H_Sword_CritHit_Stone_Body_05.ogg')
       Volume = 0.95 }
    # L'Infini's track (modules/mod-stat-growth/src/InfiniteGod.cpp sends it with SMSG_PLAY_MUSIC, as stock boss
    # scripts do): a music entry copied from Algalon's own fight music (15877 UR_CelestialPlanetariumBattle: type 28,
    # flags 0, its volume), at a fixed id the server names. The file ships in patch-Z (patchFiles.js).
    @{ Key = 'InfiniteGodTrack'; Id = 30100; Clone = 15877; Name = 'Evolutions_LInfini'
       Directory = 'Sound\Music\Evolutions'
       Files = @('LInfini.mp3')
       Volume = 1.0 }
    # What ends the track (a wipe, the kill): 2 s of silence, the same music type. A music is only ended by sending
    # another; once this one is over the client goes back to the zone's own music. Never send a zone's stock music
    # entry instead: the client's zone music then stops whatever music plays about 20 s later (measured 2026-09-29).
    @{ Key = 'InfiniteGodSilence'; Id = 30101; Clone = 15877; Name = 'Evolutions_LInfiniSilence'
       Directory = 'Sound\Music\Evolutions'
       Files = @('LInfiniSilence.mp3')
       Volume = 0.8 }
    # The Planetarium's own music before the pull ("Demon God's Chamber", made for the fight): the zone music the
    # client plays there (ZoneMusic 514, patched below), copied from the stock entry it replaces (15842
    # UR_CelestialHallWalk). The server never sends it: the fight's track goes over it on the pull.
    @{ Key = 'InfiniteGodRoom'; Id = 30102; Clone = 15842; Name = 'Evolutions_LInfiniRoom'
       Directory = 'Sound\Music\Evolutions'
       Files = @('LInfiniRoom.mp3')
       Volume = 1.0 }
    # The Hollow Voice (modules/mod-stat-growth/src/HollowVoice.cpp): Archbishop Aldric's track (phase 1) and
    # Vel'thazar's (the rest), boss music as L'Infini's; the fight ends them with L'Infini's silence (30101).
    @{ Key = 'HollowVoiceAldric'; Id = 30110; Clone = 15877; Name = 'Evolutions_HollowVoiceAldric'
       Directory = 'Sound\Music\Evolutions'
       Files = @('HollowVoiceAldric.mp3')
       Volume = 1.0 }
    @{ Key = 'HollowVoiceVelthazar'; Id = 30111; Clone = 15877; Name = 'Evolutions_HollowVoiceVelthazar'
       Directory = 'Sound\Music\Evolutions'
       Files = @('HollowVoiceVelthazar.mp3')
       Volume = 1.0 }
    # What the fight sends: the two as one track (localTools/hollowVoice/buildMusic.py), the second from 2:02.0 - a
    # track reaching its end stopped the one sent after it; the two alone are for .hollow music
    @{ Key = 'HollowVoiceFight'; Id = 30112; Clone = 15877; Name = 'Evolutions_HollowVoiceFight'
       Directory = 'Sound\Music\Evolutions'
       Files = @('HollowVoice.mp3')
       Volume = 1.0 }
    # The same from 1:50, for a pull armed with .hollow reveal (a music sent over another fades out a few seconds in)
    @{ Key = 'HollowVoiceFromReveal'; Id = 30113; Clone = 15877; Name = 'Evolutions_HollowVoiceFromReveal'
       Directory = 'Sound\Music\Evolutions'
       Files = @('HollowVoiceFromReveal.mp3')
       Volume = 1.0 }
    # Its abilities' sounds (retail paladin and dreadlord sounds, the Voidspire's paladin trio for the Archbishop's),
    # at fixed ids the server plays (HollowVoice.cpp SoundId): 30120 on, in this order. Heard over the whole chamber at
    # full volume, as Kil'jaeden's are (100 yards, cut at 200): a rogue impact's 8 yards lost them under ten players'
    # spells. The files are levelled by localTools/hollowVoice/buildSounds.py.
    @{ Key = 'HollowVoiceJudgement'; Id = 30120; Clone = 13269; Name = 'Evolutions_HollowVoice_Judgement'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_Judgement.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceBlessedHammers'; Id = 30121; Clone = 13269; Name = 'Evolutions_HollowVoice_BlessedHammers'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_BlessedHammers.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceWrathOfThePulpit'; Id = 30122; Clone = 13269; Name = 'Evolutions_HollowVoice_WrathOfThePulpit'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_WrathOfThePulpit.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceConsecratedAisles'; Id = 30123; Clone = 13269; Name = 'Evolutions_HollowVoice_ConsecratedAisles'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_ConsecratedAisles.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceLightOfDawn'; Id = 30124; Clone = 13269; Name = 'Evolutions_HollowVoice_LightOfDawn'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_LightOfDawn.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceChoir'; Id = 30125; Clone = 13269; Name = 'Evolutions_HollowVoice_Choir'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_Choir.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceAbsolution'; Id = 30126; Clone = 13269; Name = 'Evolutions_HollowVoice_Absolution'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_Absolution.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceExecutionSentence'; Id = 30127; Clone = 13269; Name = 'Evolutions_HollowVoice_ExecutionSentence'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_ExecutionSentence.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceVerdict'; Id = 30128; Clone = 13269; Name = 'Evolutions_HollowVoice_Verdict'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_Verdict.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceSeraphim'; Id = 30129; Clone = 13269; Name = 'Evolutions_HollowVoice_Seraphim'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_Seraphim.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceWakeOfAshes'; Id = 30130; Clone = 13269; Name = 'Evolutions_HollowVoice_WakeOfAshes'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_WakeOfAshes.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceLastRites'; Id = 30131; Clone = 13269; Name = 'Evolutions_HollowVoice_LastRites'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_LastRites.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceReveal'; Id = 30132; Clone = 13269; Name = 'Evolutions_HollowVoice_Reveal'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_Reveal.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceCarrionSwarm'; Id = 30133; Clone = 13269; Name = 'Evolutions_HollowVoice_CarrionSwarm'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_CarrionSwarm.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceVampiricBrand'; Id = 30134; Clone = 13269; Name = 'Evolutions_HollowVoice_VampiricBrand'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_VampiricBrand.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceHollowEcho'; Id = 30135; Clone = 13269; Name = 'Evolutions_HollowVoice_HollowEcho'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_HollowEcho.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceNightmareLances'; Id = 30136; Clone = 13269; Name = 'Evolutions_HollowVoice_NightmareLances'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_NightmareLances.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceVoiceOfRuin'; Id = 30137; Clone = 13269; Name = 'Evolutions_HollowVoice_VoiceOfRuin'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_VoiceOfRuin.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceHardEnrage'; Id = 30138; Clone = 13269; Name = 'Evolutions_HollowVoice_HardEnrage'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_HardEnrage.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    # Where a mechanic lands (HollowVoice.cpp Impact): the boss's big hit, heard over the raid - holy and void, a heavy
    # one of each for the biggest
    @{ Key = 'HollowVoiceImpactHoly'; Id = 30139; Clone = 13269; Name = 'Evolutions_HollowVoice_ImpactHoly'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_ImpactHoly.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceImpactHolyHeavy'; Id = 30140; Clone = 13269; Name = 'Evolutions_HollowVoice_ImpactHolyHeavy'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_ImpactHolyHeavy.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceImpactVoid'; Id = 30141; Clone = 13269; Name = 'Evolutions_HollowVoice_ImpactVoid'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_ImpactVoid.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
    @{ Key = 'HollowVoiceImpactVoidHeavy'; Id = 30142; Clone = 13269; Name = 'Evolutions_HollowVoice_ImpactVoidHeavy'
       Directory = 'Sound\Spells\Custom\HollowVoice'
       Files = @('HV_ImpactVoidHeavy.ogg')
       Volume = 1.0; MinDistance = 100; Cutoff = 200 }
)

# A kit's CharProc parameters are floats; the kit fields are written as raw 32-bit values
function Get-FloatBits([single]$value) {
    return [BitConverter]::ToUInt32([BitConverter]::GetBytes($value), 0)
}

# Custom spell visuals (client only). A spell's Visual clones a stock SpellVisual record and swaps kit slots
# (Precast, Cast, Impact, State, StateDone, Channel, CasterImpact, TargetImpact) for stock kit ids or the custom
# kits below. Preview a stock look in game with ".cast <spell id>" on a target dummy before swapping one.
$customVisualKits = @(
    # Shadowstep smoke, played with a one-hand special attack instead of a spell cast animation
    @{ Key = 'ShadowLungeCast'; Clone = 10123; Fields = @{ 2 = 57 } }
    # AoE impacts (field 15 = SoundEntries id, played on every enemy hit)
    # Cleave slash with a critical one-hand sword hit on flesh
    @{ Key = 'CrescentSlashImpact'; Clone = 507; Fields = @{ 15 = 144 } }
    # Blood Boil burst with Hunger for Blood's wet impact
    @{ Key = 'CrimsonSweepImpact'; Clone = 10282; Fields = @{ 15 = 13269 } }
    # Blood Strike slash with a heavy critical two-hand axe hit on flesh
    @{ Key = 'BloodWaltzImpact'; Clone = 10467; Fields = @{ 15 = 158 } }
    # Dagger Throw impact; one quiet random custom variation is selected independently for every enemy hit.
    @{ Key = 'DaggerfallImpact'; Clone = 220; Sound = 'DaggerfallImpact'; Fields = @{} }
    # Oathblade. The class is bright blue, so its signature is a frost-blue blade (Frost Strike and
    # Obliterate); the holy strikes carry the "oath", and the area abilities spin (Divine Storm, Fan of Knives).
    # Every visual holds exactly one sound, its own recording, and the finishers shake the camera by how rare
    # they are. See localTools\oathblade\Spells.ps1 for which ability wears which.
    #
    #
    # Everything is blue: every swing leaves a blue weapon trail (CharProc 8, the colour and timing copied from
    # stock kits that are blue in game: Frost Strike's frost trail, Whirlwind's and Stormstrike's deep blue),
    # the impacts are frost and arcane, and nothing gold is left (Divine Storm's spin and the Seal hit were).
    # Kit fields: 2 animation, 3-14 effect models (head, chest, base, hands, breath, weapons, specials, world),
    # 15 sound, 16 camera shake, 17-20 CharProc, 21-36 its four float parameters (written as raw bits).
    #
    # Cast kits: animation, weapon effects, the recording, and the shake
    # Sinister Strike's swing and animation, with Frost Strike's trail and a frost slash (it was magenta)
    @{ Key = 'OB_Cast_SwiftCut'; Clone = 10723; Sound = 'OathbladeQuickHit'; Fields = @{ 2 = 17; 5 = 3924 } }
    @{ Key = 'OB_Cast_Thrust'; Clone = 11860; Sound = 'OathbladeQuickHit'; Fields = @{} }
    @{ Key = 'OB_Cast_Advance'; Clone = 324; Sound = 'OathbladeQuickHit'; Fields = @{ 5 = 3924 } }
    @{ Key = 'OB_Cast_Reversal'; Clone = 324; Sound = 'OathbladeParryHit'; Fields = @{ 5 = 3924 } }
    @{ Key = 'OB_Cast_Zeal'; Clone = 10723; Sound = 'OathbladeHeavyHit'; Fields = @{} }
    @{ Key = 'OB_Cast_Verdict'; Clone = 10722; Sound = 'OathbladeHeavyHit'; Fields = @{} }
    # Camera shake, kept light: only Final Edict and the start of the burst move the camera, and only by shake 3,
    # the smallest one that can be felt (amplitude 2, 0.4 sec, one axis). 5 (amplitude 4) and 79 (10, all three
    # axes) were both too much. The frost ground trail spreads from the caster under the blow.
    @{ Key = 'OB_Cast_Edict'; Clone = 10722; Sound = 'OathbladeFinisher'; Fields = @{ 16 = 3; 11 = 4799 } }
    # The spins: Whirlwind's spin and deep blue trail (was Divine Storm, gold), with a frost ring on the ground
    @{ Key = 'OB_Cast_Sweep'; Clone = 369; Sound = 'OathbladeAoE'; Fields = @{ 5 = 282 } }
    @{ Key = 'OB_Cast_Crescent'; Clone = 369; Sound = 'OathbladeAoE'; Fields = @{ 5 = 282; 14 = 4490 } }
    # Fan of Knives' spin and blades, the blades glowing blue
    @{ Key = 'OB_Cast_BladeDance'; Clone = 11409; Sound = 'OathbladeAoE'; Fields = @{ 9 = 1644; 10 = 1644 } }
    # Hungering Cold's frost bursting out of the spin
    @{ Key = 'OB_Cast_Flourish'; Clone = 369; Sound = 'OathbladeFinisher'; Fields = @{ 5 = 282; 14 = 4533 } }
    # Impact kits: the effect on the target, silent
    @{ Key = 'OB_Imp_Frost'; Clone = 10724; Fields = @{ 15 = 0 } }
    # Arcane rather than the Seal's gold hit
    @{ Key = 'OB_Imp_Oath'; Clone = 1005; Fields = @{ 15 = 0 } }
    @{ Key = 'OB_Imp_Verdict'; Clone = 10284; Fields = @{ 15 = 0 } }
    @{ Key = 'OB_Imp_Edict'; Clone = 10727; Fields = @{ 15 = 0 } }
    # Frost Strike's burst on every enemy the spin catches (was Divine Storm's gold)
    @{ Key = 'OB_Imp_Storm'; Clone = 10724; Fields = @{ 15 = 0 } }
    @{ Key = 'OB_Imp_Knives'; Clone = 11408; Fields = @{ 15 = 0; 5 = 4501 } }
    # Howling Blast's frost burst (was the warrior's brown shockwave)
    @{ Key = 'OB_Imp_Shock'; Clone = 10727; Fields = @{ 15 = 0 } }
    #
    # Flawless Form, the burst. It should land like a transformation: a blast of frost out of the Oathblade,
    # the body lit blue and the blades glowing for as long as it lasts, every technique echoed by a crackling
    # arcane-frost strike, and a shatter when it ends.
    # Entering it: Howling Blast's explosion around the caster, its roar, and a light camera nudge
    @{ Key = 'OB_Burst_Cast'; Clone = 10810; Fields = @{ 14 = 4490; 15 = 13167; 16 = 3 } }
    # While it lasts: Icy Veins' frost aura, both blades glowing blue, and the whole body tinted ice-blue
    # (CharProc 1 is the tint; colour and fade are Frost Nova's, which is how a frozen target turns blue)
    @{ Key = 'OB_Burst_State'; Clone = 10991; Fields = @{ 9 = 1644; 10 = 1644; 17 = 1
        21 = (Get-FloatBits 6711039); 25 = 0; 29 = (Get-FloatBits 1); 33 = (Get-FloatBits 0.5) } }
    # Its end: the frost shatters off the body
    @{ Key = 'OB_Burst_End'; Clone = 10727; Fields = @{ 15 = 12879 } }
    # Every echo of a technique during it: Arcane Barrage's crackling burst over Frost Strike's frost, with the
    # barrage's sharp report, so the burst sounds as fast as it plays
    @{ Key = 'OB_Imp_Echo'; Clone = 9849; Fields = @{ 5 = 4501 } }
    #
    # The utility abilities, which used to show their clone's look (Blade Ward none, Flourish and Rally Sprint's)
    # Blade Ward: frost gathered in both hands, then Icebound Fortitude's shell for as long as the ward holds
    @{ Key = 'OB_Ward_Cast'; Clone = 203; Fields = @{ 15 = 0 } }
    @{ Key = 'OB_Ward_State'; Clone = 10299; Fields = @{} }
    # Flourish: an arcane burst at the feet and frost in both hands
    @{ Key = 'OB_Flourish_Cast'; Clone = 1004; Sound = 'OathbladeQuickHit'; Fields = @{ 6 = 126; 7 = 126 } }
    # Rally: frost in both hands and Restoration's rising light, with its sound
    @{ Key = 'OB_Rally_Cast'; Clone = 183; Fields = @{ 5 = 147; 6 = 126; 7 = 126; 15 = 1482 } }
    #
    # The Forge (mod-forge, localTools/forge/Spells.ps1): the embers of worn forged gear, Stratholme's smouldering
    # embers as Feet of Flame (8607) and Complete Immolation (8171) wear them, without their looping fire sound -
    # at the feet, then the feet and both hands, then the chest, the feet and both hands
    @{ Key = 'FORGE_Embers1'; Clone = 8607; Fields = @{ 15 = 0 } }
    @{ Key = 'FORGE_Embers2'; Clone = 8607; Fields = @{ 6 = 3899; 7 = 3899; 15 = 0 } }
    # Molten heart (Fire Reflection's chest glow) over Retribution Aura's red ring, embers in the hands
    @{ Key = 'FORGE_Embers3'; Clone = 8607; Fields = @{ 4 = 2862; 5 = 593; 6 = 3899; 7 = 3899; 15 = 0 } }
    # The head aflame (Molten Armor's) over the molten heart and the ring of heat
    @{ Key = 'FORGE_Embers4'; Clone = 8607; Fields = @{ 3 = 3100; 4 = 2862; 5 = 593; 6 = 3899; 7 = 3899; 15 = 0 } }
    # Avatar of the Forge: the head aflame and the molten heart, Lightning Shield's charges circling low at the feet
    # (4245, its low version) and lightning crackling in both hands, where the weapons are. Nothing rises above the
    # character: the tall pillar of light and the wings it had filled the screen. Every few seconds mod-forge strikes
    # it with lightning (FORGE_Lightning).
    @{ Key = 'FORGE_Embers5'; Clone = 8607; Fields = @{ 3 = 3100; 4 = 2862; 5 = 4245; 6 = 88; 7 = 88; 15 = 0 } }
    # The Avatar's lightning: Lightning Bolt's strike on the chest and Lightning Shield's discharge at the feet, silent
    @{ Key = 'FORGE_Lightning'; Clone = 556; Fields = @{ 2 = 0; 4 = 281; 5 = 173; 7 = 0; 15 = 0 } }
    # The master smith's hammer on the anvil: the capitals' smiths' own anvil work (animation 136, the one emote 233
    # loops), Fire Blast's molten burst at the hammer hand, and Sunder Armor's clang of metal (no chest effect)
    @{ Key = 'FORGE_Strike'; Clone = 556; Fields = @{ 2 = 136; 4 = 0; 7 = 321 } }
    #
    # The Hollow Voice (localTools/hollowVoice/Spells.ps1): Beam of Light's pillar (its state kit, 11581) without its
    # sound, Holy_Form_Precast, which loops (flag 0x200) for as long as the aura stays: it went on after a held tower
    @{ Key = 'HV_LightPillar'; Clone = 11581; Fields = @{ 15 = 0 } }
    # and Mark of Rimefang's reticle (its state kit, 13250) without Sindragosa_DarkmoonVengeance_Impact_Head, which went on
    # for as long as the mark
    @{ Key = 'HV_MarkReticle'; Clone = 13250; Fields = @{ 15 = 0 } }
)
$visualKitSlots = @{
    Precast = 1; Cast = 2; Impact = 3; State = 4; StateDone = 5; Channel = 6; CasterImpact = 14; TargetImpact = 15
    # The ground effect of a persistent area (a pool, a cloud)
    PersistentArea = 25
}

# The whole Combat talent tree, rewritten on the WotLK talent spell ids (same grid, ranks and prerequisites).
# Description placeholders {0}, {1} take V0/V1 per rank. Auras are real passive effects; everything else is scripted
# in modules/mod-stat-growth/src/CombatRogue*.cpp.
$combatTalents = @(
    @{ Name = 'Keen Openings'; Icon = 'CombatRogue_KeenOpenings'; Ids = @(13741, 13793, 13792); V0 = @(5, 10, 15)
       Description = 'Increases the chance for Sinister Strike to grant Opening by {0}%.' },
    @{ Name = 'Honed Blades'; Icon = 'CombatRogue_HonedBlades'; Ids = @(13732, 13863); V0 = @(5, 10); V1 = @(2, 4)
       Description = 'Sinister Strike deals {0}% more damage and generates {1} additional Energy.' },
    @{ Name = 'Crimson Discipline'; Icon = 'CombatRogue_CrimsonDiscipline'; Ids = @(13715, 13848, 13849, 13851, 13852); V0 = @(3, 6, 9, 12, 15); V1 = @(5, 10, 15, 20, 25)
       Description = 'Crescent Slash and Crimson Sweep deal {0}% more damage, and your off-hand weapon deals {1}% more damage.'
       Auras = @(@{ Aura = $A_ModOffhandDamagePct; Values = @(5, 10, 15, 20, 25) }) },
    @{ Name = 'Tempo Mastery'; Icon = 'CombatRogue_TempoMastery'; Ids = @(14165, 14166); V0 = @(1, 2); V1 = @(2, 4)
       Description = 'Battle Tempo grants an additional {0}% attack speed per stack and lasts {1} sec longer.' },
    @{ Name = 'Parry Instinct'; Icon = 'CombatRogue_ParryInstinct'; Ids = @(13713, 13853, 13854); V0 = @(1, 2, 3); V1 = @(3, 6, 9)
       Description = 'Increases your dodge and parry chance by {0}%. Dodging or parrying an attack restores {1} Energy.'
       Auras = @(@{ Aura = $A_ModDodgePercent; Values = @(1, 2, 3) }, @{ Aura = $A_ModParryPercent; Values = @(1, 2, 3) }) },
    @{ Name = 'Surgical Precision'; Icon = 'CombatRogue_SurgicalPrecision'; Ids = @(13705, 13832, 13843, 13844, 13845); V0 = @(1, 2, 3, 4, 5)
       Description = 'Increases your chance to hit with melee attacks by {0}% and your expertise by {0}.'
       Auras = @(@{ Aura = $A_ModHitChance; Values = @(1, 2, 3, 4, 5) }, @{ Aura = $A_ModExpertise; Values = @(1, 2, 3, 4, 5) }) },
    @{ Name = 'Fleet Footwork'; Icon = 'CombatRogue_FleetFootwork'; Ids = @(13742, 13872); V0 = @(2, 4)
       Description = 'Reduces the cooldown of Shadow Lunge by {0} sec.' },
    @{ Name = 'Counter Rhythm'; Icon = 'CombatRogue_CounterRhythm'; Ids = @(14251)
       Description = 'Dodging or parrying an attack resets the cooldown of Riposte. This effect cannot occur more than once every 6 sec. A Riposte used within 5 sec of a dodge or parry grants Opening.' },
    @{ Name = 'Deep Cuts'; Icon = 'CombatRogue_DeepCuts'; Ids = @(13706, 13804, 13805, 13806, 13807); V0 = @(6, 12, 18, 24, 30)
       Description = 'Increases the damage done by your Crimson Wounds by {0}%.' },
    @{ Name = 'Opportunist'; Icon = 'CombatRogue_Opportunist'; Ids = @(13754, 13867); V0 = @(10, 20)
       Description = 'Interrupting a spell with Kick grants Opening and {0} Energy.' },
    @{ Name = 'Relentless Pursuit'; Icon = 'CombatRogue_RelentlessPursuit'; Ids = @(13743, 13875); V0 = @(2, 4)
       Description = 'Each stack of Killing Momentum grants an additional {0}% movement speed and Energy regeneration.' },
    @{ Name = 'Sanguine Instinct'; Icon = 'CombatRogue_SanguineInstinct'; Ids = @(13712, 13788, 13789); V0 = @(2, 4, 6)
       Description = 'Sanguine Veil heals you for an additional {0}% of damage dealt.' },
    @{ Name = 'Lethal Edge'; Icon = 'CombatRogue_LethalEdge'; Ids = @(18427, 18428, 18429, 61330, 61331); V0 = @(1, 2, 3, 4, 5)
       Description = 'Increases your critical strike chance by {0}%.'
       Auras = @(@{ Aura = $A_ModCritPct; Values = @(1, 2, 3, 4, 5) }) },
    @{ Name = 'Twin Cuts'; Icon = 'CombatRogue_TwinCuts'; Ids = @(13709, 13800, 13801, 13802, 13803); V0 = @(4, 8, 12, 16, 20)
       Description = 'Quick Cut has a {0}% chance not to consume Opening.' },
    @{ Name = 'Flowing Strikes'; Icon = 'CombatRogue_FlowingStrikes'; Ids = @(13877)
       Description = 'Every 4th Sinister Strike automatically performs a free Quick Cut on its target, without needing or consuming Opening.' },
    @{ Name = 'Rending Arc'; Icon = 'CombatRogue_RendingArc'; Ids = @(13960, 13961, 13962, 13963, 13964); V0 = @(4, 8, 12, 16, 20); V1 = @('.', '.', '.', '.', ' and adds 2 sec to their Crimson Wounds, up to its full duration.')
       Description = 'Crescent Slash deals {0}% more damage to enemies affected by your Crimson Wounds{1}' },
    @{ Name = 'Relentless Flow'; Icon = 'CombatRogue_RelentlessFlow'; Ids = @(30919, 30920); V0 = @('.', ', and its free Quick Cut restores 10 Energy.')
       Description = 'Flowing Strikes triggers on every 3rd Sinister Strike{0}' },
    @{ Name = 'Widening Arcs'; Icon = 'CombatRogue_WideningArcs'; Ids = @(31124, 31126); V0 = @(2, 4)
       Description = 'Increases the radius of Crescent Slash, Crimson Sweep, Blood Waltz and Crimson Daggerfall by {0} yards.' },
    @{ Name = 'Tempo Echo'; Icon = 'CombatRogue_TempoEcho'; Ids = @(31122, 31123, 61329); V0 = @(10, 20, 30)
       Description = 'Eviscerate has a {0}% chance to strike again 0.5 sec later for 40% of its damage.' },
    @{ Name = 'Adrenaline Flow'; Icon = 'CombatRogue_AdrenalineFlow'; Ids = @(13750)
       Description = 'Increases your Energy regeneration by 10%. Your finishing moves have a 20% chance to refund 1 combo point.'
       Auras = @(@{ Aura = $A_ModPowerRegenPercent; Values = @(10); Misc = 3 }) },
    @{ Name = 'Sanguine Resolve'; Icon = 'CombatRogue_SanguineResolve'; Ids = @(31130, 31131); V0 = @(3, 6)
       Description = 'While Sanguine Veil is active, damage taken is reduced by {0}%.' },
    @{ Name = 'Relentless Tempo'; Icon = 'CombatRogue_RelentlessTempo'; Ids = @(5952, 51679); V0 = @(10, 20)
       Description = 'Your finishing moves have a {0}% chance per combo point spent to grant Opening.' },
    @{ Name = 'Hemorrhaging Blades'; Icon = 'CombatRogue_HemorrhagingBlades'; Ids = @(35541, 35550, 35551, 35552, 35553); V0 = @(4, 8, 12, 16, 20)
       Description = 'Crescent Slash has a {0}% chance to apply Crimson Wounds for 6 sec to each enemy it hits that is not already bleeding.' },
    @{ Name = 'Riposte Echo'; Icon = 'CombatRogue_RiposteEcho'; Ids = @(51672, 51674); V0 = @(1, 2); V1 = @('enemy', 'enemies')
       Description = 'Riposte also strikes {0} additional {1} in front of you, within 5 yards, for the same damage.' },
    @{ Name = 'Waltz of Blades'; Icon = 'CombatRogue_WaltzOfBlades'; Ids = @(32601)
       Description = 'Blood Waltz grants 1 combo point for each enemy hit, up to 3.' },
    @{ Name = 'Crimson Frenzy'; Icon = 'CombatRogue_CrimsonFrenzy'; Ids = @(51682, 58413); V0 = @(1, 2)
       Description = 'Each tick of your Crimson Wounds grants you {0}% attack speed for 5 sec, stacking up to 5 times.' },
    @{ Name = 'Executioner''s Tempo'; Icon = 'CombatRogue_ExecutionersTempo'; Ids = @(51685, 51686, 51687, 51688, 51689); V0 = @(4, 8, 12, 16, 20); V1 = @(3, 6, 9, 12, 15)
       Description = 'Your abilities deal {0}% more damage to enemies below 35% health, and killing an enemy restores {1} Energy.' },
    @{ Name = 'Crimson Cadence'; Icon = 'CombatRogue_CrimsonCadence'; Ids = @(51690)
       Description = 'Every 5th finishing move releases a free Blood Waltz with 5 combo points when 2 or more enemies are within its reach. Otherwise it releases an echo of your last Eviscerate for 50% of its damage.' }
)

$talentRankInfo = @{}
foreach ($talent in $combatTalents) {
    for ($rank = 0; $rank -lt $talent.Ids.Count; ++$rank) {
        $talentRankInfo[[int]$talent.Ids[$rank]] = @{ Talent = $talent; Rank = $rank }
    }
}

function Assert-Wdbc([byte[]]$data, [string]$name) {
    if ($data.Length -lt 21 -or [Text.Encoding]::ASCII.GetString($data, 0, 4) -ne 'WDBC') {
        throw "$name has an invalid WDBC header."
    }
}

# The loops that run over every record or every locale, compiled: a PowerShell function call costs tens of microseconds,
# and these ran hundreds of thousands of times (a quarter of the client build)
Add-Type -TypeDefinition @'
public static class DbcFast
{
    // Every localized string group of every record: the locales left empty take the English text
    public static void FillLocales(byte[] records, int count, int recordSize, int[] groups)
    {
        for (int index = 0; index < count; ++index)
        {
            int offset = index * recordSize;
            foreach (int group in groups)
            {
                uint english = System.BitConverter.ToUInt32(records, offset + group * 4);
                if (english == 0)
                    continue;
                for (int locale = 1; locale < 16; ++locale)
                {
                    int at = offset + (group + locale) * 4;
                    if (System.BitConverter.ToUInt32(records, at) == 0)
                        System.BitConverter.GetBytes(english).CopyTo(records, at);
                }
            }
        }
    }

    // Fields `first` to `last` of a record set to zero
    public static void Clear(byte[] buffer, int baseOffset, int first, int last)
    {
        System.Array.Clear(buffer, baseOffset + first * 4, (last - first + 1) * 4);
    }

    // One string in all 16 locales of a group, from `firstField`
    public static void SetLocalized(byte[] buffer, int baseOffset, int firstField, uint value)
    {
        byte[] bytes = System.BitConverter.GetBytes(value);
        for (int locale = 0; locale < 16; ++locale)
            bytes.CopyTo(buffer, baseOffset + (firstField + locale) * 4);
    }
}
'@

function Set-Field([byte[]]$record, [int]$field, [uint32]$value) {
    [BitConverter]::GetBytes($value).CopyTo($record, $field * 4)
}

# Writes a field of a record starting at $base inside $buffer; accepts negative values (stored as 32-bit)
function Write-Field([byte[]]$buffer, [int]$base, [int]$field, [long]$value) {
    [BitConverter]::GetBytes([uint32]($value -band 4294967295)).CopyTo($buffer, $base + $field * 4)
}

function Read-Field([byte[]]$buffer, [int]$base, [int]$field) {
    return [BitConverter]::ToUInt32($buffer, $base + $field * 4)
}

function Add-DbcString([Collections.Generic.List[byte]]$strings, [string]$value) {
    $offset = $strings.Count
    $strings.AddRange([Text.Encoding]::UTF8.GetBytes($value))
    $strings.Add(0)
    return [uint32]$offset
}

function Write-LocalizedString([byte[]]$buffer, [int]$base, [int]$firstField, [uint32]$stringOffset) {
    [DbcFast]::SetLocalized($buffer, $base, $firstField, $stringOffset)
}

# Replaces all three effects. Value = final amount (base points + 1 die); BasePoints = raw base points (no die).
function Write-Effects([byte[]]$buffer, [int]$base, $effects) {
    [DbcFast]::Clear($buffer, $base, 71, 130)
    foreach ($effect in $effects) {
        $index = [int]$effect.Index
        Write-Field $buffer $base (71 + $index) $(if ($null -ne $effect.Effect) { $effect.Effect } else { 6 })
        Write-Field $buffer $base (86 + $index) $(if ($null -ne $effect.TargetA) { $effect.TargetA } else { 1 })
        if ($null -ne $effect.Aura) { Write-Field $buffer $base (95 + $index) $effect.Aura }
        if ($null -ne $effect.Value) {
            Write-Field $buffer $base (80 + $index) ($effect.Value - 1)
            Write-Field $buffer $base (74 + $index) 1
        }
        elseif ($null -ne $effect.BasePoints) {
            Write-Field $buffer $base (80 + $index) $effect.BasePoints
        }
        if ($null -ne $effect.Misc) { Write-Field $buffer $base (110 + $index) $effect.Misc }
    }
}

if (-not (Test-Path -LiteralPath $spellBackupPath)) {
    Copy-Item -LiteralPath $serverSpellPath -Destination $spellBackupPath
}
if (-not (Test-Path -LiteralPath $skillBackupPath)) {
    Copy-Item -LiteralPath $serverSkillPath -Destination $skillBackupPath
}
if (-not (Test-Path -LiteralPath $iconBackupPath)) {
    Copy-Item -LiteralPath $serverIconPath -Destination $iconBackupPath
}
if (-not (Test-Path -LiteralPath $visualBackupPath)) {
    Copy-Item -LiteralPath $serverVisualPath -Destination $visualBackupPath
}
if (-not (Test-Path -LiteralPath $visualKitBackupPath)) {
    Copy-Item -LiteralPath $serverVisualKitPath -Destination $visualKitBackupPath
}
if (-not (Test-Path -LiteralPath $soundBackupPath)) {
    Copy-Item -LiteralPath $serverSoundPath -Destination $soundBackupPath
}
if (-not (Test-Path -LiteralPath $effectNameBackupPath)) {
    Copy-Item -LiteralPath $serverEffectNamePath -Destination $effectNameBackupPath
}
if (-not (Test-Path -LiteralPath $attachBackupPath)) {
    Copy-Item -LiteralPath $serverAttachPath -Destination $attachBackupPath
}
if (-not (Test-Path -LiteralPath $motionBackupPath)) {
    Copy-Item -LiteralPath $serverMotionPath -Destination $motionBackupPath
}

# Table DBC without strings: records by id, and appends new records built from copies of existing ones
function Read-Dbc([string]$path, [string]$name, [int]$fields) {
    $data = [IO.File]::ReadAllBytes($path)
    Assert-Wdbc $data $name
    $dbc = @{
        Name = $name; Data = $data; Count = [BitConverter]::ToInt32($data, 4)
        RecordSize = [BitConverter]::ToInt32($data, 12); Offsets = @{}; MaxId = 0
        NewRecords = [Collections.Generic.List[byte]]::new()
    }
    if ([BitConverter]::ToInt32($data, 8) -ne $fields -or $dbc.RecordSize -ne $fields * 4) {
        throw "Unexpected $name layout."
    }
    for ($index = 0; $index -lt $dbc.Count; ++$index) {
        $offset = 20 + $index * $dbc.RecordSize
        $id = [BitConverter]::ToUInt32($data, $offset)
        $dbc.Offsets[[int]$id] = $offset
        if ($id -gt $dbc.MaxId) { $dbc.MaxId = $id }
    }
    return $dbc
}

function Add-DbcRecordCopy($dbc, [int]$cloneId, $fields) {
    if (-not $dbc.Offsets.ContainsKey($cloneId)) { throw "$($dbc.Name) record $cloneId was not found." }
    $record = [byte[]]::new($dbc.RecordSize)
    [Array]::Copy($dbc.Data, $dbc.Offsets[$cloneId], $record, 0, $dbc.RecordSize)
    $dbc.MaxId = $dbc.MaxId + 1
    Set-Field $record 0 ([uint32]$dbc.MaxId)
    foreach ($field in $fields.Keys) { Write-Field $record 0 ([int]$field) ([long]$fields[$field]) }
    $dbc.NewRecords.AddRange($record)
    return [uint32]$dbc.MaxId
}

function Get-DbcOutput($dbc) {
    $recordsSize = $dbc.Count * $dbc.RecordSize
    $stringSize = [BitConverter]::ToInt32($dbc.Data, 16)
    $output = [byte[]]::new($dbc.Data.Length + $dbc.NewRecords.Count)
    [Array]::Copy($dbc.Data, 0, $output, 0, 20 + $recordsSize)
    [BitConverter]::GetBytes([uint32]($dbc.Count + $dbc.NewRecords.Count / $dbc.RecordSize)).CopyTo($output, 4)
    $dbc.NewRecords.CopyTo($output, 20 + $recordsSize)
    [Array]::Copy($dbc.Data, 20 + $recordsSize, $output, 20 + $recordsSize + $dbc.NewRecords.Count, $stringSize)
    return $output
}

# String-backed DBC used for custom sounds. Existing string offsets stay valid because new strings are appended.
function Read-StringDbc([string]$path, [string]$name, [int]$fields) {
    $data = [IO.File]::ReadAllBytes($path)
    Assert-Wdbc $data $name
    $count = [BitConverter]::ToInt32($data, 4)
    $recordSize = [BitConverter]::ToInt32($data, 12)
    if ([BitConverter]::ToInt32($data, 8) -ne $fields -or $recordSize -ne $fields * 4) {
        throw "Unexpected $name layout."
    }
    $recordsSize = $count * $recordSize
    $stringSize = [BitConverter]::ToInt32($data, 16)
    $strings = [Collections.Generic.List[byte]]::new()
    $strings.AddRange([byte[]]$data[(20 + $recordsSize)..(20 + $recordsSize + $stringSize - 1)])
    $dbc = @{
        Name = $name; Data = $data; Count = $count; RecordSize = $recordSize; RecordsSize = $recordsSize
        Strings = $strings; Offsets = @{}; MaxId = 0; NewRecords = [Collections.Generic.List[byte]]::new()
    }
    for ($index = 0; $index -lt $count; ++$index) {
        $offset = 20 + $index * $recordSize
        $id = [BitConverter]::ToUInt32($data, $offset)
        $dbc.Offsets[[int]$id] = $offset
        if ($id -gt $dbc.MaxId) { $dbc.MaxId = $id }
    }
    return $dbc
}

function Add-SoundEntry($dbc, $sound) {
    if (-not $dbc.Offsets.ContainsKey([int]$sound.Clone)) {
        throw "$($dbc.Name) record $($sound.Clone) was not found."
    }
    $record = [byte[]]::new($dbc.RecordSize)
    [Array]::Copy($dbc.Data, $dbc.Offsets[[int]$sound.Clone], $record, 0, $dbc.RecordSize)
    # An entry the server sends by id (music) keeps the one it is given; the others take the next free one
    if ($sound.Id) {
        if ($dbc.Offsets.ContainsKey([int]$sound.Id)) { throw "$($dbc.Name) already has a row $($sound.Id)." }
        $id = [uint32]$sound.Id
    }
    else {
        $dbc.MaxId = $dbc.MaxId + 1
        $id = [uint32]$dbc.MaxId
    }
    Set-Field $record 0 $id
    Set-Field $record 2 (Add-DbcString $dbc.Strings $sound.Name)
    for ($index = 0; $index -lt 10; ++$index) {
        if ($index -lt $sound.Files.Count) {
            $fileName = $sound.Files[$index]
            if ($sound.Format -eq 'wav') { $fileName = [IO.Path]::ChangeExtension($fileName, '.wav') }
            Set-Field $record (3 + $index) (Add-DbcString $dbc.Strings $fileName)
            Set-Field $record (13 + $index) 1
        }
        else {
            Set-Field $record (3 + $index) 0
            Set-Field $record (13 + $index) 0
        }
    }
    Set-Field $record 23 (Add-DbcString $dbc.Strings $sound.Directory)
    Set-Field $record 24 ([BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$sound.Volume), 0))
    # Heard this far at full volume, and not at all past the cutoff (a boss's sound: the whole room)
    if ($sound.MinDistance) {
        Set-Field $record 26 ([BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$sound.MinDistance), 0))
    }
    if ($sound.Cutoff) {
        Set-Field $record 27 ([BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$sound.Cutoff), 0))
    }
    $dbc.NewRecords.AddRange($record)
    return $id
}

function Get-StringDbcOutput($dbc) {
    $newCount = $dbc.Count + $dbc.NewRecords.Count / $dbc.RecordSize
    $output = [byte[]]::new(20 + $newCount * $dbc.RecordSize + $dbc.Strings.Count)
    [Array]::Copy($dbc.Data, 0, $output, 0, 20 + $dbc.RecordsSize)
    [BitConverter]::GetBytes([uint32]$newCount).CopyTo($output, 4)
    [BitConverter]::GetBytes([uint32]$dbc.Strings.Count).CopyTo($output, 16)
    $dbc.NewRecords.CopyTo($output, 20 + $dbc.RecordsSize)
    $dbc.Strings.CopyTo($output, 20 + $dbc.RecordsSize + $dbc.NewRecords.Count)
    return $output
}

# --- SoundEntries.dbc / SpellVisualKit.dbc / SpellVisual.dbc: custom sound and visuals ---
$soundDbc = Read-StringDbc $soundBackupPath 'SoundEntries.dbc' 30
# The game's combat noise turned down, so a boss's sounds (ours, added below at full volume) carry over ten players'
# spells: the Hollow Voice's were at full volume and full scale already, and still lost under the raid's. Spells (1),
# weapon impacts and swings (4, 6), the characters' and creatures' combat sounds (10) and siege attacks (53) play at
# CombatSoundVolume of their own volume; music, ambience, voices and the interface are left as they are. A player
# turns the effects up a little and the bosses stand out twice as much.
$CombatSoundVolume = 0.45
$combatSoundTypes = @(1, 4, 6, 10, 53)
foreach ($id in @($soundDbc.Offsets.Keys)) {
    $base = $soundDbc.Offsets[$id]
    if ($combatSoundTypes -notcontains [int](Read-Field $soundDbc.Data $base 1)) { continue }
    $volume = [BitConverter]::ToSingle($soundDbc.Data, $base + 24 * 4)
    Write-Field $soundDbc.Data $base 24 ([BitConverter]::ToUInt32([BitConverter]::GetBytes([single]($volume * $CombatSoundVolume)), 0))
}
$soundIdsByKey = @{}
foreach ($sound in $customSounds) {
    $soundIdsByKey[$sound.Key] = Add-SoundEntry $soundDbc $sound
}
$visualDbc = Read-Dbc $visualBackupPath 'SpellVisual.dbc' 32
$visualKitDbc = Read-Dbc $visualKitBackupPath 'SpellVisualKit.dbc' 38

# --- SpellVisualEffectName.dbc: the Oathblade's own blue effect models ---
#
# Every effect model an Oathblade kit (OB_*) plays, its own or inherited from the kit it clones, gets a blue copy
# (localTools\oathblade\buildBlueEffects.py) and a new effect record naming it, and the kit is pointed at that record.
# The stock models and records are left alone, so no other class's spells change.
$effectNameDbc = Read-StringDbc $effectNameBackupPath 'SpellVisualEffectName.dbc' 7
$EffectFields = 3..14

function Get-KitField($kit, [int]$field) {
    if ($kit.Fields.ContainsKey($field)) { return [uint32]$kit.Fields[$field] }
    return Read-Field $visualKitDbc.Data $visualKitDbc.Offsets[[int]$kit.Clone] $field
}

function Get-StringDbcString($dbc, [uint32]$offset) {
    $start = 20 + $dbc.RecordsSize + $offset
    $end = [Array]::IndexOf($dbc.Data, [byte]0, $start)
    return [Text.Encoding]::UTF8.GetString($dbc.Data, $start, $end - $start)
}

$oathbladeKits = @($customVisualKits | Where-Object { $_.Key -like 'OB_*' })
$stockEffectIds = [Collections.Generic.SortedSet[uint32]]::new()
foreach ($kit in $oathbladeKits) {
    foreach ($field in $EffectFields) {
        $effectId = Get-KitField $kit $field
        if ($effectId) { [void]$stockEffectIds.Add($effectId) }
    }
}
$effectList = @(foreach ($effectId in $stockEffectIds) {
    if (-not $effectNameDbc.Offsets.ContainsKey([int]$effectId)) {
        throw "SpellVisualEffectName $effectId was not found."
    }
    $fileName = Read-Field $effectNameDbc.Data $effectNameDbc.Offsets[[int]$effectId] 2
    [ordered]@{ id = $effectId; path = Get-StringDbcString $effectNameDbc $fileName }
})
$effectListPath = Join-Path ([IO.Path]::GetTempPath()) 'oathblade-effects.json'
$effectMappingPath = Join-Path ([IO.Path]::GetTempPath()) 'oathblade-effect-mapping.json'
[IO.File]::WriteAllText($effectListPath, (ConvertTo-Json -InputObject $effectList -Depth 3))
& python (Join-Path $repoRoot 'localTools\oathblade\buildBlueEffects.py') $effectListPath $effectMappingPath
if ($LASTEXITCODE -ne 0) { throw 'Oathblade blue effect models failed to build.' }
$blueEffectIds = @{}
foreach ($entry in (Get-Content -LiteralPath $effectMappingPath -Raw | ConvertFrom-Json).PSObject.Properties) {
    $record = [byte[]]::new($effectNameDbc.RecordSize)
    [Array]::Copy($effectNameDbc.Data, $effectNameDbc.Offsets[[int]$entry.Name], $record, 0, $effectNameDbc.RecordSize)
    $effectNameDbc.MaxId = $effectNameDbc.MaxId + 1
    Set-Field $record 0 ([uint32]$effectNameDbc.MaxId)
    Set-Field $record 1 (Add-DbcString $effectNameDbc.Strings ('Oathblade ' + [IO.Path]::GetFileNameWithoutExtension($entry.Value)))
    Set-Field $record 2 (Add-DbcString $effectNameDbc.Strings $entry.Value)
    $effectNameDbc.NewRecords.AddRange($record)
    $blueEffectIds[[uint32]$entry.Name] = [uint32]$effectNameDbc.MaxId
}

# --- The red ground indicators of enemy abilities (localTools\groundIndicators) ---
#
# One spell per shape: a hidden, never-ending dummy aura (cloned from Beam Target State, 62898) whose state kit
# plays the shape's model at its owner's feet. The server puts it on an invisible stalker scaled to the ability's
# size (modules/mod-stat-growth/src/GroundIndicators.cpp). The effect record copies the stock ground ring's (5240:
# scale 1), the kit the one that ring's aura plays (12338) with nothing else in it.
$indicatorConfig = Get-Content -LiteralPath (Join-Path $repoRoot 'localTools\groundIndicators\shapes.json') -Raw | ConvertFrom-Json
# A painting's fading twin (GI_<key>Fade, its alpha falling to nothing in 0.3 s): the server turns a carrier to it just
# before the carrier goes (GroundIndicators.cpp FadeLookOf, buildGroundIndicators.py fade_spell: the same rule)
function Get-IndicatorFadeSpell([int]$spell) {
    if ($spell -ge 90600 -and $spell -lt 90900) { return $spell + 6000 }
    if ($spell -ge 94000 -and $spell -lt 94200) { return $spell + 200 }
    if ($spell -ge 94200 -and $spell -lt 94250) { return $spell + 300 }
    throw "Indicator spell $spell has no fading twin range."
}
foreach ($shape in $indicatorConfig.shapes) {
    $looks = @(@{ Key = $shape.key; Spell = [int]$shape.spell })
    if (-not $shape.carried) {
        $looks += @{ Key = "$($shape.key)Fade"; Spell = (Get-IndicatorFadeSpell ([int]$shape.spell)) }
    }
    foreach ($look in $looks) {
        $record = [byte[]]::new($effectNameDbc.RecordSize)
        [Array]::Copy($effectNameDbc.Data, $effectNameDbc.Offsets[5240], $record, 0, $effectNameDbc.RecordSize)
        $effectNameDbc.MaxId = $effectNameDbc.MaxId + 1
        Set-Field $record 0 ([uint32]$effectNameDbc.MaxId)
        Set-Field $record 1 (Add-DbcString $effectNameDbc.Strings "Evolutions Indicator $($look.Key)")
        Set-Field $record 2 (Add-DbcString $effectNameDbc.Strings "Spells\Evolutions\GI_$($look.Key).mdx")
        $effectNameDbc.NewRecords.AddRange($record)

        $kitFields = @{ 15 = 0; 16 = 0 }
        foreach ($field in $EffectFields) { $kitFields[$field] = 0 }
        $kitFields[5] = [uint32]$effectNameDbc.MaxId
        $customVisualKits += @{ Key = "GI_$($look.Key)"; Clone = 12338; Fields = $kitFields }
        # A carried circle is an aura on the player carrying it: its buff tells them what to do (a shape that is no
        # danger, a fight's mark, says what it is: `description`)
        $description = if ($shape.description) { $shape.description }
            elseif ($shape.carried) { 'Vous portez une zone de danger : éloignez-vous des autres joueurs.' }
            else { 'Ne restez pas dans la zone rouge.' }
        # ... and is named for it too (`title`: a fight's mark worn by a player, Vorhan's inmate numbers)
        $title = if ($shape.title) { $shape.title } else { 'Zone de danger' }
        $customSpells += @{ Id = $look.Spell; Clone = 62898; Name = $title; Cost = 0; Cooldown = 0; Level = 0
            Spellbook = $false; Description = $description; AuraDescription = $description
            Visual = @{ Clone = 13273; State = "GI_$($look.Key)" } }
    }
}

$kitIdsByKey = @{}
foreach ($kit in $customVisualKits) {
    $fields = @{}
    foreach ($field in $kit.Fields.Keys) { $fields[$field] = $kit.Fields[$field] }
    if ($kit.Key -like 'OB_*') {
        foreach ($field in $EffectFields) {
            $effectId = Get-KitField $kit $field
            if ($effectId) { $fields[$field] = $blueEffectIds[$effectId] }
        }
    }
    if ($kit.Sound) { $fields[15] = $soundIdsByKey[$kit.Sound] }
    $kitIdsByKey[$kit.Key] = Add-DbcRecordCopy $visualKitDbc $kit.Clone $fields
}
$visualIdsBySpell = @{}
foreach ($custom in $customSpells) {
    if (-not $custom.Visual) { continue }
    $slotFields = @{}
    foreach ($slot in $custom.Visual.Keys) {
        if ($slot -eq 'Clone') { continue }
        if (-not $visualKitSlots.ContainsKey($slot)) { throw "Unknown visual kit slot '$slot' on $($custom.Name)." }
        $kit = $custom.Visual[$slot]
        $slotFields[$visualKitSlots[$slot]] = if ($kit -is [string]) { $kitIdsByKey[$kit] } else { $kit }
    }
    # A plain stock visual is used as is; kit swaps get their own record
    $visualIdsBySpell[[int]$custom.Id] = if ($slotFields.Count -eq 0) {
        if (-not $visualDbc.Offsets.ContainsKey([int]$custom.Visual.Clone)) {
            throw "SpellVisual $($custom.Visual.Clone) was not found."
        }
        [uint32]$custom.Visual.Clone
    }
    else {
        Add-DbcRecordCopy $visualDbc $custom.Visual.Clone $slotFields
    }
}

# --- Spell visuals imported from the Ascension client (localTools\ascensionImport\importVisuals.py) ---
#
# Each modules\<module>\client-assets\imported\visuals.json holds whole rows, already numbered (from the import's
# idBase) and pointing at one another, so they are appended as they are: a spell names its look by the id the import
# gave it (Spell.dbc field 131). The files they play ship from that folder's files\ (patchFiles.js). The sounds take
# the same combat volume as the game's own (CombatSoundVolume above).
$attachDbc = Read-Dbc $attachBackupPath 'SpellVisualKitModelAttach.dbc' 10
$motionDbc = Read-StringDbc $motionBackupPath 'SpellMissileMotion.dbc' 5
$importedTables = [ordered]@{
    SoundEntries = $soundDbc; SpellVisualEffectName = $effectNameDbc; SpellMissileMotion = $motionDbc
    SpellVisualKit = $visualKitDbc; SpellVisualKitModelAttach = $attachDbc; SpellVisual = $visualDbc
}
foreach ($importFile in @(Get-ChildItem -Path (Join-Path $repoRoot 'modules\*\client-assets\imported\visuals.json'))) {
    $import = Get-Content -LiteralPath $importFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($table in $importedTables.Keys) {
        $dbc = $importedTables[$table]
        foreach ($row in @($import.tables.$table)) {
            if (-not $row) { continue }
            if ($dbc.Offsets.ContainsKey([int]$row.id)) { throw "$table already has a row $($row.id) ($($importFile.FullName))." }
            $record = [byte[]]::new($dbc.RecordSize)
            for ($field = 0; $field -lt $row.fields.Count; ++$field) {
                Set-Field $record $field ([uint32]$row.fields[$field])
            }
            foreach ($property in $row.strings.PSObject.Properties) {
                Set-Field $record ([int]$property.Name) (Add-DbcString $dbc.Strings $property.Value)
            }
            if ($table -eq 'SoundEntries' -and $combatSoundTypes -contains [int64]$row.fields[1]) {
                $volume = [BitConverter]::ToSingle([BitConverter]::GetBytes([uint32]$row.fields[24]), 0)
                Set-Field $record 24 (Get-FloatBits ($volume * $CombatSoundVolume))
            }
            $dbc.Offsets[[int]$row.id] = -1
            $dbc.NewRecords.AddRange($record)
        }
    }
}

# --- SpellIcon.dbc: existing icons by path, new records added on demand ---
$iconSource = [IO.File]::ReadAllBytes($iconBackupPath)
Assert-Wdbc $iconSource 'SpellIcon.dbc'
$iconCount = [BitConverter]::ToInt32($iconSource, 4)
$iconRecordSize = [BitConverter]::ToInt32($iconSource, 12)
$iconStringSize = [BitConverter]::ToInt32($iconSource, 16)
if ([BitConverter]::ToInt32($iconSource, 8) -ne 2 -or $iconRecordSize -ne 8) { throw 'Unexpected SpellIcon.dbc layout.' }
$iconRecordsSize = $iconCount * $iconRecordSize
$iconStringOffset = 20 + $iconRecordsSize
$iconStrings = [Collections.Generic.List[byte]]::new()
$iconStrings.AddRange([byte[]]$iconSource[$iconStringOffset..($iconStringOffset + $iconStringSize - 1)])
$iconIdsByPath = [Collections.Generic.Dictionary[string, uint32]]::new([StringComparer]::OrdinalIgnoreCase)
$script:maxIconId = 0
for ($index = 0; $index -lt $iconCount; ++$index) {
    $iconId = [BitConverter]::ToUInt32($iconSource, 20 + $index * $iconRecordSize)
    $pathOffset = [BitConverter]::ToUInt32($iconSource, 20 + $index * $iconRecordSize + 4)
    $pathEnd = [Array]::IndexOf($iconSource, [byte]0, $iconStringOffset + $pathOffset)
    $path = [Text.Encoding]::UTF8.GetString($iconSource, $iconStringOffset + $pathOffset, $pathEnd - $iconStringOffset - $pathOffset)
    if (-not $iconIdsByPath.ContainsKey($path)) { $iconIdsByPath[$path] = $iconId }
    if ($iconId -gt $script:maxIconId) { $script:maxIconId = $iconId }
}
$newIconRecords = [Collections.Generic.List[byte]]::new()

function Get-IconIdForPath([string]$path) {
    if ($iconIdsByPath.ContainsKey($path)) { return $iconIdsByPath[$path] }
    $script:maxIconId = $script:maxIconId + 1
    $record = [byte[]]::new($iconRecordSize)
    Set-Field $record 0 ([uint32]$script:maxIconId)
    Set-Field $record 1 (Add-DbcString $iconStrings $path)
    $newIconRecords.AddRange($record)
    $iconIdsByPath[$path] = [uint32]$script:maxIconId
    return [uint32]$script:maxIconId
}

# Explicit IconPath, else the generated icon when compiled, else the fallback icon id
function Resolve-IconId($spec, [uint32]$fallbackIconId) {
    if ($spec.IconPath) { return Get-IconIdForPath $spec.IconPath }
    if ($spec.Icon) {
        foreach ($iconRoot in @($compiledIconRoot, $pestifereCompiledIconRoot, $necromancerCompiledIconRoot,
            $warriorIconRoot)) {
            if (Test-Path -LiteralPath (Join-Path $iconRoot "$($spec.Icon).tga")) {
                return Get-IconIdForPath "Interface\Icons\$($spec.Icon)"
            }
        }
    }
    return $fallbackIconId
}

# --- Spell.dbc ---
$source = [IO.File]::ReadAllBytes($spellBackupPath)
Assert-Wdbc $source 'Spell.dbc'
$recordCount = [BitConverter]::ToInt32($source, 4)
$fieldCount = [BitConverter]::ToInt32($source, 8)
$recordSize = [BitConverter]::ToInt32($source, 12)
$stringSize = [BitConverter]::ToInt32($source, 16)
if ($fieldCount -lt 234 -or $recordSize -lt 936) { throw 'Unexpected Spell.dbc layout.' }
$recordBlockSize = $recordCount * $recordSize
$stringOffset = 20 + $recordBlockSize
$records = [byte[]]::new($recordBlockSize)
[Array]::Copy($source, 20, $records, 0, $records.Length)
$strings = [Collections.Generic.List[byte]]::new()
$strings.AddRange([byte[]]$source[$stringOffset..($stringOffset + $stringSize - 1)])

function Get-SourceString([uint32]$offset) {
    $start = $stringOffset + $offset
    $end = [Array]::IndexOf($source, [byte]0, $start)
    return [Text.Encoding]::UTF8.GetString($source, $start, $end - $start)
}

$sinisterDescription = Add-DbcString $strings 'Strike for weapon damage, generating a combo point. Combat: costs no Energy and generates 10 Energy (12 from level 20), with a 25% chance to grant Opening (35% from level 50), and always grants it on a critical strike.'
$eviscerateAppend = ' Combat: deals 10% more damage, plus 5% per combo point. Grants Battle Tempo: 3% attack speed per combo point for 10 sec. Consumes up to 30 extra Energy to deal up to 50% more damage. From level 25, grants Opening at 5 combo points.'
$sliceAndDiceAppend = ' Combat: consumes up to 30 extra Energy to last up to 50% longer.'
$appendedDescriptions = @{}
$foundSinister = [Collections.Generic.HashSet[uint32]]::new()
$foundAppended = [Collections.Generic.HashSet[uint32]]::new()
$foundTalentRanks = [Collections.Generic.HashSet[uint32]]::new()
# Victory Rush works like retail: no killing blow ("Victorious" caster aura state) or stance requirement,
# a 30 sec cooldown, and the module heals for 20% of maximum health on hit
$victoryRushId = 34428
$victoryRushDescription = Add-DbcString $strings 'Instantly attack the target causing ${$AP*$m1/100} damage and healing you for 20% of your maximum health.  Damage is based on your attack power.'
$foundVictoryRush = $false
$cloneRecords = @{}
$iconIdsBySpell = @{}
$fallbackIconSpells = [Collections.Generic.HashSet[int]]::new()
foreach ($custom in $customSpells) { if ($custom.FallbackIconSpell) { [void]$fallbackIconSpells.Add([int]$custom.FallbackIconSpell) } }
# The custom spells by the stock spell they copy: looked up once per record (scanning every custom spell for every one
# of the ~50,000 records was 110 million comparisons, two minutes of every client build)
$customSpellsByClone = @{}
foreach ($custom in $customSpells) {
    $cloneId = [int]$custom.Clone
    if (-not $customSpellsByClone.ContainsKey($cloneId)) { $customSpellsByClone[$cloneId] = [Collections.Generic.List[object]]::new() }
    $customSpellsByClone[$cloneId].Add($custom)
}

# Name, rank, description and aura text: an empty locale reads the English one
[DbcFast]::FillLocales($records, $recordCount, $recordSize, [int[]]@(136, 153, 170, 187))
for ($index = 0; $index -lt $recordCount; ++$index) {
    $offset = $index * $recordSize
    $spellId = [BitConverter]::ToUInt32($records, $offset)
    if ($fallbackIconSpells.Contains([int]$spellId)) {
        $iconIdsBySpell[[int]$spellId] = Read-Field $records $offset $F_SpellIconID
    }
    if ((Read-Field $records $offset 208) -eq 3) {
        for ($slot = 0; $slot -lt 8; ++$slot) {
            if ($mageFreeReagents -contains [int](Read-Field $records $offset (52 + $slot))) {
                Write-Field $records $offset (52 + $slot) 0
                Write-Field $records $offset (60 + $slot) 0
            }
        }
    }
    $stockRangeRemap = $stockRangeRemaps[[int](Read-Field $records $offset 208)]
    if ($stockRangeRemap -and $stockSkillSpells.Contains([uint32]$spellId)) {
        $stockNewRange = $stockRangeRemap[[int](Read-Field $records $offset 46)]
        if ($stockNewRange) {
            Write-Field $records $offset 46 ([long]$stockNewRange)
            ++$stockRangeRemapCount
        }
    }
    $stockEdit = $stockEditsById[[int]$spellId]
    if (-not $stockEdit) {
        $stockFamily = [int](Read-Field $records $offset 208)
        if ($stockEditFamilies.Contains($stockFamily) -and $stockSkillSpells.Contains([uint32]$spellId) -and
            ((Read-Field $records $offset 204) -ne 0 -or (Read-Field $records $offset $F_ManaCost) -ne 0)) {
            $stockEdit = $stockEditsByName["$stockFamily|$(Get-SourceString (Read-Field $records $offset $F_Name))"]
        }
    }
    if ($stockEdit) {
        if ($stockEdit.Effects) { Write-Effects $records $offset $stockEdit.Effects }
        foreach ($field in $stockEdit.Fields.Keys) {
            Write-Field $records $offset ([int]$field) ([long]$stockEdit.Fields[$field])
        }
        foreach ($text in @(@($F_Description, $stockEdit.Description), @($F_AuraDescription, $stockEdit.AuraDescription))) {
            if ($null -eq $text[1]) { continue }
            if (-not $text[1]) { Write-LocalizedString $records $offset $text[0] 0; continue }
            if (-not $stockEditTexts.ContainsKey($text[1])) { $stockEditTexts[$text[1]] = Add-DbcString $strings $text[1] }
            Write-LocalizedString $records $offset $text[0] $stockEditTexts[$text[1]]
        }
        ++$stockEditCount
    }
    if ($sinisterStrikeRanks -contains $spellId) {
        Write-Field $records $offset $F_ManaCost 0
        Write-LocalizedString $records $offset $F_Description $sinisterDescription
        [void]$foundSinister.Add($spellId)
    }
    if (($eviscerateRanks -contains $spellId) -or ($sliceAndDiceRanks -contains $spellId)) {
        $original = Get-SourceString (Read-Field $records $offset $F_Description)
        $text = $original + $(if ($eviscerateRanks -contains $spellId) { $eviscerateAppend } else { $sliceAndDiceAppend })
        if (-not $appendedDescriptions.ContainsKey($text)) { $appendedDescriptions[$text] = Add-DbcString $strings $text }
        Write-LocalizedString $records $offset $F_Description $appendedDescriptions[$text]
        [void]$foundAppended.Add($spellId)
    }
    if ($spellId -eq $victoryRushId) {
        Write-Field $records $offset 12 0
        Write-Field $records $offset $F_CasterAuraState 0
        # 30 sec cooldown
        Write-Field $records $offset 29 30000
        Write-LocalizedString $records $offset $F_Description $victoryRushDescription
        $foundVictoryRush = $true
    }
    if ($talentRankInfo.ContainsKey([int]$spellId)) {
        $talent = $talentRankInfo[[int]$spellId].Talent
        $rank = [int]$talentRankInfo[[int]$spellId].Rank

        # Passive, permanent, self-only talent aura without any of the WotLK behaviour
        Write-Field $records $offset $F_Attributes 0x140
        [DbcFast]::Clear($records, $offset, 5, 36)
        Write-Field $records $offset 28 1
        Write-Field $records $offset 37 0
        Write-Field $records $offset $F_DurationIndex 21
        [DbcFast]::Clear($records, $offset, 41, 45)
        Write-Field $records $offset 46 1
        Write-Field $records $offset $F_StackAmount 0
        [DbcFast]::Clear($records, $offset, 50, 67)
        Write-Field $records $offset $F_EquippedItemClass -1
        Write-Field $records $offset 69 0
        Write-Field $records $offset 70 0
        $effects = @()
        if ($talent.Auras) {
            for ($auraIndex = 0; $auraIndex -lt $talent.Auras.Count; ++$auraIndex) {
                $aura = $talent.Auras[$auraIndex]
                $effects += @{ Index = $auraIndex; Aura = $aura.Aura; Value = $aura.Values[$rank]; Misc = $aura.Misc }
            }
        }
        else {
            $effects += @{ Index = 0; Aura = $A_Dummy }
        }
        Write-Effects $records $offset $effects
        # No spell visual: a permanent passive would otherwise show the old cooldown glow all the time
        Write-Field $records $offset $F_SpellVisual 0
        Write-Field $records $offset ($F_SpellVisual + 1) 0
        [DbcFast]::Clear($records, $offset, 205, 214)
        Write-Field $records $offset $F_SpellIconID (Resolve-IconId $talent (Read-Field $records $offset $F_SpellIconID))

        $arguments = [object[]]@(
            $(if ($talent.V0) { $talent.V0[$rank] } else { '' }),
            $(if ($talent.V1) { $talent.V1[$rank] } else { '' })
        )
        Write-LocalizedString $records $offset $F_Name (Add-DbcString $strings $talent.Name)
        Write-LocalizedString $records $offset $F_Description (Add-DbcString $strings ([string]::Format($talent.Description, $arguments)))
        Write-LocalizedString $records $offset $F_AuraDescription 0
        [void]$foundTalentRanks.Add($spellId)
    }
    $copies = $customSpellsByClone[[int]$spellId]
    if ($copies) {
        foreach ($custom in $copies) {
            $clone = [byte[]]::new($recordSize)
            [Array]::Copy($records, $offset, $clone, 0, $recordSize)
            $cloneRecords[[int]$custom.Id] = $clone
        }
    }
}

if ($foundSinister.Count -ne $sinisterStrikeRanks.Count) { throw 'Not all Sinister Strike ranks were found.' }
if ($foundAppended.Count -ne ($eviscerateRanks.Count + $sliceAndDiceRanks.Count)) { throw 'Not all Eviscerate and Slice and Dice ranks were found.' }
if (-not $foundVictoryRush) { throw 'Victory Rush was not found.' }
if ($foundTalentRanks.Count -ne $talentRankInfo.Count) { throw "Only $($foundTalentRanks.Count) of $($talentRankInfo.Count) Combat talent ranks were found." }
if ($cloneRecords.Count -ne $customSpells.Count) { throw 'Not all custom clone spells were found.' }

$customRecordBytes = [Collections.Generic.List[byte]]::new()
foreach ($custom in $customSpells) {
    $record = $cloneRecords[[int]$custom.Id]
    $cloneIconId = [BitConverter]::ToUInt32($record, $F_SpellIconID * 4)
    Set-Field $record 0 ([uint32]$custom.Id)
    Set-Field $record 29 ([uint32]$custom.Cooldown)
    Set-Field $record 30 0
    Set-Field $record 37 0
    Set-Field $record 38 ([uint32]$custom.Level)
    Set-Field $record 39 ([uint32]$custom.Level)
    Set-Field $record 42 ([uint32]$custom.Cost)
    Set-Field $record 43 0
    Set-Field $record 44 0
    Set-Field $record 45 0
    if ($custom.NoEquipment) {
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
    }
    if ($custom.DummyAura) {
        Set-Field $record 34 0
        Set-Field $record 35 0
        Set-Field $record 36 0
        Set-Field $record 41 3
        Set-Field $record 49 ([uint32]$(if ($custom.MaxStacks) { $custom.MaxStacks } else { 1 }))
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        [DbcFast]::Clear($record, 0, 71, 130)
        Set-Field $record 71 6
        Set-Field $record 86 1
        Set-Field $record 95 4
        Set-Field $record 131 0
        Set-Field $record 132 0
        Set-Field $record 205 133
        Set-Field $record 206 1000
    }
    if ($custom.BleedAura) {
        Set-Field $record 40 32
        Set-Field $record 41 3
        Set-Field $record 49 1
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        [DbcFast]::Clear($record, 0, 71, 130)
        Set-Field $record 71 6
        Set-Field $record 80 0
        Set-Field $record 86 6
        Set-Field $record 95 3
        Set-Field $record 98 2000
        Set-Field $record 208 0
        Set-Field $record 209 0
        Set-Field $record 210 0
        Set-Field $record 211 0
    }
    if ($custom.TravelCast) {
        Set-Field $record 28 14
        Set-Field $record 40 0
        Set-Field $record 41 0
        Set-Field $record 49 0
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        [DbcFast]::Clear($record, 0, 71, 130)
        Set-Field $record 71 3
        Set-Field $record 86 1
    }
    if ($custom.ShadowLungeDamage) {
        Set-Field $record 73 31
        Set-Field $record 76 0
        Set-Field $record 79 0
        Set-Field $record 82 149
        Set-Field $record 85 0
        Set-Field $record 88 6
        Set-Field $record 91 0
        Set-Field $record 94 0
        Set-Field $record 97 0
        Set-Field $record 100 0
        Set-Field $record 103 0
        Set-Field $record 106 0
        Set-Field $record 109 0
        Set-Field $record 112 0
        Set-Field $record 115 0
        Set-Field $record 118 0
        Set-Field $record 121 0
        [DbcFast]::Clear($record, 0, 128, 130)
        Set-Field $record 213 2
        Set-Field $record 225 1
    }
    if ($custom.GladiatorStance) {
        # Permanent self aura usable only in Defensive Stance: +100% damage done and taken, all schools.
        # The shield requirement is enforced by the module script: an item requirement here would stop
        # the damage done bonus from applying to weapon attacks.
        Set-Field $record 1 0
        Set-Field $record 4 0x10
        [DbcFast]::Clear($record, 0, 5, 11)
        Set-Field $record 12 131072
        [DbcFast]::Clear($record, 0, 13, 27)
        Set-Field $record 28 1
        [DbcFast]::Clear($record, 0, 31, 36)
        Set-Field $record 40 21
        Set-Field $record 41 1
        Set-Field $record 46 1
        Set-Field $record 49 0
        [DbcFast]::Clear($record, 0, 50, 67)
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        [DbcFast]::Clear($record, 0, 71, 130)
        Set-Field $record 71 6
        Set-Field $record 72 6
        Set-Field $record 74 1
        Set-Field $record 75 1
        Set-Field $record 80 99
        Set-Field $record 81 99
        Set-Field $record 86 1
        Set-Field $record 87 1
        Set-Field $record 95 79
        Set-Field $record 96 87
        Set-Field $record 110 127
        Set-Field $record 111 127
        Set-Field $record 134 0
        Set-Field $record 204 0
        Set-Field $record 205 133
        Set-Field $record 206 1500
        Set-Field $record 208 4
        [DbcFast]::Clear($record, 0, 209, 214)
        Set-Field $record 225 1
    }
    if ($custom.CantCancel) {
        # SPELL_ATTR0_NO_AURA_CANCEL: the carried plagues cannot be right-clicked away. Shedding one is meant to
        # cost something (Purge cathartique, or letting it lapse out of combat), never to be free.
        $attributes = [BitConverter]::ToUInt32($record, 4 * 4)
        Set-Field $record 4 ([uint32]($attributes -bor [uint32]2147483648))
    }
    if ($custom.TalentAura) {
        # A learned talent rank: passive, hidden, permanent, self only, no cost, no cast, no visual.
        # Everything the clone carried is stripped so only the effects the talent declares remain.
        # PASSIVE | HIDDEN_CLIENTSIDE | HIDE_IN_COMBAT_LOG: without HIDDEN_CLIENTSIDE every learned rank shows up
        # in the spellbook's General tab as a "Passive" entry
        Set-Field $record 4 0x1c0
        [DbcFast]::Clear($record, 0, 5, 27)
        Set-Field $record 28 1
        [DbcFast]::Clear($record, 0, 29, 39)
        Set-Field $record 40 21
        [DbcFast]::Clear($record, 0, 41, 45)
        Set-Field $record 46 1
        [DbcFast]::Clear($record, 0, 47, 70)
        Set-Field $record 68 ([uint32]::MaxValue)
        [DbcFast]::Clear($record, 0, 71, 132)
        Set-Field $record 134 0
        [DbcFast]::Clear($record, 0, 204, 233)
        Set-Field $record 225 1
    }
    if ($custom.Effects) {
        Write-Effects $record 0 $custom.Effects
    }
    if ($custom.Fields) {
        foreach ($field in $custom.Fields.Keys) {
            Write-Field $record 0 ([int]$field) ([long]$custom.Fields[$field])
        }
    }
    # Necromancer spells clone visuals/cast layouts from several classes, including Death Knight. Strip every
    # inherited rune, stance, weapon and spell-family requirement centrally: the class is a mana caster and all
    # its spells belong exclusively to family 18.
    if ([int]$custom.Id -ge 90400 -and [int]$custom.Id -le 90599) {
        Set-Field $record 12 0
        Set-Field $record 13 0
        Set-Field $record 41 0
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        Set-Field $record 208 18
        Set-Field $record 209 0
        Set-Field $record 210 0
        Set-Field $record 211 0
        Set-Field $record 226 0
    }
    # The Oathblade: 90800-90999, and 91000-91199 for its talent trees (localTools\oathblade\talentTree.json)
    if ([int]$custom.Id -ge 90800 -and [int]$custom.Id -le 91199) {
        Set-Field $record 1 0
        Set-Field $record 12 0
        Set-Field $record 13 0
        Set-Field $record 41 3
        Set-Field $record 208 19
        Set-Field $record 209 0
        Set-Field $record 210 0
        Set-Field $record 211 0
        Set-Field $record 226 0
    }
    # The Pestiféré is a rage class built on Death Knight spells: without this every clone keeps its
    # source's rune cost and asks for Blood runes the class can never have.
    if ([int]$custom.Id -ge 90200 -and [int]$custom.Id -le 90399) {
        Set-Field $record 41 ([uint32]$POWER_RAGE)
        Set-Field $record 226 0
        # Warrior clones carry their stance requirement: the class has no stances to be in
        Set-Field $record 12 0
        Set-Field $record 13 0
        # Its own spell family: the clone's Warrior or Death Knight family and flags would let those classes'
        # talents and scripts reach it
        Set-Field $record 208 ([uint32]$FAM_PESTIFERE)
        $familyFlags = if ($pestifereFamilyFlags.ContainsKey([int]$custom.Id)) { $pestifereFamilyFlags[[int]$custom.Id] } else { 0 }
        Set-Field $record 209 ([uint32]$familyFlags)
        Set-Field $record 210 0
        Set-Field $record 211 0
        # Only modifiers name other spells, in their own Fields: a mask inherited from the clone is cleared
        $setsMask = $custom.Fields -and @($custom.Fields.Keys | Where-Object { [int]$_ -ge 122 -and [int]$_ -le 130 }).Count
        if (-not $custom.TalentAura -and -not $setsMask) {
            [DbcFast]::Clear($record, 0, 122, 130)
        }
    }
    if ($visualIdsBySpell.ContainsKey([int]$custom.Id)) {
        Set-Field $record $F_SpellVisual $visualIdsBySpell[[int]$custom.Id]
        Set-Field $record ($F_SpellVisual + 1) 0
    }
    $fallbackIconId = if ($custom.FallbackIconSpell) { $iconIdsBySpell[[int]$custom.FallbackIconSpell] } else { $cloneIconId }
    Set-Field $record $F_SpellIconID (Resolve-IconId $custom ([uint32]$fallbackIconId))
    $nameOffset = Add-DbcString $strings $custom.Name
    $descriptionOffset = Add-DbcString $strings $custom.Description
    $auraDescriptionOffset = if ($custom.AuraDescription) { Add-DbcString $strings $custom.AuraDescription } else { [uint32]0 }
    [DbcFast]::SetLocalized($record, 0, 136, $nameOffset)
    [DbcFast]::SetLocalized($record, 0, 153, 0)
    [DbcFast]::SetLocalized($record, 0, 170, $descriptionOffset)
    [DbcFast]::SetLocalized($record, 0, 187, $auraDescriptionOffset)
    $customRecordBytes.AddRange($record)
}

$newCount = $recordCount + $customSpells.Count
$spellOutput = [byte[]]::new(20 + ($newCount * $recordSize) + $strings.Count)
[Array]::Copy($source, 0, $spellOutput, 0, 20)
[BitConverter]::GetBytes([uint32]$newCount).CopyTo($spellOutput, 4)
[BitConverter]::GetBytes([uint32]$strings.Count).CopyTo($spellOutput, 16)
[Array]::Copy($records, 0, $spellOutput, 20, $records.Length)
$customRecordBytes.CopyTo($spellOutput, 20 + $records.Length)
$strings.CopyTo($spellOutput, 20 + ($newCount * $recordSize))

# --- SpellIcon.dbc output ---
$newIconCount = $iconCount + $newIconRecords.Count / $iconRecordSize
$iconOutput = [byte[]]::new(20 + ($newIconCount * $iconRecordSize) + $iconStrings.Count)
[Array]::Copy($iconSource, 0, $iconOutput, 0, 20 + $iconRecordsSize)
[BitConverter]::GetBytes([uint32]$newIconCount).CopyTo($iconOutput, 4)
[BitConverter]::GetBytes([uint32]$iconStrings.Count).CopyTo($iconOutput, 16)
$newIconRecords.CopyTo($iconOutput, 20 + $iconRecordsSize)
$iconStrings.CopyTo($iconOutput, 20 + ($newIconCount * $iconRecordSize))

# --- SkillLineAbility.dbc: spellbook entries ---
$skillSource = [IO.File]::ReadAllBytes($skillBackupPath)
Assert-Wdbc $skillSource 'SkillLineAbility.dbc'
$skillCount = [BitConverter]::ToInt32($skillSource, 4)
$skillFields = [BitConverter]::ToInt32($skillSource, 8)
$skillRecordSize = [BitConverter]::ToInt32($skillSource, 12)
$skillStringSize = [BitConverter]::ToInt32($skillSource, 16)
if ($skillFields -ne 14 -or $skillRecordSize -ne 56) { throw 'Unexpected SkillLineAbility.dbc layout.' }
$skillRecordsSize = $skillCount * $skillRecordSize
$skillStringsOffset = 20 + $skillRecordsSize
$sinisterSkillRecord = $null
$sinisterSkillIndex = -1
$maxSkillId = 0
for ($index = 0; $index -lt $skillCount; ++$index) {
    $offset = 20 + $index * $skillRecordSize
    $rowId = [BitConverter]::ToUInt32($skillSource, $offset)
    if ($rowId -gt $maxSkillId) { $maxSkillId = $rowId }
    if ([BitConverter]::ToUInt32($skillSource, $offset + 8) -eq 1752) {
        $sinisterSkillRecord = [byte[]]::new($skillRecordSize)
        [Array]::Copy($skillSource, $offset, $sinisterSkillRecord, 0, $skillRecordSize)
        $sinisterSkillIndex = $index
    }
}
if (-not $sinisterSkillRecord) { throw 'Sinister Strike SkillLineAbility row was not found.' }

$newSkillCount = $skillCount + $spellbookSpells.Count
$skillOutput = [byte[]]::new(20 + ($newSkillCount * $skillRecordSize) + $skillStringSize)
[Array]::Copy($skillSource, 0, $skillOutput, 0, 20)
[BitConverter]::GetBytes([uint32]$newSkillCount).CopyTo($skillOutput, 4)
$customSkillRecords = [Collections.Generic.List[byte]]::new()
for ($index = 0; $index -lt $spellbookSpells.Count; ++$index) {
    $record = [byte[]]::new($skillRecordSize)
    [Array]::Copy($sinisterSkillRecord, 0, $record, 0, $skillRecordSize)
    Set-Field $record 0 ([uint32]($maxSkillId + $index + 1))
    Set-Field $record 2 ([uint32]$spellbookSpells[$index].Id)
    Set-Field $record 8 0
    # AcquireMethod 2 teaches a spell with its whole skill line - and a custom class carries its own skill line
    # from level 1, so the core handed out the entire kit at creation (a level 1 Nécromancien opened with every
    # spell it would ever learn, its level 60 army included). 0 is what Blizzard's class abilities use: the
    # spell is taught by something else - the class module at the level its AbilityUnlocks names, or a talent
    # rank. The stock classes' skill lines did the same: every rogue and mage was handed the whole Crimson Duelist
    # kit and every ability of its talent trees, as temporary spells, at each login and level. Every custom spell
    # is taught that way; a row that really wants the skill line to teach it says so with AutoLearn = $true.
    if (-not $spellbookSpells[$index].AutoLearn) {
        Set-Field $record 9 0
    } else {
        Set-Field $record 9 2
    }
    if ($spellbookSpells[$index].SkillLine) {
        Set-Field $record 1 ([uint32]$spellbookSpells[$index].SkillLine)
        Set-Field $record 3 0
        Set-Field $record 4 ([uint32]$spellbookSpells[$index].ClassMask)
    }
    $customSkillRecords.AddRange($record)
}

$destinationOffset = 20
for ($index = 0; $index -lt $skillCount; ++$index) {
    [Array]::Copy($skillSource, 20 + $index * $skillRecordSize, $skillOutput, $destinationOffset, $skillRecordSize)
    $destinationOffset += $skillRecordSize
    if ($index -eq $sinisterSkillIndex) {
        $customSkillRecords.CopyTo($skillOutput, $destinationOffset)
        $destinationOffset += $customSkillRecords.Count
    }
}
[Array]::Copy($skillSource, $skillStringsOffset, $skillOutput, 20 + ($newSkillCount * $skillRecordSize), $skillStringSize)

New-Item -ItemType Directory -Path $clientDbcRoot -Force | Out-Null
[IO.File]::WriteAllBytes($serverSpellPath, $spellOutput)
[IO.File]::WriteAllBytes($clientSpellPath, $spellOutput)
[IO.File]::WriteAllBytes($serverSkillPath, $skillOutput)
[IO.File]::WriteAllBytes($clientSkillPath, $skillOutput)
[IO.File]::WriteAllBytes($serverIconPath, $iconOutput)
[IO.File]::WriteAllBytes($clientIconPath, $iconOutput)
$visualOutput = Get-DbcOutput $visualDbc
$visualKitOutput = Get-DbcOutput $visualKitDbc
$soundOutput = Get-StringDbcOutput $soundDbc
[IO.File]::WriteAllBytes($serverVisualPath, $visualOutput)
[IO.File]::WriteAllBytes($clientVisualPath, $visualOutput)
[IO.File]::WriteAllBytes($serverVisualKitPath, $visualKitOutput)
[IO.File]::WriteAllBytes($clientVisualKitPath, $visualKitOutput)
[IO.File]::WriteAllBytes($serverSoundPath, $soundOutput)
[IO.File]::WriteAllBytes($clientSoundPath, $soundOutput)
$effectNameOutput = Get-StringDbcOutput $effectNameDbc
[IO.File]::WriteAllBytes($serverEffectNamePath, $effectNameOutput)
[IO.File]::WriteAllBytes($clientEffectNamePath, $effectNameOutput)
$attachOutput = Get-DbcOutput $attachDbc
[IO.File]::WriteAllBytes($serverAttachPath, $attachOutput)
[IO.File]::WriteAllBytes($clientAttachPath, $attachOutput)
$motionOutput = Get-StringDbcOutput $motionDbc
[IO.File]::WriteAllBytes($serverMotionPath, $motionOutput)
[IO.File]::WriteAllBytes($clientMotionPath, $motionOutput)

# --- Paragon glyph icons (Item.dbc) --------------------------------------------------------------------------
#
# The 30 paragon glyphs (mod-stat-growth ParagonSystem.cpp) are items 13642-13671: rows the client's Item.dbc already
# had, empty "ornate box" rows of class 15, with no template on the server. An item's icon comes from this row's
# display id, so each gets an inscription glyph's (localTools\paragon\glyphItemDisplays.json, written by
# buildParagonTree.py). Rebuilt from the untouched backup like the rest; the server reads the same file.
$serverItemPath = Join-Path $serverDbcRoot 'Item.dbc'
$itemBackupPath = Join-Path $serverDbcRoot 'Item.before-paragon-glyphs.dbc'
$clientItemPath = Join-Path $clientDbcRoot 'Item.dbc'
$glyphDisplayPath = Join-Path $repoRoot 'localTools\paragon\glyphItemDisplays.json'
if (-not (Test-Path -LiteralPath $itemBackupPath)) {
    Copy-Item -LiteralPath $serverItemPath -Destination $itemBackupPath
}
$itemBytes = [IO.File]::ReadAllBytes($itemBackupPath)
Assert-Wdbc $itemBytes 'Item.dbc'
$itemCount = [BitConverter]::ToInt32($itemBytes, 4)
$itemRecordSize = [BitConverter]::ToInt32($itemBytes, 12)
$itemOffsets = @{}
for ($index = 0; $index -lt $itemCount; ++$index) {
    $itemOffsets[[int][BitConverter]::ToUInt32($itemBytes, 20 + $index * $itemRecordSize)] = 20 + $index * $itemRecordSize
}
$glyphDisplays = Get-Content -LiteralPath $glyphDisplayPath -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($glyph in $glyphDisplays) {
    if (-not $itemOffsets.ContainsKey([int]$glyph.item)) {
        throw "Item.dbc has no row $($glyph.item) for the paragon glyph $($glyph.icon)."
    }
    # Field 5: DisplayInfoID
    [BitConverter]::GetBytes([uint32]$glyph.display).CopyTo($itemBytes, $itemOffsets[[int]$glyph.item] + 5 * 4)
}
# Le Front du Nord's currency, the Éclat de givre (item 37711, stat_growth_frontier.sql): its own display, built below
# on its stock row's, with the shard's icon (localTools\interface\buildFrontierArt.py)
$frontierShard = @{ Item = 37711; Display = 70100; CloneOf = 32278; Icon = 'INV_Frontier_FrostShard' }
if (-not $itemOffsets.ContainsKey($frontierShard.Item)) { throw "Item.dbc has no row $($frontierShard.Item) for the frost shard." }
[BitConverter]::GetBytes([uint32]$frontierShard.Display).CopyTo($itemBytes, $itemOffsets[$frontierShard.Item] + 5 * 4)
# The Faucheur's starting weapon, Faux usée (item 4901, mod-reaper's pending SQL): a two-hand polearm wearing the stock
# scythe model (display 33086). The server enforces this row over its item_template, so the look lives here too.
$reaperScythe = @{ Item = 4901; Display = 33086 }
if (-not $itemOffsets.ContainsKey($reaperScythe.Item)) { throw "Item.dbc has no row $($reaperScythe.Item) for the Faucheur's scythe." }
# ClassID 2 (weapon), SubclassID 6 (polearm), no sound override, Material 1 (metal), its display, InventoryType 17
# (two-hand), SheatheType 1 (on the back)
$reaperScytheValues = @(2, 6, -1, 1, $reaperScythe.Display, 17, 1)
for ($field = 1; $field -le 7; ++$field) {
    Write-Field $itemBytes $itemOffsets[$reaperScythe.Item] $field ([long]$reaperScytheValues[$field - 1])
}
# Marque de l'Inquisiteur (modules/mod-legendary), the first legendary: a cloak, item 24567 - a cloth cloak row the client
# had with no template on the server - wearing the Recovered Scarlet Onslaught Cape's Scarlet red: its own display (71000),
# that cape's (64326) with the legendary's icon (localTools\interfaceuildLegendaryClientArt.py)
$legendaryCloak = @{ Item = 24567; Display = 71000; CloneOf = 64326; Icon = 'INV_Legendary_MarqueInquisiteur' }
if (-not $itemOffsets.ContainsKey($legendaryCloak.Item)) { throw "Item.dbc has no row $($legendaryCloak.Item) for Marque de l'Inquisiteur." }
[BitConverter]::GetBytes([uint32]$legendaryCloak.Display).CopyTo($itemBytes, $itemOffsets[$legendaryCloak.Item] + 5 * 4)
# Consécration de Mograine, the gloves: item 21428, a hands row of the "misc" armour subclass (every class wears it;
# mod-legendary rolls its armour for the looter's own armour type), wearing Turalyon's red and gold gauntlets: its own
# display (71002), theirs (62062) with its icon. Serment de Whitemane, the ring (item 996): its own display (71001), its
# row's (3453) with its icon.
$legendaryGloves = @{ Item = 21428; Display = 71002; CloneOf = 62062; Icon = 'INV_Legendary_ConsecrationMograine' }
$legendaryRing = @{ Item = 996; Display = 71001; CloneOf = 3453; Icon = 'INV_Legendary_SermentWhitemane' }
if (-not $itemOffsets.ContainsKey($legendaryRing.Item)) { throw "Item.dbc has no row $($legendaryRing.Item) for Serment de Whitemane." }
[BitConverter]::GetBytes([uint32]$legendaryRing.Display).CopyTo($itemBytes, $itemOffsets[$legendaryRing.Item] + 5 * 4)
if (-not $itemOffsets.ContainsKey($legendaryGloves.Item)) { throw "Item.dbc has no row $($legendaryGloves.Item) for Consécration de Mograine." }
[BitConverter]::GetBytes([uint32]$legendaryGloves.Display).CopyTo($itemBytes, $itemOffsets[$legendaryGloves.Item] + 5 * 4)
# The other dungeons' legendaries (mod-legendary Definitions 4-24): Item.dbc rows with no template (most of the "misc"
# armour subclass: every class wears them), each wearing a look its dungeon's own bosses drop in that slot: its own
# display (71003 on), that look with the legendary's painted icon - or the look's own icon while it is not painted
# (modules/mod-stat-growth/client-assets/compiled/<Icon>.tga, localTools\interface\buildLegendaryClientArt.py).
$legendaryItems = @(
    @{ Item = 21424; Display = 71003; CloneOf = 43801; Icon = 'INV_Legendary_EpaulieresCapacitus' }     # Mechanar: Pauldrons of Surging Mana
    @{ Item = 1258;  Display = 71004; CloneOf = 41449; Icon = 'INV_Legendary_AbaquePathaleon' }         # Mechanar: Abacus of Violent Odds
    @{ Item = 21432; Display = 71005; CloneOf = 42847; Icon = 'INV_Legendary_BrassardsSepethrea' }      # Mechanar: Bracers of Recklessness
    @{ Item = 21425; Display = 71006; CloneOf = 52509; Icon = 'INV_Legendary_CeintureIngvar' }          # Utgarde: Girdle of the Howling Berserker
    @{ Item = 21420; Display = 71007; CloneOf = 49353; Icon = 'INV_Legendary_CuirasseKeleseth' }        # Utgarde: Chestplate of the Northern Lights
    @{ Item = 26541; Display = 71008; CloneOf = 9657;  Icon = 'INV_Legendary_CollierAnnhylde' }         # Utgarde: Winterfall's Frozen Necklace
    @{ Item = 21437; Display = 71009; CloneOf = 43052; Icon = 'INV_Legendary_PoignesKargath' }          # Shattered Halls: Gauntlets of Desolation
    @{ Item = 21433; Display = 71010; CloneOf = 42975; Icon = 'INV_Legendary_BandelettesNethekurse' }   # Shattered Halls: Bands of Nethekurse
    @{ Item = 5828;  Display = 71011; CloneOf = 28733; Icon = 'INV_Legendary_ChevalierePorung' }        # Shattered Halls: Band of Dominion
    @{ Item = 21421; Display = 71012; CloneOf = 9123;  Icon = 'INV_Legendary_PlastronVanCleef' }        # Deadmines: Blackened Defias Armor
    @{ Item = 21429; Display = 71013; CloneOf = 14389; Icon = 'INV_Legendary_PoudreGilnid' }            # Deadmines: Blackened Defias Belt
    @{ Item = 21444; Display = 71014; CloneOf = 16642; Icon = 'INV_Legendary_MouflesCookie' }           # Deadmines: Magefist Gloves
    @{ Item = 18161; Display = 71015; CloneOf = 50618; Icon = 'INV_Legendary_BottesDred' }              # Drak'Tharon: Scytheclaw Boots
    @{ Item = 21430; Display = 71016; CloneOf = 52216; Icon = 'INV_Legendary_RobeNovos' }               # Drak'Tharon: Robes of Novos
    @{ Item = 27218; Display = 71017; CloneOf = 35437; Icon = 'INV_Legendary_PendentifTharonja' }       # Drak'Tharon: Crystal Pendant of Warding
    @{ Item = 21423; Display = 71018; CloneOf = 64722; Icon = 'INV_Legendary_JambieresDevoreur' }       # Forge of Souls: Legplates of Frozen Granite
    @{ Item = 21434; Display = 71019; CloneOf = 52304; Icon = 'INV_Legendary_HeaumeBronjahm' }          # Forge of Souls: Skullcage of Eternal Terror
    @{ Item = 6673;  Display = 71020; CloneOf = 33534; Icon = 'INV_Legendary_AnneauAmeRefletee' }       # Forge of Souls: Spiteful Signet
    @{ Item = 8688;  Display = 71021; CloneOf = 55033; Icon = 'INV_Legendary_EtincelleIonar' }          # Halls of Lightning: Tears of Bitter Anguish
    @{ Item = 21450; Display = 71022; CloneOf = 51932; Icon = 'INV_Legendary_PoingsLoken' }             # Halls of Lightning: Fists of Loken
    @{ Item = 6674;  Display = 71023; CloneOf = 31905; Icon = 'INV_Legendary_ChevaliereBjarngrim' }     # Halls of Lightning: Bjarngrim Family Signet
    @{ Item = 10555; Display = 71024; CloneOf = 33534; Icon = 'INV_Unique_EchoDuNeant' }                # The Hollow Voice's Unique: a ring
    # L'Infini's: a ring's free row made a trinket (no free trinket row is left), Dark Matter's look
    @{ Item = 16067; Display = 71025; CloneOf = 59323; Icon = 'INV_Legendary_EtoileCaptive'; InventoryType = 12 }
    # Gardien-chef Vorhan's sets (mod-legendary 101-135): free rows made each piece's class, armour and slot, wearing
    # the tier set looks they are recoloured from (Onslaught, Demon Stalker, Netherblade, Voidheart and their tier 6
    # wrists, waists and feet) until their own
    @{ Item = 13710; Display = 71026; CloneOf = 49684; Icon = 'INV_Vorhan_Plate_Head'; Class = 4; Subclass = 4; Material = 1; InventoryType = 1 }
    @{ Item = 13711; Display = 71027; CloneOf = 45661; Icon = 'INV_Vorhan_Plate_Shoulders'; Class = 4; Subclass = 4; Material = 1; InventoryType = 3 }
    @{ Item = 13712; Display = 71028; CloneOf = 45658; Icon = 'INV_Vorhan_Plate_Chest'; Class = 4; Subclass = 4; Material = 1; InventoryType = 5 }
    @{ Item = 13713; Display = 71029; CloneOf = 45659; Icon = 'INV_Vorhan_Plate_Hands'; Class = 4; Subclass = 4; Material = 1; InventoryType = 10 }
    @{ Item = 13714; Display = 71030; CloneOf = 45660; Icon = 'INV_Vorhan_Plate_Legs'; Class = 4; Subclass = 4; Material = 1; InventoryType = 7 }
    @{ Item = 13715; Display = 71031; CloneOf = 48347; Icon = 'INV_Vorhan_Plate_Wrists'; Class = 4; Subclass = 4; Material = 1; InventoryType = 9 }
    @{ Item = 13716; Display = 71032; CloneOf = 48360; Icon = 'INV_Vorhan_Plate_Waist'; Class = 4; Subclass = 4; Material = 1; InventoryType = 6 }
    @{ Item = 13717; Display = 71033; CloneOf = 48372; Icon = 'INV_Vorhan_Plate_Feet'; Class = 4; Subclass = 4; Material = 1; InventoryType = 8 }
    @{ Item = 13672; Display = 71034; CloneOf = 40451; Icon = 'INV_Vorhan_Mail_Head'; Class = 4; Subclass = 3; Material = 5; InventoryType = 1 }
    @{ Item = 13673; Display = 71035; CloneOf = 40453; Icon = 'INV_Vorhan_Mail_Shoulders'; Class = 4; Subclass = 3; Material = 5; InventoryType = 3 }
    @{ Item = 13674; Display = 71036; CloneOf = 40448; Icon = 'INV_Vorhan_Mail_Chest'; Class = 4; Subclass = 3; Material = 5; InventoryType = 5 }
    @{ Item = 13675; Display = 71037; CloneOf = 40831; Icon = 'INV_Vorhan_Mail_Hands'; Class = 4; Subclass = 3; Material = 5; InventoryType = 10 }
    @{ Item = 13676; Display = 71038; CloneOf = 40454; Icon = 'INV_Vorhan_Mail_Legs'; Class = 4; Subclass = 3; Material = 5; InventoryType = 7 }
    @{ Item = 13677; Display = 71039; CloneOf = 48315; Icon = 'INV_Vorhan_Mail_Wrists'; Class = 4; Subclass = 3; Material = 5; InventoryType = 9 }
    @{ Item = 13678; Display = 71040; CloneOf = 48353; Icon = 'INV_Vorhan_Mail_Waist'; Class = 4; Subclass = 3; Material = 5; InventoryType = 6 }
    @{ Item = 13679; Display = 71041; CloneOf = 48364; Icon = 'INV_Vorhan_Mail_Feet'; Class = 4; Subclass = 3; Material = 5; InventoryType = 8 }
    @{ Item = 13680; Display = 71042; CloneOf = 45810; Icon = 'INV_Vorhan_Leather_Head'; Class = 4; Subclass = 2; Material = 8; InventoryType = 1 }
    @{ Item = 13681; Display = 71043; CloneOf = 45811; Icon = 'INV_Vorhan_Leather_Shoulders'; Class = 4; Subclass = 2; Material = 8; InventoryType = 3 }
    @{ Item = 13682; Display = 71044; CloneOf = 45812; Icon = 'INV_Vorhan_Leather_Chest'; Class = 4; Subclass = 2; Material = 8; InventoryType = 5 }
    @{ Item = 13683; Display = 71045; CloneOf = 45813; Icon = 'INV_Vorhan_Leather_Hands'; Class = 4; Subclass = 2; Material = 8; InventoryType = 10 }
    @{ Item = 13684; Display = 71046; CloneOf = 45814; Icon = 'INV_Vorhan_Leather_Legs'; Class = 4; Subclass = 2; Material = 8; InventoryType = 7 }
    @{ Item = 13685; Display = 71047; CloneOf = 48324; Icon = 'INV_Vorhan_Leather_Wrists'; Class = 4; Subclass = 2; Material = 8; InventoryType = 9 }
    @{ Item = 13686; Display = 71048; CloneOf = 48357; Icon = 'INV_Vorhan_Leather_Waist'; Class = 4; Subclass = 2; Material = 8; InventoryType = 6 }
    @{ Item = 13687; Display = 71049; CloneOf = 48369; Icon = 'INV_Vorhan_Leather_Feet'; Class = 4; Subclass = 2; Material = 8; InventoryType = 8 }
    @{ Item = 13688; Display = 71050; CloneOf = 40480; Icon = 'INV_Vorhan_Cloth_Head'; Class = 4; Subclass = 1; Material = 7; InventoryType = 1 }
    @{ Item = 13689; Display = 71051; CloneOf = 40684; Icon = 'INV_Vorhan_Cloth_Shoulders'; Class = 4; Subclass = 1; Material = 7; InventoryType = 3 }
    @{ Item = 13690; Display = 71052; CloneOf = 40482; Icon = 'INV_Vorhan_Cloth_Chest'; Class = 4; Subclass = 1; Material = 7; InventoryType = 20 }
    @{ Item = 13691; Display = 71053; CloneOf = 40479; Icon = 'INV_Vorhan_Cloth_Hands'; Class = 4; Subclass = 1; Material = 7; InventoryType = 10 }
    @{ Item = 13692; Display = 71054; CloneOf = 40481; Icon = 'INV_Vorhan_Cloth_Legs'; Class = 4; Subclass = 1; Material = 7; InventoryType = 7 }
    @{ Item = 13693; Display = 71055; CloneOf = 48349; Icon = 'INV_Vorhan_Cloth_Wrists'; Class = 4; Subclass = 1; Material = 7; InventoryType = 9 }
    @{ Item = 13694; Display = 71056; CloneOf = 48359; Icon = 'INV_Vorhan_Cloth_Waist'; Class = 4; Subclass = 1; Material = 7; InventoryType = 6 }
    @{ Item = 13695; Display = 71057; CloneOf = 48371; Icon = 'INV_Vorhan_Cloth_Feet'; Class = 4; Subclass = 1; Material = 7; InventoryType = 8 }
    @{ Item = 13696; Display = 71058; CloneOf = 43528; Icon = 'INV_Vorhan_Neck'; Class = 4; Subclass = 0; Material = 4; InventoryType = 2 }
    @{ Item = 13697; Display = 71059; CloneOf = 43528; Icon = 'INV_Vorhan_Ring'; Class = 4; Subclass = 0; Material = 4; InventoryType = 11 }
    @{ Item = 12187; Display = 71060; CloneOf = 47067; Icon = 'INV_Vorhan_Cloak'; Class = 4; Subclass = 1; Material = 7; InventoryType = 16 }
)
foreach ($legendary in $legendaryItems) {
    if (-not $itemOffsets.ContainsKey($legendary.Item)) { throw "Item.dbc has no row $($legendary.Item) for $($legendary.Icon)." }
    [BitConverter]::GetBytes([uint32]$legendary.Display).CopyTo($itemBytes, $itemOffsets[$legendary.Item] + 5 * 4)
    # A row of another slot made this one (InventoryType, field 6), and of another class or armour (fields 1, 2, 4):
    # the server enforces the row's
    if ($legendary.InventoryType) {
        [BitConverter]::GetBytes([uint32]$legendary.InventoryType).CopyTo($itemBytes, $itemOffsets[$legendary.Item] + 6 * 4)
    }
    if ($legendary.Class) {
        [BitConverter]::GetBytes([uint32]$legendary.Class).CopyTo($itemBytes, $itemOffsets[$legendary.Item] + 1 * 4)
        [BitConverter]::GetBytes([uint32]$legendary.Subclass).CopyTo($itemBytes, $itemOffsets[$legendary.Item] + 2 * 4)
        [BitConverter]::GetBytes([uint32]$legendary.Material).CopyTo($itemBytes, $itemOffsets[$legendary.Item] + 4 * 4)
    }
    $legendary.Painted = Test-Path -LiteralPath (Join-Path $compiledIconRoot "$($legendary.Icon).tga")
}

# --- Retail item looks (ItemDisplayInfo.dbc, Item.dbc) --------------------------------------------------------
#
# localTools\retailImport converts retail item models into modules\mod-stat-growth\client-assets\compiled\
# retail-items (shipped by patchFiles.js) and lists the rows that use them in retailItems.generated.json:
# - new ItemDisplayInfo rows (70001+), each copying a stock row's sound group and flags, with the retail model,
#   texture and icon and nothing else (no body textures, geosets or item visual of the stock row)
# - the Item.dbc rows of its test items: rows the client already had, with no template on the server, given a
#   class, slot and display. The server enforces these over item_template (DBC.EnforceItemAttributes).
# ItemDisplayInfo is built on the copy the client reads today, Patch-D's (HD weapon remakes of 124 stock rows),
# kept once as ItemDisplayInfo.before-retail-items.dbc; the server reads the same file.
$retailItemsPath = Join-Path $repoRoot 'localTools\retailImport\retailItems.generated.json'
$retailItems = Get-Content -LiteralPath $retailItemsPath -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($item in $retailItems.items) {
    if (-not $itemOffsets.ContainsKey([int]$item.entry)) {
        throw "Item.dbc has no row $($item.entry) for the retail import test item."
    }
    $offset = $itemOffsets[[int]$item.entry]
    # ClassID, SubclassID, SoundOverrideSubclassID, Material, DisplayInfoID, InventoryType, SheatheType
    $values = @($item.class, $item.subclass, -1, $item.material, $item.display, $item.inventoryType, $item.sheath)
    for ($field = 1; $field -le 7; ++$field) {
        Write-Field $itemBytes $offset $field ([long]$values[$field - 1])
    }
}
[IO.File]::WriteAllBytes($serverItemPath, $itemBytes)
[IO.File]::WriteAllBytes($clientItemPath, $itemBytes)
Write-Host "Gave $(@($glyphDisplays).Count) paragon glyph items their inscription icons (Item.dbc)."

$serverDisplayPath = Join-Path $serverDbcRoot 'ItemDisplayInfo.dbc'
$displayBackupPath = Join-Path $serverDbcRoot 'ItemDisplayInfo.before-retail-items.dbc'
$clientDisplayPath = Join-Path $clientDbcRoot 'ItemDisplayInfo.dbc'
if (-not (Test-Path -LiteralPath $displayBackupPath)) {
    & node (Join-Path $repoRoot 'localTools\mpq-builder\extractEffectiveClientFile.js') `
        'DBFilesClient\ItemDisplayInfo.dbc' $displayBackupPath
    if ($LASTEXITCODE -ne 0) { throw "Could not extract the client's ItemDisplayInfo.dbc." }
}
$displayDbc = Read-StringDbc $displayBackupPath 'ItemDisplayInfo.dbc' 25
foreach ($display in $retailItems.displays) {
    if ($displayDbc.Offsets.ContainsKey([int]$display.id)) {
        throw "ItemDisplayInfo.dbc already has a row $($display.id)."
    }
    if (-not $displayDbc.Offsets.ContainsKey([int]$display.cloneOf)) {
        throw "ItemDisplayInfo.dbc has no row $($display.cloneOf) to copy for $($display.id)."
    }
    $record = [byte[]]::new($displayDbc.RecordSize)
    [Array]::Copy($displayDbc.Data, $displayDbc.Offsets[[int]$display.cloneOf], $record, 0, $displayDbc.RecordSize)
    # Kept from the clone: 10 Flags, 11 SpellVisualID, 12 GroupSoundIndex, 13-14 helmet geoset visibility (what a
    # helmet hides of the hair and ears: a helmet clones a stock helmet of the same cover). Cleared, then set from the
    # import: 6 second icon, 7-9 geoset groups, 15-22 body textures, 23 ItemVisual, 24 ParticleColorID.
    foreach ($field in @(6, 7, 8, 9, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24)) { Set-Field $record $field 0 }
    if ($display.geosets) {
        for ($group = 0; $group -lt 3; ++$group) { Set-Field $record (7 + $group) ([uint32]$display.geosets[$group]) }
    }
    if ($display.helmetVis) {
        Set-Field $record 13 ([uint32]$display.helmetVis[0])
        Set-Field $record 14 ([uint32]$display.helmetVis[1])
    }
    if ($display.bodyTextures) {
        for ($region = 0; $region -lt 8; ++$region) {
            $texture = [string]$display.bodyTextures[$region]
            if ($texture) { Set-Field $record (15 + $region) (Add-DbcString $displayDbc.Strings $texture) }
        }
    }
    Set-Field $record 0 ([uint32]$display.id)
    for ($slot = 0; $slot -lt 2; ++$slot) {
        $model = [string]$display.modelName[$slot]
        $texture = [string]$display.modelTexture[$slot]
        Set-Field $record (1 + $slot) $(if ($model) { Add-DbcString $displayDbc.Strings $model } else { 0 })
        Set-Field $record (3 + $slot) $(if ($texture) { Add-DbcString $displayDbc.Strings $texture } else { 0 })
    }
    if ($display.icon) { Set-Field $record 5 (Add-DbcString $displayDbc.Strings ([string]$display.icon)) }
    $displayDbc.NewRecords.AddRange($record)
}
# The frost shard's: its stock row (a token's), with its own icon
if ($displayDbc.Offsets.ContainsKey($frontierShard.Display)) { throw "ItemDisplayInfo.dbc already has a row $($frontierShard.Display)." }
$record = [byte[]]::new($displayDbc.RecordSize)
[Array]::Copy($displayDbc.Data, $displayDbc.Offsets[$frontierShard.CloneOf], $record, 0, $displayDbc.RecordSize)
Set-Field $record 0 ([uint32]$frontierShard.Display)
Set-Field $record 5 (Add-DbcString $displayDbc.Strings $frontierShard.Icon)
$displayDbc.NewRecords.AddRange($record)
# The legendaries' (modules/mod-legendary): each the look it is made from, with its own icon
foreach ($legendary in @($legendaryCloak, $legendaryRing, $legendaryGloves) + $legendaryItems) {
    if ($displayDbc.Offsets.ContainsKey($legendary.Display)) { throw "ItemDisplayInfo.dbc already has a row $($legendary.Display)." }
    if (-not $displayDbc.Offsets.ContainsKey($legendary.CloneOf)) { throw "ItemDisplayInfo.dbc has no row $($legendary.CloneOf) for $($legendary.Icon)." }
    $record = [byte[]]::new($displayDbc.RecordSize)
    [Array]::Copy($displayDbc.Data, $displayDbc.Offsets[$legendary.CloneOf], $record, 0, $displayDbc.RecordSize)
    Set-Field $record 0 ([uint32]$legendary.Display)
    # A legendary not painted yet keeps its look's own icon (a missing icon is a question mark)
    if ($legendary.Painted -ne $false) { Set-Field $record 5 (Add-DbcString $displayDbc.Strings $legendary.Icon) }
    $displayDbc.NewRecords.AddRange($record)
}
$displayOutput = Get-StringDbcOutput $displayDbc
[IO.File]::WriteAllBytes($serverDisplayPath, $displayOutput)
[IO.File]::WriteAllBytes($clientDisplayPath, $displayOutput)
Write-Host ("Installed $(@($retailItems.displays).Count) retail item looks (ItemDisplayInfo.dbc) and " +
    "$(@($retailItems.items).Count) test items (Item.dbc).")

# --- Creature displays of our own: L'Infini, the ground loot (CreatureModelData.dbc, CreatureDisplayInfo.dbc) --------
#
# The Défi board's god (modules/mod-stat-growth/src/InfiniteGod.cpp) is Algalon recoloured: a copy of his model at its
# own path (localTools\infiniteBoss\buildInfiniModel.py, shipped by patchFiles.js). A CreatureModelData row copying
# Algalon's (3064) points at it, and a CreatureDisplayInfo row copying his display (28641) at that model; both 60001.
# The ground loot (modules/mod-stat-growth/src/GroundLoot.cpp) is a bag or a pile of gold under a light beam of the
# item's quality, models imported from Ascension (localTools\groundLoot\ascensionVisuals.json): each a copy of the
# sparkle's rows (2816, 25334, a spell effect worn as a creature), drawn at its own scale; 60002 on, model and display
# alike (their sounds are our own sound engine's, client-assets/audio/groundLoot.json). The server's copies and the
# client's differ: the client reads Patch-D's (HD creature remakes), the server its stock ones, and each keeps its own.
# The client's go to server\Data\dbc\client-only, which patchFiles.js ships in patch-Z.
$tintedBeam = 'Spells\Evolutions\Tint\moonbeam_impact_base_beams_{0}.mdx'
$ownDisplays = @(
    @{ Id = 60001; CloneModel = 3064; CloneDisplay = 28641; Model = 'Creature\Evolutions\Infini\Infini.mdx' }
    @{ Id = 60002; CloneModel = 2816; CloneDisplay = 25334; Model = 'Spells\ashran_loot_state.mdx'; Scale = 1.0 }
    @{ Id = 60003; CloneModel = 2816; CloneDisplay = 25334; Model = 'Spells\treasuregoblin_coinpile.mdx'; Scale = 2.5 }
)
$beamColours = @('ffffff', '1eff00', '0070dd', 'a335ee', 'ff8000', 'ffd100')
for ($index = 0; $index -lt $beamColours.Count; ++$index) {
    $ownDisplays += @{ Id = 60004 + $index; CloneModel = 2816; CloneDisplay = 25334
                       Model = ($tintedBeam -f $beamColours[$index]); Scale = 0.45 }
}
$clientOnlyDbcRoot = Join-Path $serverDbcRoot 'client-only'
New-Item -ItemType Directory -Path $clientOnlyDbcRoot -Force | Out-Null

function Add-OwnDisplays([string]$modelSource, [string]$displaySource, [string[]]$modelTargets,
                         [string[]]$displayTargets) {
    $modelDbc = Read-StringDbc $modelSource 'CreatureModelData.dbc' 28
    $displayDbc = Read-StringDbc $displaySource 'CreatureDisplayInfo.dbc' 16
    foreach ($own in $ownDisplays) {
        foreach ($check in @(@($modelDbc, $own.CloneModel), @($displayDbc, $own.CloneDisplay))) {
            if (-not $check[0].Offsets.ContainsKey([int]$check[1])) { throw "$($check[0].Name) has no row $($check[1]) to copy." }
            if ($check[0].Offsets.ContainsKey([int]$own.Id)) { throw "$($check[0].Name) already has a row $($own.Id)." }
        }
        $model = [byte[]]::new($modelDbc.RecordSize)
        [Array]::Copy($modelDbc.Data, $modelDbc.Offsets[[int]$own.CloneModel], $model, 0, $modelDbc.RecordSize)
        Set-Field $model 0 ([uint32]$own.Id)
        Set-Field $model 2 (Add-DbcString $modelDbc.Strings $own.Model)
        $modelDbc.NewRecords.AddRange($model)
        $display = [byte[]]::new($displayDbc.RecordSize)
        [Array]::Copy($displayDbc.Data, $displayDbc.Offsets[[int]$own.CloneDisplay], $display, 0, $displayDbc.RecordSize)
        Set-Field $display 0 ([uint32]$own.Id)
        Set-Field $display 1 ([uint32]$own.Id)
        if ($own.ContainsKey('Scale')) { Set-Field $display 4 (Get-FloatBits $own.Scale) }
        $displayDbc.NewRecords.AddRange($display)
    }
    $modelOutput = Get-StringDbcOutput $modelDbc
    $displayOutput = Get-StringDbcOutput $displayDbc
    foreach ($target in $modelTargets) { [IO.File]::WriteAllBytes($target, $modelOutput) }
    foreach ($target in $displayTargets) { [IO.File]::WriteAllBytes($target, $displayOutput) }
}

$serverModelBackup = Join-Path $serverDbcRoot 'CreatureModelData.before-infinite-boss.dbc'
$serverDisplayBackup = Join-Path $serverDbcRoot 'CreatureDisplayInfo.before-infinite-boss.dbc'
$clientModelBackup = Join-Path $serverDbcRoot 'CreatureModelData.client-before-infinite-boss.dbc'
$clientDisplayBackup = Join-Path $serverDbcRoot 'CreatureDisplayInfo.client-before-infinite-boss.dbc'
if (-not (Test-Path -LiteralPath $serverModelBackup)) {
    Copy-Item -LiteralPath (Join-Path $serverDbcRoot 'CreatureModelData.dbc') -Destination $serverModelBackup
}
if (-not (Test-Path -LiteralPath $serverDisplayBackup)) {
    Copy-Item -LiteralPath (Join-Path $serverDbcRoot 'CreatureDisplayInfo.dbc') -Destination $serverDisplayBackup
}
foreach ($pair in @(@('DBFilesClient\CreatureModelData.dbc', $clientModelBackup),
                    @('DBFilesClient\CreatureDisplayInfo.dbc', $clientDisplayBackup))) {
    if (-not (Test-Path -LiteralPath $pair[1])) {
        & node (Join-Path $repoRoot 'localTools\mpq-builder\extractEffectiveClientFile.js') $pair[0] $pair[1]
        if ($LASTEXITCODE -ne 0) { throw "Could not extract the client's $($pair[0])." }
    }
}
Add-OwnDisplays $serverModelBackup $serverDisplayBackup @(Join-Path $serverDbcRoot 'CreatureModelData.dbc') `
    @(Join-Path $serverDbcRoot 'CreatureDisplayInfo.dbc')
Add-OwnDisplays $clientModelBackup $clientDisplayBackup `
    @((Join-Path $clientOnlyDbcRoot 'CreatureModelData.dbc'), (Join-Path $clientDbcRoot 'CreatureModelData.dbc')) `
    @((Join-Path $clientOnlyDbcRoot 'CreatureDisplayInfo.dbc'), (Join-Path $clientDbcRoot 'CreatureDisplayInfo.dbc'))
Write-Host ("Installed $($ownDisplays.Count) creature displays of our own, $($ownDisplays[0].Id) on " +
    '(CreatureModelData.dbc, CreatureDisplayInfo.dbc).')

# The Celestial Planetarium's own music: L'Infini's room music (SoundEntries 30102 above), in a ZoneMusic row of its own
# (a copy of 514 Zone-UlduarRaidCelestialHallWalk, the hall leading to it) with 2 s of silence between two plays
# instead of 3 to 5 minutes, so it holds while players wait for the pull. The platform is one WMO group, 24083, whose
# WMOAreaTable row (43600 "The Celestial Planetarium") had no music at all (found with .infini wmo, 2026-09-29): it gets
# the new row. ZoneMusic fields: 2-3 SilenceIntervalMin, 4-5 SilenceIntervalMax (day, night), 6-7 Sounds.
# WMOAreaTable field 7: ZoneMusic.
$serverZoneMusicPath = Join-Path $serverDbcRoot 'ZoneMusic.dbc'
$zoneMusicBackupPath = Join-Path $serverDbcRoot 'ZoneMusic.before-infinite-room.dbc'
if (-not (Test-Path -LiteralPath $zoneMusicBackupPath)) {
    Copy-Item -LiteralPath $serverZoneMusicPath -Destination $zoneMusicBackupPath
}
$zoneMusicDbc = Read-StringDbc $zoneMusicBackupPath 'ZoneMusic.dbc' 8
$roomMusic = [byte[]]::new($zoneMusicDbc.RecordSize)
[Array]::Copy($zoneMusicDbc.Data, $zoneMusicDbc.Offsets[514], $roomMusic, 0, $zoneMusicDbc.RecordSize)
$roomMusicId = [uint32]($zoneMusicDbc.MaxId + 1)
Set-Field $roomMusic 0 $roomMusicId
Set-Field $roomMusic 1 (Add-DbcString $zoneMusicDbc.Strings 'Zone-EvolutionsLInfiniRoom')
foreach ($field in 2..5) { Set-Field $roomMusic $field 2000 }
foreach ($field in 6..7) { Set-Field $roomMusic $field 30102 }
$zoneMusicDbc.NewRecords.AddRange($roomMusic)
$zoneMusicOutput = Get-StringDbcOutput $zoneMusicDbc
[IO.File]::WriteAllBytes($serverZoneMusicPath, $zoneMusicOutput)
[IO.File]::WriteAllBytes((Join-Path $clientDbcRoot 'ZoneMusic.dbc'), $zoneMusicOutput)

$serverWmoAreaPath = Join-Path $serverDbcRoot 'WMOAreaTable.dbc'
$wmoAreaBackupPath = Join-Path $serverDbcRoot 'WMOAreaTable.before-infinite-room.dbc'
if (-not (Test-Path -LiteralPath $wmoAreaBackupPath)) {
    Copy-Item -LiteralPath $serverWmoAreaPath -Destination $wmoAreaBackupPath
}
$wmoAreaBytes = [IO.File]::ReadAllBytes($wmoAreaBackupPath)
Assert-Wdbc $wmoAreaBytes 'WMOAreaTable.dbc'
$wmoAreaRecordSize = [BitConverter]::ToInt32($wmoAreaBytes, 12)
$planetariumArea = $null
for ($index = 0; $index -lt [BitConverter]::ToInt32($wmoAreaBytes, 4); ++$index) {
    if ([BitConverter]::ToUInt32($wmoAreaBytes, 20 + $index * $wmoAreaRecordSize) -eq 43600) {
        $planetariumArea = 20 + $index * $wmoAreaRecordSize
    }
}
if ($null -eq $planetariumArea) { throw 'WMOAreaTable.dbc has no row 43600.' }
Write-Field $wmoAreaBytes $planetariumArea 7 $roomMusicId
[IO.File]::WriteAllBytes($serverWmoAreaPath, $wmoAreaBytes)
[IO.File]::WriteAllBytes((Join-Path $clientDbcRoot 'WMOAreaTable.dbc'), $wmoAreaBytes)
Write-Host "Gave the Celestial Planetarium L'Infini's room music (ZoneMusic.dbc $roomMusicId, WMOAreaTable.dbc 43600)."

# L'Infini's gear (modules/mod-stat-growth/src/MythicDungeonSystem.cpp TouchByInfiniteGod): five enchantments on every
# piece it gives, one in each of its random property slots, shown by the tooltip as five lines of the item: the bonus's
# name (starry blue; the one carrying its equip spell, localTools\infiniteBoss\Spells.ps1), what it does in two lines
# (green, as the item's own "Équipé :" lines) and a line of its lore in two (gold, as an item's flavour text). A
# tooltip line is never wrapped: each stays about as wide as the item's own lines. Five ids a kind from 3890: armour, weapons,
# jewellery. Fields: 2-4 Effect, 11-13 EffectArg, 14-29 Name, 30 its locale mask.
$infiniName = '|cff7fbfff{0}|r'
$infiniEffect = '|cff00ff00{0}|r'
$infiniLore = '|cffffd100{0}|r'
$infiniGear = @(
    @{ Spell = 90757; Lines = @(($infiniName -f 'Égide des astres'),
        ($infiniEffect -f 'Équipé : quand vous subissez des dégâts, chance'),
        ($infiniEffect -f 'de réduire de 5% les dégâts subis pendant 10 s.'),
        ($infiniLore -f '« Tissé dans la lumière d''étoiles mortes'),
        ($infiniLore -f 'bien avant la naissance d''Azeroth. »')) }
    @{ Spell = 90759; Lines = @(($infiniName -f 'Éclat d''étoile filante'),
        ($infiniEffect -f 'Équipé : vos attaques et sorts nuisibles ont'),
        ($infiniEffect -f 'une chance d''infliger 4000 dégâts des Arcanes.'),
        ($infiniLore -f '« Une étoile filante, figée'),
        ($infiniLore -f 'à l''instant de sa chute. »')) }
    @{ Spell = 90761; Lines = @(($infiniName -f 'Étincelle d''éternité'),
        ($infiniEffect -f 'Équipé : vos attaques et sorts ont une chance'),
        ($infiniEffect -f 'd''augmenter votre score de hâte de 150 (10 s).'),
        ($infiniLore -f '« Une parcelle du temps'),
        ($infiniLore -f 'que L''Infini a laissée s''échapper. »')) }
)
$infiniEnchants = @()
for ($kind = 0; $kind -lt $infiniGear.Count; ++$kind) {
    for ($line = 0; $line -lt 5; ++$line) {
        $infiniEnchants += @{ Id = 3890 + 5 * $kind + $line; Name = $infiniGear[$kind].Lines[$line]
            Spell = $(if ($line -eq 0) { $infiniGear[$kind].Spell } else { 0 }) }
    }
}
# The Hollow Voice's gear (MythicDungeonSystem.cpp TouchByHollowVoice, one piece in four): the same five lines, its
# name in Aldric's pale gold-violet. Five ids a bonus from 4100. Their spells: localTools\hollowVoice\Spells.ps1.
$voiceName = '|cffd8b4ff{0}|r'
$voiceGear = @(
    @{ Spell = 94156; Lines = @(($voiceName -f 'Ailes du Séraphin'),
        ($infiniEffect -f 'Équipé : vos attaques, sorts et soins ont une chance'),
        ($infiniEffect -f 'd''augmenter vos dégâts et soins de 10% (10 s).'),
        ($infiniLore -f '« Les ailes qu''Aldric déploya'),
        ($infiniLore -f 'pour sa dernière prière. »')) }
    @{ Spell = 94158; Lines = @(($voiceName -f 'Égide d''Aldric'),
        ($infiniEffect -f 'Équipé : quand vous subissez des dégâts, chance'),
        ($infiniEffect -f 'd''absorber 30000 points de dégâts (10 s).'),
        ($infiniLore -f '« Sa lumière garde encore'),
        ($infiniLore -f 'ceux qui l''ont libéré. »')) }
    @{ Spell = 94160; Lines = @(($voiceName -f 'Murmure de Vel''thazar'),
        ($infiniEffect -f 'Équipé : vos attaques et sorts nuisibles ont une'),
        ($infiniEffect -f 'chance d''infliger 35000 dégâts d''Ombre.'),
        ($infiniLore -f '« Le démon s''est tu.'),
        ($infiniLore -f 'Sa voix, elle, murmure encore. »')) }
)
for ($kind = 0; $kind -lt $voiceGear.Count; ++$kind) {
    for ($line = 0; $line -lt 5; ++$line) {
        $infiniEnchants += @{ Id = 4100 + 5 * $kind + $line; Name = $voiceGear[$kind].Lines[$line]
            Spell = $(if ($line -eq 0) { $voiceGear[$kind].Spell } else { 0 }) }
    }
}
$serverEnchantPath = Join-Path $serverDbcRoot 'SpellItemEnchantment.dbc'
$enchantBackupPath = Join-Path $serverDbcRoot 'SpellItemEnchantment.before-infinite-gear.dbc'
if (-not (Test-Path -LiteralPath $enchantBackupPath)) {
    Copy-Item -LiteralPath $serverEnchantPath -Destination $enchantBackupPath
}
$enchantDbc = Read-StringDbc $enchantBackupPath 'SpellItemEnchantment.dbc' 38
foreach ($enchant in $infiniEnchants) {
    if ($enchantDbc.Offsets.ContainsKey([int]$enchant.Id)) { throw "SpellItemEnchantment.dbc already has a row $($enchant.Id)." }
    $record = [byte[]]::new($enchantDbc.RecordSize)
    Set-Field $record 0 ([uint32]$enchant.Id)
    if ($enchant.Spell) {
        Set-Field $record 2 3
        Set-Field $record 11 ([uint32]$enchant.Spell)
    }
    $nameOffset = Add-DbcString $enchantDbc.Strings $enchant.Name
    for ($locale = 0; $locale -lt 16; ++$locale) { Set-Field $record (14 + $locale) $nameOffset }
    Set-Field $record 30 16712190
    $enchantDbc.NewRecords.AddRange($record)
}
$enchantOutput = Get-StringDbcOutput $enchantDbc
[IO.File]::WriteAllBytes($serverEnchantPath, $enchantOutput)
[IO.File]::WriteAllBytes((Join-Path $clientDbcRoot 'SpellItemEnchantment.dbc'), $enchantOutput)
Write-Host "Added L'Infini's $($infiniEnchants.Count) gear enchantments (SpellItemEnchantment.dbc)."

$generatedIcons = @(Get-ChildItem -LiteralPath $compiledIconRoot -Filter 'CombatRogue_*.tga' -ErrorAction SilentlyContinue).Count
$newVisualCount = $visualDbc.NewRecords.Count / $visualDbc.RecordSize
Write-Host "Installed $($visualIdsBySpell.Count) custom spell visuals ($newVisualCount new, $($customVisualKits.Count) new kits)."
Write-Host "Oathblade: $($blueEffectIds.Count) blue effect models of its own, used by its $($oathbladeKits.Count) kits."
Write-Host "Installed $($customSounds.Count) custom sound entry with $($customSounds[0].Files.Count) quiet impact variations."
$pestifereTalentRanks = ($pestifereTalents | ForEach-Object { $_.Ids.Count } | Measure-Object -Sum).Sum
Write-Host "Changed $stockEditCount stock spells in place (localTools\hunter\StockSpells.ps1, localTools\priest\StockSpells.ps1, localTools\warrior\StockSpells.ps1, localTools\shaman\StockSpells.ps1, localTools\warlock\StockSpells.ps1, localTools\druid\StockSpells.ps1, localTools\rogue\StockSpells.ps1)."
Write-Host "Swapped the range of $stockRangeRemapCount stock spells (no ranged dead zone, localTools\hunter\StockSpells.ps1)."
Write-Host "Installed $($customSpells.Count) custom spells ($($spellbookSpells.Count) in the spellbook) and $($foundTalentRanks.Count) Combat talent ranks."
Write-Host "Pestiféré talent trees: $($pestifereTalents.Count) talents, $pestifereTalentRanks ranks (run buildCustomClasses.py next for the grid)."
Write-Host "Combat rogue icons generated: $generatedIcons (missing ones use stock game icons)."

# --- No Oathblade spell may require combo points -----------------------------------------------------------
#
# Every one of them clones a rogue ability, and a rogue finisher carries SPELL_ATTR1_FINISHING_MOVE_DAMAGE
# (0x00100000) or _DURATION (0x00400000) in AttributesEx. Unless a spell sets that field explicitly it keeps
# its clone's, and the client then refuses the cast with "requires combo points". This class spends Flow.
#
# Read back off the file that was just written, so it checks what shipped rather than what was intended.
$comboBits = 0x00100000 -bor 0x00400000
$verifyBytes = [IO.File]::ReadAllBytes($serverSpellPath)
$verifyCount = [BitConverter]::ToInt32($verifyBytes, 4)
$verifySize = [BitConverter]::ToInt32($verifyBytes, 12)
$comboOffenders = [Collections.Generic.List[string]]::new()
for ($index = 0; $index -lt $verifyCount; ++$index) {
    $recordAt = 20 + $index * $verifySize
    $spellId = [BitConverter]::ToUInt32($verifyBytes, $recordAt)
    if ($spellId -lt 90800 -or $spellId -gt 91199) { continue }
    $attributesEx = [BitConverter]::ToInt32($verifyBytes, $recordAt + 5 * 4)
    if ($attributesEx -band $comboBits) {
        [void]$comboOffenders.Add("$spellId (AttributesEx 0x{0:X8})" -f $attributesEx)
    }
}
if ($comboOffenders.Count) {
    throw ("Oathblade spells still require combo points: " + ($comboOffenders -join ', ') +
        ". Set field 5 on them in localTools\oathblade\Spells.ps1 instead of inheriting the clone's.")
}
Write-Host "Checked $($comboOffenders.Count + 0) Oathblade spells requiring combo points (expected 0)."

