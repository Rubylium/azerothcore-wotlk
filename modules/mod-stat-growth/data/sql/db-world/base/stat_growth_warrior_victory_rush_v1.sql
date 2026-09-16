-- Victory Rush heals for 20% of maximum health on hit; the core spell_warr_victory_rush script stays attached
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'WarriorVictoryRushHealScript';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(34428, 'WarriorVictoryRushHealScript');
