-- Drak'Tharon Keep in Mythic+ (src/mythic/DrakTharonKeep.cpp): Trollgore draws his Corpse Explode in red and, in a
-- key, only players feed his Consume stacks; Novos lays no Arcane Field in a key.
UPDATE `creature_template` SET `ScriptName` = 'boss_trollgore_evolutions' WHERE `entry` = 26630;
DELETE FROM `spell_script_names` WHERE `spell_id` = 47346 AND `ScriptName` = 'spell_novos_arcane_field_evolutions';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (47346, 'spell_novos_arcane_field_evolutions');
