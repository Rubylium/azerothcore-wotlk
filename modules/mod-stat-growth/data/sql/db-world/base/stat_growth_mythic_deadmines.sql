-- The Deadmines' bosses fight with abilities drawn in red before they land (src/mythic/Deadmines.cpp).
-- Rhahk'Zor, Gilnid and Edwin VanCleef keep their SmartAI (yells, doors, VanCleef's allies) under a script that adds
-- the new abilities; Mr. Smite's script replaces boss_mr_smite; Captain Greenskin's script replaces his SmartAI (its
-- Cleave and Poisoned Harpoon become the telegraphed Cleave and Harpoon).
UPDATE `creature_template` SET `ScriptName` = 'boss_rhahkzor_evolutions' WHERE `entry` = 644;
UPDATE `creature_template` SET `ScriptName` = 'boss_gilnid_evolutions' WHERE `entry` = 1763;
UPDATE `creature_template` SET `ScriptName` = 'boss_mr_smite_evolutions' WHERE `entry` = 646;
UPDATE `creature_template` SET `ScriptName` = 'boss_edwin_vancleef_evolutions' WHERE `entry` = 639;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'boss_captain_greenskin_evolutions' WHERE `entry` = 647;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 647 AND `source_type` = 0;

-- Pierce Armor (Defias Miners, Strip Miners, Miner Johnson) cuts less armour in a Mythic+ key
DELETE FROM `spell_script_names` WHERE `spell_id` IN (6016, 12097) AND `ScriptName` = 'spell_mythic_deadmines_pierce_armor';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(6016, 'spell_mythic_deadmines_pierce_armor'),
(12097, 'spell_mythic_deadmines_pierce_armor');
