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

# Spell.dbc field indexes used below (3.3.5a, 234 fields)
$F_Attributes = 4; $F_AttributesEx = 5; $F_CasterAuraState = 20; $F_ProcFlags = 34; $F_ProcChance = 35
$F_DurationIndex = 40; $F_ManaCost = 42; $F_StackAmount = 49; $F_EquippedItemClass = 68
$F_EffectBasePoints = 80; $F_EffectRadius = 92; $F_SpellVisual = 131; $F_SpellIconID = 133
$F_Name = 136; $F_Description = 170; $F_AuraDescription = 187

$sinisterStrikeRanks = @(1752, 1757, 1758, 1759, 1760, 8621, 11293, 11294, 26861, 26862, 48637, 48638)
$eviscerateRanks = @(2098, 6760, 6761, 6762, 8623, 8624, 11299, 11300, 31016, 26865, 48667, 48668)
$sliceAndDiceRanks = @(5171, 6774)

# Aura type ids
$A_Dummy = 4; $A_ModIncreaseSpeed = 31; $A_ModParryPercent = 47; $A_ModDodgePercent = 49; $A_ModHitChance = 54
$A_ModDamagePercentTaken = 87; $A_ModPowerRegenPercent = 110; $A_ModOffhandDamagePct = 122; $A_ModMeleeHaste = 138
$A_ModExpertise = 240; $A_ModCritPct = 290

