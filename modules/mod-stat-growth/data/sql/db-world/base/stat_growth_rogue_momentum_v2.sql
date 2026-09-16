DELETE FROM `spell_script_names`
WHERE `ScriptName` IN ('RogueQuickCutScript', 'RogueShadowLungeScript', 'RogueRiposteScript');

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90010, 'RogueQuickCutScript'),
(90011, 'RogueShadowLungeScript'),
(90012, 'RogueRiposteScript');
