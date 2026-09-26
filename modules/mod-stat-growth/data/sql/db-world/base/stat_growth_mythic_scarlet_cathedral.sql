-- The Cathedral's bosses fight with abilities drawn in red before they land (src/mythic/ScarletCathedral.cpp). They
-- keep their SmartAI (Mograine's fall and resurrection, Fairbanks' gossip) under a script that adds the new abilities.
UPDATE `creature_template` SET `ScriptName` = 'boss_high_inquisitor_fairbanks_evolutions' WHERE `entry` = 4542;
UPDATE `creature_template` SET `ScriptName` = 'boss_scarlet_commander_mograine_evolutions' WHERE `entry` = 3976;
UPDATE `creature_template` SET `ScriptName` = 'boss_high_inquisitor_whitemane_evolutions' WHERE `entry` = 3977;

-- Holy Strike (Scarlet Champions) hits half as hard in a Mythic+ key
DELETE FROM `spell_script_names` WHERE `spell_id` = 17143 AND `ScriptName` = 'spell_mythic_cathedral_holy_strike';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(17143, 'spell_mythic_cathedral_holy_strike');

-- Scarlet Commander Mograine: the call of the chapel ("In Combat - Set Instance Data 1 to 1", id 2) is made by his
-- script, which brings only the Scarlets within 30 yards in a key. The rest is his block as of 2026_07_25_01.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 3976 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(3976, 0, 0, 0, 38, 0, 100, 0, 0, 1, 0, 0, 0, 0, 1, 6, 0, 1, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Data Set 0 1 - Say Line 6'),
(3976, 0, 1, 0, 0, 0, 100, 1, 0, 0, 0, 0, 0, 0, 11, 8990, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - In Combat - Cast \'Retribution Aura\' (No Repeat)'),
(3976, 0, 3, 0, 0, 0, 100, 0, 1000, 5000, 10000, 10000, 0, 0, 11, 14518, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - In Combat - Cast \'Crusader Strike\''),
(3976, 0, 4, 0, 0, 0, 100, 0, 6000, 11000, 60000, 60000, 0, 0, 11, 5589, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - In Combat - Cast \'Hammer of Justice\''),
(3976, 0, 5, 6, 11, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Respawn - Set Active Off'),
(3976, 0, 6, 7, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 19, 33554432, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Respawn - Remove Flags Not Selectable'),
(3976, 0, 7, 8, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Respawn - Set Reactstate Aggressive'),
(3976, 0, 8, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 91, 7, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Respawn - Remove FlagStandstate Dead'),
(3976, 0, 9, 10, 4, 0, 100, 0, 0, 0, 0, 0, 0, 0, 42, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Aggro - Set Invincibility Hp 1'),
(3976, 0, 10, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Aggro - Say Line 0'),
(3976, 0, 11, 12, 7, 0, 100, 0, 0, 0, 0, 0, 0, 0, 212, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Evade - Stop motion (StopMoving: 0, MovementExpired: 0)'),
(3976, 0, 12, 13, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 96, 32, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Evade - Remove Dynamic Flags Dead'),
(3976, 0, 13, 14, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 34, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Evade - Set Instance Data 1 to 2'),
(3976, 0, 14, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 19, 33554432, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Evade - Remove Flags Not Selectable'),
(3976, 0, 15, 0, 5, 0, 100, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Killed Unit - Say Line 1'),
(3976, 0, 16, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 0, 34, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Just Died - Set Instance Data 1 to 3'),
(3976, 0, 17, 0, 2, 0, 100, 1, 0, 1, 0, 0, 0, 0, 80, 397600, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - Between 0-1% Health - Run Script (No Repeat)'),
(3976, 0, 18, 0, 8, 1, 100, 512, 9232, 0, 0, 0, 0, 0, 80, 397601, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Spellhit \'Scarlet Resurrection\' - Run Script (Phase 1)'),
(3976, 0, 19, 20, 34, 0, 100, 0, 8, 1, 0, 0, 0, 0, 42, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Reset Invincibility Hp'),
(3976, 0, 20, 21, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 19, 33554432, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Remove Flags Not Selectable'),
(3976, 0, 21, 22, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Set Reactstate Aggressive'),
(3976, 0, 22, 23, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 11, 8990, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Cast \'Retribution Aura\''),
(3976, 0, 23, 24, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 20, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Start Attacking'),
(3976, 0, 24, 28, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Start Attacking'),
(3976, 0, 25, 0, 8, 0, 100, 1, 28441, 0, 0, 0, 0, 0, 80, 397602, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Spellhit \'AB Effect 000\' - Run Script (No Repeat)'),
(3976, 0, 26, 0, 8, 0, 100, 1, 28441, 0, 0, 0, 0, 0, 67, 1, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Spellhit \'AB Effect 000\' - Create Timed Event (No Repeat)'),
(3976, 0, 27, 0, 59, 0, 100, 0, 1, 0, 0, 0, 0, 0, 12, 16062, 8, 0, 0, 0, 0, 8, 0, 0, 0, 0, 1033.46, 1399.1, 27.3374, 6.25796, 'Scarlet Commander Mograine - On Timed Event 1 Triggered - Summon Creature \'Highlord Mograine\''),
(3976, 0, 28, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 117, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Commander Mograine - On Reached Point 1 - Enable Evade');

-- High Inquisitor Whitemane: Dominate Mind (id 7) and Deep Sleep (actionlist 397700, id 3) are made by her script,
-- drawn before they land (the 100-yard sleep came without warning, the mind control on a random player).
DELETE FROM `smart_scripts` WHERE `entryorguid` = 3977 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(3977, 0, 0, 1, 38, 0, 100, 0, 0, 1, 0, 0, 0, 0, 118, 2, 0, 0, 0, 0, 0, 14, 11877, 104600, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Data Set 0 1 - Set GO State To 2'),
(3977, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Data Set 0 1 - Say Line 0'),
(3977, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 1, 0, 0, 0, 0, 0, 10, 40029, 3976, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Data Set 0 1 - Move To Closest Creature \'Scarlet Commander Mograine\''),
(3977, 0, 3, 4, 4, 0, 100, 0, 0, 0, 0, 0, 0, 0, 42, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Aggro - Set Invincibility Hp 1'),
(3977, 0, 4, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Aggro - Set Event Phase 1'),
(3977, 0, 5, 0, 0, 3, 100, 0, 1000, 1000, 2600, 4000, 0, 0, 11, 9481, 64, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - In Combat - Cast \'Holy Smite\' (Phases 1 & 2)'),
(3977, 0, 6, 0, 16, 3, 100, 0, 22187, 40, 5000, 10000, 1, 0, 11, 22187, 32, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Friendly Unit Missing Buff \'Power Word: Shield\' - Cast \'Power Word: Shield\' (Phases 1 & 2)'),
(3977, 0, 8, 0, 74, 2, 100, 0, 5000, 15000, 5000, 15000, 75, 40, 11, 12039, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Friendly Below 75% Health - Cast \'Heal\' (Phase 2)'),
(3977, 0, 9, 0, 5, 0, 100, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Killed Unit - Say Line 1'),
(3977, 0, 10, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 0, 118, 1, 0, 0, 0, 0, 0, 14, 11877, 104600, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Respawn - Set GO State To 1'),
(3977, 0, 11, 12, 7, 0, 100, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Evade - Set Reactstate Aggressive'),
(3977, 0, 12, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 34, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Evade - Set Instance Data 1 to 2'),
(3977, 0, 13, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 0, 34, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Just Died - Set Instance Data 1 to 3'),
(3977, 0, 14, 15, 2, 0, 100, 513, 0, 50, 0, 0, 0, 0, 80, 397700, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - Between 0-50% Health - Run Script (No Repeat)'),
(3977, 0, 15, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - Between 0-50% Health - Set Event Phase 1 (No Repeat)'),
(3977, 0, 16, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 0, 67, 1, 200, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Data Set 1 1 - Create Timed Event'),
(3977, 0, 17, 0, 59, 0, 100, 0, 1, 0, 0, 0, 0, 0, 80, 397701, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Timed Event 1 Triggered - Run Script'),
(3977, 0, 18, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 0, 69, 2, 0, 0, 2, 0, 0, 10, 40029, 3976, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Data Set 1 1 - Move To Closest Creature \'Scarlet Commander Mograine\''),
(3977, 0, 19, 0, 34, 0, 100, 512, 8, 2, 0, 0, 0, 0, 80, 397701, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - On Reached Point 2 - Run Script');
DELETE FROM `smart_scripts` WHERE `entryorguid` = 397700 AND `source_type` = 9;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(397700, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 224, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - Actionlist - Stop Attack'),
(397700, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 22, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - Actionlist - Set Event Phase 0'),
(397700, 9, 2, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - Actionlist - Set Reactstate Passive'),
(397700, 9, 4, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'After 0 seconds - Self: Set data[1] to 1'),
(397700, 9, 5, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 117, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'High Inquisitor Whitemane - Actionlist - Disable Evade');

-- Scarlet Sorcerer: its "Blizzard" event cast Frostbolt; it casts Blizzard, a pool to step out of.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 4294 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(4294, 0, 0, 0, 4, 0, 20, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Sorcerer - On Aggro - Say Line 1'),
(4294, 0, 1, 0, 0, 0, 100, 0, 4000, 8000, 15000, 25000, 0, 0, 11, 6146, 0, 0, 0, 0, 0, 5, 30, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Sorcerer - In Combat - Cast Slow'),
(4294, 0, 2, 0, 0, 0, 100, 0, 0, 1000, 3000, 3500, 0, 0, 11, 9672, 64, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Sorcerer - In Combat - Cast Frostbolt'),
(4294, 0, 3, 0, 0, 0, 100, 0, 14000, 29000, 19000, 28000, 0, 0, 11, 8364, 0, 0, 0, 0, 0, 5, 30, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Sorcerer - In Combat - Cast Blizzard'),
(4294, 0, 4, 0, 2, 0, 100, 1, 0, 15, 0, 0, 0, 0, 25, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Sorcerer - Between 0-15% Health - Flee For Assist'),
(4294, 0, 5, 0, 8, 0, 100, 768, 28441, 0, 45000, 45000, 0, 0, 80, 429400, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Scarlet Sorcerer - spellhit_target - AshbringerEvent');
