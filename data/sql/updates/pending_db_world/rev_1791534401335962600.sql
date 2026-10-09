-- The Crimson Duelist (the Combat rogue rework, mod-stat-growth CombatRogue*.cpp) is retired for Hors-la-loi (retail
-- Outlaw, mod-rogue RogueOutlaw.cpp): its scripts are gone from the code. No spell binds them any more - the names
-- stat_growth_combat_rogue_v1.sql, stat_growth_combat_rogue_daggerfall.sql and the momentum updates before them bound
-- (Sinister Strike, Kick, Eviscerate and Slice and Dice ranks, the kit 90010-90018 / 90100-90105) - and its hidden
-- passive's proc row goes with it.
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
    'CombatRogueCrimsonDuelistAuraScript',
    'CombatRogueCrimsonDaggerfallScript'
);

-- Whatever else still names the Crimson kit's spells (Quick Travel, 90019, stays)
DELETE FROM `spell_script_names` WHERE `spell_id` BETWEEN 90010 AND 90018 OR `spell_id` BETWEEN 90100 AND 90105;

DELETE FROM `spell_proc` WHERE `SpellId` = 90104;
