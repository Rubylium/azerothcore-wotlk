-- Combat rogue rework: scripts for the new kit, the hidden Crimson Duelist proc passive, and removal of the
-- WotLK behaviour bound to the Combat talent spells, which are all reworked into new talents

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
    'SinisterStrikeEnergyScript',
    'RogueQuickCutScript',
    'RogueShadowLungeScript',
    'RogueRiposteScript',
    'RogueEviscerateScript',
    'RogueCrimsonSweepScript',
    'RogueCrimsonWoundsAuraScript',
    'CombatRogueSinisterStrikeScript',
    'CombatRogueQuickCutScript',
    'CombatRogueShadowLungeScript',
    'CombatRogueRiposteScript',
    'CombatRogueCrescentSlashScript',
    'CombatRogueCrimsonSweepScript',
    'CombatRogueKickScript',
    'CombatRogueEviscerateScript',
    'CombatRogueSliceAndDiceScript',
    'CombatRogueSanguineVeilScript',
    'CombatRogueBloodWaltzScript',
    'CombatRogueOpeningAuraScript',
    'CombatRogueBattleTempoAuraScript',
    'CombatRogueKillingMomentumAuraScript',
    'CombatRogueCrimsonWoundsAuraScript',
    'CombatRogueCrimsonFrenzyAuraScript',
    'CombatRogueCrimsonDuelistAuraScript'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-1752, 'CombatRogueSinisterStrikeScript'),
(90010, 'CombatRogueQuickCutScript'),
(90011, 'CombatRogueShadowLungeScript'),
(90012, 'CombatRogueRiposteScript'),
(90100, 'CombatRogueCrescentSlashScript'),
(90017, 'CombatRogueCrimsonSweepScript'),
(-1766, 'CombatRogueKickScript'),
(-2098, 'CombatRogueEviscerateScript'),
(-5171, 'CombatRogueSliceAndDiceScript'),
(90013, 'CombatRogueSanguineVeilScript'),
(90101, 'CombatRogueBloodWaltzScript'),
(90014, 'CombatRogueOpeningAuraScript'),
(90015, 'CombatRogueBattleTempoAuraScript'),
(90016, 'CombatRogueKillingMomentumAuraScript'),
(90018, 'CombatRogueCrimsonWoundsAuraScript'),
(90103, 'CombatRogueCrimsonFrenzyAuraScript'),
(90104, 'CombatRogueCrimsonDuelistAuraScript');

-- Talent spells now reworked: Prey on the Weak, Combat Potency, Nerves of Steel, Blade Flurry, Killing Spree
DELETE FROM `spell_script_names` WHERE (`spell_id`, `ScriptName`) IN (
    (-51685, 'spell_rog_prey_on_the_weak'),
    (-35541, 'spell_rog_combat_potency'),
    (-31130, 'spell_rog_nerves_of_steel'),
    (13877, 'spell_rog_blade_flurry'),
    (51690, 'spell_rog_killing_spree')
);

DELETE FROM `spell_linked_spell` WHERE `spell_trigger` = 51690 AND `spell_effect` = 61851;

-- Savage Combat, Unfair Advantage, Combat Potency, Blade Twisting, Hack and Slash, Improved Kick,
-- Throwing Specialization
DELETE FROM `spell_proc` WHERE `SpellId` IN (-51682, -51672, -35541, -31124, -13960, -13754, -5952);

-- Crimson Duelist: dodges and parries taken (melee attacks and melee spells), critical melee spell hits done
-- ProcFlags 0x38 = TAKEN_MELEE_AUTO_ATTACK | DONE_SPELL_MELEE_DMG_CLASS | TAKEN_SPELL_MELEE_DMG_CLASS
-- HitMask 0x32 = CRITICAL | DODGE | PARRY
DELETE FROM `spell_proc` WHERE `SpellId` = 90104;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(90104, 0, 0, 0, 0, 0, 56, 0, 2, 50, 0, 0, 0, 100, 0, 0);
