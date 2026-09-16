DELETE FROM `spell_script_names`
WHERE `ScriptName` IN (
    'RogueCrimsonSweepScript',
    'RogueCrimsonWoundsAuraScript',
    'QuickTravelSpellScript'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90017, 'RogueCrimsonSweepScript'),
(90018, 'RogueCrimsonWoundsAuraScript'),
(90019, 'QuickTravelSpellScript');