$customSpells = @(
    # --- Combat rogue: abilities ---
    @{ Id = 90010; Clone = 1752; Name = 'Quick Cut'; IconPath = 'Interface\Icons\RogueMomentum_QuickCut'; Cost = 20; Cooldown = 0; Level = 4; Spellbook = $true
       Description = 'Requires Opening. Carve through the target for 175% damage and generate 2 combo points. Consumes Opening.'
       Fields = @{ 20 = 9 }
       # Mutilate: twin stab animation, deep wound on the target
       Visual = @{ Clone = 7913 } },
    @{ Id = 90011; Clone = 36554; Name = 'Shadow Lunge'; IconPath = 'Interface\Icons\RogueMomentum_ShadowLunge'; Cost = 0; Cooldown = 12000; Level = 8; Spellbook = $true; ShadowLungeDamage = $true
       Description = 'Step through the shadows to your target and strike for 150% weapon damage, generating a combo point. The cooldown resets when you kill an enemy.'
       # Shadowstep smoke with a weapon swing, shadow slash on the target
       Visual = @{ Clone = 8262; Cast = 'ShadowLungeCast'; Impact = 6642 } },
    @{ Id = 90012; Clone = 14278; Name = 'Riposte'; IconPath = 'Interface\Icons\RogueMomentum_Riposte'; Cost = 10; Cooldown = 8000; Level = 10; Spellbook = $true
       Description = 'Counterattack for 165% weapon damage and a combo point. Within 5 sec after you dodge or parry, Riposte strikes critically and grants an additional combo point.'
       # Devastate's glowing blades, then Mortal Strike's heavy wound
       Visual = @{ Clone = 12295; Cast = 11383; Impact = 437; TargetImpact = 0 } },
    @{ Id = 90100; Clone = 51723; Name = 'Crescent Slash'; Icon = 'CombatRogue_CrescentSlash'; Cost = 20; Cooldown = 0; Level = 14; Spellbook = $true; NoEquipment = $true
       Description = 'Slash all enemies within 8 yards for 110% weapon damage and generate a combo point. Restores 2 Energy for each enemy hit, up to 8.'
       Fields = @{ 80 = 109; 92 = 18 }
       # Cleave: ground crescent under the rogue, slash on every enemy hit
       Visual = @{ Clone = 219; Impact = 'CrescentSlashImpact' } },
    @{ Id = 90017; Clone = 51723; Name = 'Crimson Sweep'; IconPath = 'Interface\Icons\RogueMomentum_CrimsonSweep'; Cost = 30; Cooldown = 6000; Level = 18; Spellbook = $true; NoEquipment = $true
       Description = 'Sweep through all enemies within 8 yards for 60% weapon damage, apply Crimson Wounds for 6 sec and generate a combo point. Each Crimson Wounds tick restores 2 Energy. Consumes Opening to make the bleed last 10 sec and generate 2 combo points.'
       Fields = @{ 80 = 59; 92 = 18 }
       # Blood-red glowing blades, blood burst on every enemy hit
       Visual = @{ Clone = 11117; Cast = 10971; Impact = 'CrimsonSweepImpact' } },

    # --- Combat rogue: finishers ---
    @{ Id = 90013; Clone = 5171; Name = 'Sanguine Veil'; IconPath = 'Interface\Icons\RogueMomentum_SanguineVeil'; Cost = 20; Cooldown = 0; Level = 16; Spellbook = $true
       Description = 'Finishing move that shrouds you in a sanguine veil, healing you for 15% of all damage you deal. Lasts 6 sec per combo point. Consumes up to 30 extra Energy to last up to 50% longer.'
       AuraDescription = 'Healing for a share of damage dealt.'
       Effects = @(@{ Index = 0; Aura = $A_Dummy }, @{ Index = 1; Aura = $A_ModDamagePercentTaken; BasePoints = 0; Misc = 127 })
       Fields = @{ 40 = 21; 209 = 0; 210 = 0; 211 = 0 }
       # Slice and Dice flourish, blood tap burst, red glowing hands while the veil lasts
       Visual = @{ Clone = 254; Cast = 416; Impact = 10285; State = 108 } },
    @{ Id = 90101; Clone = 51723; Name = 'Blood Waltz'; Icon = 'CombatRogue_BloodWaltz'; FallbackIconSpell = 46924; Cost = 30; Cooldown = 0; Level = 26; Spellbook = $true; NoEquipment = $true
       Description = 'Finishing move that spins through all enemies within 8 yards, dealing 45% weapon damage per combo point to each, refreshing your Crimson Wounds on them and extending Battle Tempo by 1 sec per enemy hit. Consumes up to 30 extra Energy to deal up to 50% more damage.'
       Fields = @{ 5 = 0x00100010; 80 = 44; 92 = 18 }
       # Whirlwind spin, blood strike slash on every enemy hit
       Visual = @{ Clone = 223; Cast = 369; Impact = 'BloodWaltzImpact' } },
    @{ Id = 90105; Clone = 51723; Name = 'Crimson Daggerfall'; Icon = 'CombatRogue_CrimsonDaggerfall'; FallbackIconSpell = 51723; Cost = 30; Cooldown = 15000; Level = 20; Spellbook = $true; NoEquipment = $true
       Description = 'Finishing move that launches a storm of daggers at all enemies within 8 yards, dealing 60% weapon damage per combo point. Deals 30% more damage to enemies suffering from one of your damage-over-time effects. Consumes up to 30 extra Energy to deal up to 50% more damage. Improves at levels 45 and 70.'
       Fields = @{ 5 = 0x00100010; 80 = 59; 92 = 18 }
       # A physical dagger missile reaches every affected enemy; its custom impact kit owns the randomized sound.
       Visual = @{ Clone = 14261; Impact = 'DaggerfallImpact' } },

    # --- Combat rogue: buffs, bleed and hidden support spells ---
    @{ Id = 90014; Clone = 2983; Name = 'Opening'; IconPath = 'Interface\Icons\RogueMomentum_Opening'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Quick Cut is available.'; AuraDescription = 'Quick Cut is available.' },
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
    @{ Id = 90021; Clone = 2565; Name = 'Gladiator Stance'; IconPath = 'Interface\Icons\Ability_Warrior_GladiatorStance'; Description = 'Fight as a gladiator while in Defensive Stance with a shield equipped. Doubles all damage you deal and all damage you take before other reductions. Cast again to leave.'; AuraDescription = 'Damage dealt and damage taken increased by 100%.'; Cost = 0; Cooldown = 0; Level = 10; Spellbook = $true; GladiatorStance = $true; SkillLine = 257; ClassMask = 1 }
)
$spellbookSpells = @($customSpells | Where-Object { $_.Spellbook })

$customSounds = @(
    @{ Key = 'DaggerfallImpact'; Clone = 13269; Name = 'Crimson_Daggerfall_Impact'
       Directory = 'Sound\Spells\Custom\CombatRogue'
       Files = @('DaggerfallImpact01.ogg', 'DaggerfallImpact02.ogg', 'DaggerfallImpact03.ogg', 'DaggerfallImpact04.ogg', 'DaggerfallImpact05.ogg')
       # Near normal combat-effect volume while retaining headroom when several target impacts overlap.
       Volume = 0.85 }
)

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
)
$visualKitSlots = @{
    Precast = 1; Cast = 2; Impact = 3; State = 4; StateDone = 5; Channel = 6; CasterImpact = 14; TargetImpact = 15
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
       Description = 'Dodging or parrying an attack resets the cooldown of Riposte. This effect cannot occur more than once every 6 sec. Riposte critical strikes grant Opening.' },
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
    @{ Name = 'Rending Arc'; Icon = 'CombatRogue_RendingArc'; Ids = @(13960, 13961, 13962, 13963, 13964); V0 = @(4, 8, 12, 16, 20); V1 = @('.', '.', '.', '.', ' and extends their Crimson Wounds by 2 sec.')
       Description = 'Crescent Slash deals {0}% more damage to enemies affected by your Crimson Wounds{1}' },
    @{ Name = 'Relentless Flow'; Icon = 'CombatRogue_RelentlessFlow'; Ids = @(30919, 30920); V0 = @('.', ', and its free Quick Cut restores 10 Energy.')
       Description = 'Flowing Strikes triggers on every 3rd Sinister Strike{0}' },
    @{ Name = 'Widening Arcs'; Icon = 'CombatRogue_WideningArcs'; Ids = @(31124, 31126); V0 = @(2, 4)
       Description = 'Increases the radius of Crescent Slash, Crimson Sweep and Blood Waltz by {0} yards.' },
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
       Description = 'Crescent Slash has a {0}% chance to apply Crimson Wounds to each enemy it hits.' },
    @{ Name = 'Riposte Echo'; Icon = 'CombatRogue_RiposteEcho'; Ids = @(51672, 51674); V0 = @(1, 2); V1 = @('enemy', 'enemies')
       Description = 'Riposte also strikes {0} additional {1} in front of you.' },
    @{ Name = 'Waltz of Blades'; Icon = 'CombatRogue_WaltzOfBlades'; Ids = @(32601)
       Description = 'Blood Waltz grants 1 combo point for each enemy hit, up to 3.' },
    @{ Name = 'Crimson Frenzy'; Icon = 'CombatRogue_CrimsonFrenzy'; Ids = @(51682, 58413); V0 = @(1, 2)
       Description = 'Each tick of your Crimson Wounds grants you {0}% attack speed for 5 sec, stacking up to 5 times.' },
    @{ Name = 'Executioner''s Tempo'; Icon = 'CombatRogue_ExecutionersTempo'; Ids = @(51685, 51686, 51687, 51688, 51689); V0 = @(4, 8, 12, 16, 20); V1 = @(3, 6, 9, 12, 15)
       Description = 'Your abilities deal {0}% more damage to enemies below 35% health, and killing an enemy restores {1} Energy.' },
    @{ Name = 'Crimson Cadence'; Icon = 'CombatRogue_CrimsonCadence'; Ids = @(51690)
       Description = 'Every 5th finishing move releases a free Blood Waltz with 5 combo points. Against a single enemy it releases an echo of your last Eviscerate for 50% of its damage instead.' }
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
    for ($locale = 0; $locale -lt 16; ++$locale) {
        Write-Field $buffer $base ($firstField + $locale) $stringOffset
    }
}

