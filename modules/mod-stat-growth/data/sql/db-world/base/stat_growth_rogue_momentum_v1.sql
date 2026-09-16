DELETE FROM `spell_script_names`
WHERE `ScriptName` IN (
    'SinisterStrikeEnergyScript',
    'RogueQuickCutScript',
    'RogueShadowLungeScript',
    'RogueRiposteScript',
    'RogueEviscerateScript'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-1752, 'SinisterStrikeEnergyScript'),
(16511, 'RogueQuickCutScript'),
(36554, 'RogueShadowLungeScript'),
(14278, 'RogueRiposteScript'),
(-2098, 'RogueEviscerateScript');
