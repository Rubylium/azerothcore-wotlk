DELETE FROM `spell_script_names` WHERE `ScriptName` = 'WarriorGladiatorStanceScript';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90021, 'WarriorGladiatorStanceScript');

-- Keep Gladiator Stance a cancelable buff despite its damage taken increase
-- 0x06000000 = SPELL_ATTR0_CU_POSITIVE_EFF0 | SPELL_ATTR0_CU_POSITIVE_EFF1
DELETE FROM `spell_custom_attr` WHERE `spell_id` = 90021;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`) VALUES
(90021, 100663296);