# Replaces all three effects. Value = final amount (base points + 1 die); BasePoints = raw base points (no die).
function Write-Effects([byte[]]$buffer, [int]$base, $effects) {
    for ($field = 71; $field -le 130; ++$field) { Write-Field $buffer $base $field 0 }
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
    $dbc.MaxId = $dbc.MaxId + 1
    Set-Field $record 0 ([uint32]$dbc.MaxId)
    Set-Field $record 2 (Add-DbcString $dbc.Strings $sound.Name)
    for ($index = 0; $index -lt 10; ++$index) {
        if ($index -lt $sound.Files.Count) {
            Set-Field $record (3 + $index) (Add-DbcString $dbc.Strings $sound.Files[$index])
            Set-Field $record (13 + $index) 1
        }
        else {
            Set-Field $record (3 + $index) 0
            Set-Field $record (13 + $index) 0
        }
    }
    Set-Field $record 23 (Add-DbcString $dbc.Strings $sound.Directory)
    Set-Field $record 24 ([BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$sound.Volume), 0))
    $dbc.NewRecords.AddRange($record)
    return [uint32]$dbc.MaxId
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
$soundIdsByKey = @{}
foreach ($sound in $customSounds) {
    $soundIdsByKey[$sound.Key] = Add-SoundEntry $soundDbc $sound
}
$visualDbc = Read-Dbc $visualBackupPath 'SpellVisual.dbc' 32
$visualKitDbc = Read-Dbc $visualKitBackupPath 'SpellVisualKit.dbc' 38
$kitIdsByKey = @{}
foreach ($kit in $customVisualKits) {
    $fields = @{}
    foreach ($field in $kit.Fields.Keys) { $fields[$field] = $kit.Fields[$field] }
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
    if ($spec.Icon -and (Test-Path -LiteralPath (Join-Path $compiledIconRoot "$($spec.Icon).tga"))) {
        return Get-IconIdForPath "Interface\Icons\$($spec.Icon)"
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

$sinisterDescription = Add-DbcString $strings 'Strike for weapon damage, generating a combo point and 10 Energy. Has a 25% chance to grant Opening, and always grants it on a critical strike.'
$eviscerateAppend = ' Grants Battle Tempo: 3% attack speed per combo point for 10 sec. Consumes up to 30 extra Energy to deal up to 50% more damage.'
$sliceAndDiceAppend = ' Consumes up to 30 extra Energy to last up to 50% longer.'
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

for ($index = 0; $index -lt $recordCount; ++$index) {
    $offset = $index * $recordSize
    $spellId = [BitConverter]::ToUInt32($records, $offset)
    foreach ($localeGroup in @(136, 153, 170, 187)) {
        $englishOffset = [BitConverter]::ToUInt32($records, $offset + $localeGroup * 4)
        if ($englishOffset -ne 0) {
            for ($locale = 1; $locale -lt 16; ++$locale) {
                $localeOffset = $offset + ($localeGroup + $locale) * 4
                if ([BitConverter]::ToUInt32($records, $localeOffset) -eq 0) {
                    [BitConverter]::GetBytes($englishOffset).CopyTo($records, $localeOffset)
                }
            }
        }
    }
    if ($fallbackIconSpells.Contains([int]$spellId)) {
        $iconIdsBySpell[[int]$spellId] = Read-Field $records $offset $F_SpellIconID
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
        for ($field = 5; $field -le 36; ++$field) { Write-Field $records $offset $field 0 }
        Write-Field $records $offset 28 1
        Write-Field $records $offset 37 0
        Write-Field $records $offset $F_DurationIndex 21
        for ($field = 41; $field -le 45; ++$field) { Write-Field $records $offset $field 0 }
        Write-Field $records $offset 46 1
        Write-Field $records $offset $F_StackAmount 0
        for ($field = 50; $field -le 67; ++$field) { Write-Field $records $offset $field 0 }
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
        for ($field = 205; $field -le 214; ++$field) { Write-Field $records $offset $field 0 }
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
    foreach ($custom in $customSpells) {
        if ($spellId -eq $custom.Clone) {
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
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
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
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
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
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
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
        for ($field = 128; $field -le 130; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 213 2
        Set-Field $record 225 1
    }
    if ($custom.GladiatorStance) {
        # Permanent self aura usable only in Defensive Stance: +100% damage done and taken, all schools.
        # The shield requirement is enforced by the module script: an item requirement here would stop
        # the damage done bonus from applying to weapon attacks.
        Set-Field $record 1 0
        Set-Field $record 4 0x10
        for ($field = 5; $field -le 11; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 12 131072
        for ($field = 13; $field -le 27; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 28 1
        for ($field = 31; $field -le 36; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 40 21
        Set-Field $record 41 1
        Set-Field $record 46 1
        Set-Field $record 49 0
        for ($field = 50; $field -le 67; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
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
        for ($field = 209; $field -le 214; ++$field) { Set-Field $record $field 0 }
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
    if ($visualIdsBySpell.ContainsKey([int]$custom.Id)) {
        Set-Field $record $F_SpellVisual $visualIdsBySpell[[int]$custom.Id]
        Set-Field $record ($F_SpellVisual + 1) 0
    }
    $fallbackIconId = if ($custom.FallbackIconSpell) { $iconIdsBySpell[[int]$custom.FallbackIconSpell] } else { $cloneIconId }
    Set-Field $record $F_SpellIconID (Resolve-IconId $custom ([uint32]$fallbackIconId))
    $nameOffset = Add-DbcString $strings $custom.Name
    $descriptionOffset = Add-DbcString $strings $custom.Description
    $auraDescriptionOffset = if ($custom.AuraDescription) { Add-DbcString $strings $custom.AuraDescription } else { [uint32]0 }
    for ($locale = 0; $locale -lt 16; ++$locale) {
        Set-Field $record (136 + $locale) $nameOffset
        Set-Field $record (153 + $locale) 0
        Set-Field $record (170 + $locale) $descriptionOffset
        Set-Field $record (187 + $locale) $auraDescriptionOffset
    }
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
    Set-Field $record 9 2
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

$generatedIcons = @(Get-ChildItem -LiteralPath $compiledIconRoot -Filter 'CombatRogue_*.tga' -ErrorAction SilentlyContinue).Count
$newVisualCount = $visualDbc.NewRecords.Count / $visualDbc.RecordSize
Write-Host "Installed $($visualIdsBySpell.Count) custom spell visuals ($newVisualCount new, $($customVisualKits.Count) new kits)."
Write-Host "Installed $($customSounds.Count) custom sound entry with $($customSounds[0].Files.Count) quiet impact variations."
Write-Host "Installed $($customSpells.Count) custom spells ($($spellbookSpells.Count) in the spellbook) and $($foundTalentRanks.Count) Combat talent ranks."
Write-Host "Combat rogue icons generated: $generatedIcons (missing ones use stock game icons)."
