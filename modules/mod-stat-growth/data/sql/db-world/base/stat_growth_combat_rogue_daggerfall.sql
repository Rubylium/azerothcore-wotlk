-- Crimson Daggerfall: level-scaling AoE finisher with per-target dagger visuals and sound.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'CombatRogueCrimsonDaggerfallScript';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90105, 'CombatRogueCrimsonDaggerfallScript');
