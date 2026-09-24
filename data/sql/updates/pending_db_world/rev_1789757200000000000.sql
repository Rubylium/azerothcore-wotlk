-- Native Oathblade spell bindings.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('OathbladeAbilitySpellScript', 'OathbladeBleedAuraScript');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90800, 'OathbladeAbilitySpellScript'),
(90802, 'OathbladeAbilitySpellScript'),
(90803, 'OathbladeAbilitySpellScript'),
(90804, 'OathbladeAbilitySpellScript'),
(90806, 'OathbladeAbilitySpellScript'),
(90807, 'OathbladeAbilitySpellScript'),
(90808, 'OathbladeAbilitySpellScript'),
(90809, 'OathbladeAbilitySpellScript'),
(90810, 'OathbladeAbilitySpellScript'),
(90811, 'OathbladeAbilitySpellScript'),
(90812, 'OathbladeAbilitySpellScript'),
(90813, 'OathbladeAbilitySpellScript'),
(90942, 'OathbladeAbilitySpellScript'),
(90953, 'OathbladeAbilitySpellScript'),
(90830, 'OathbladeBleedAuraScript');
