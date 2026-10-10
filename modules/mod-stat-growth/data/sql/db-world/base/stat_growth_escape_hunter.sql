-- Vrogar, Maître-fondeur de la Geôle: the Hellfire Gaol's second gate, in the Blood Furnace (ForgeMaster.cpp; the board
-- holds it from mod-playerbots ChallengeBoard.cpp, 8 players - 2 tanks, 2 healers, 4 damage dealers - as Gardien-chef
-- Vorhan; plan: .agents/plans/escape-hunter, "The fight v3"). His fight runs on his music, in a walled arena.
-- 930400 Vrogar: Goraluk Anvilcrack's look (10899, display 10222, the Blackrock Spire's orc blacksmith) and hammer,
--        until his own; a level 83 boss. His health is set by his script from the power model when his instance is
--        known to be a challenge's.
-- (930401-930403, the Traqueur d'évadés's pack and beaters, are gone with the hunt.)
-- His static spawn stands where Keli'dan the Breaker channels, in every Blood Furnace; his script hides him and removes
-- him from any instance that is not a challenge's, and clears the room of its own occupants.

DELETE FROM `creature` WHERE `guid` = 9000600;
DELETE FROM `creature_equip_template` WHERE `CreatureID` = 930400;
DELETE FROM `creature_text_locale` WHERE `CreatureID` = 930400;
DELETE FROM `creature_text` WHERE `CreatureID` = 930400;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 930400 AND 930403;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 930400 AND 930403;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 930400 AND 930403;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 930400 AND 930403;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_forge_master`;
CREATE TEMPORARY TABLE `tmp_stat_growth_forge_master` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_forge_master`
SELECT * FROM `creature_template` WHERE `entry` = 10899;
UPDATE `tmp_stat_growth_forge_master` SET
    `entry` = 930400,
    `difficulty_entry_1` = 0,
    `name` = 'Vrogar',
    `subname` = 'Forge-master of the Gaol',
    `minlevel` = 83,
    `maxlevel` = 83,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 3,
    `npcflag` = 0,
    `unit_flags` = 0,
    `DamageModifier` = 60,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 1000,
    `ManaModifier` = 1,
    `RegenHealth` = 0,
    `CreatureImmunitiesId` = -361,
    `flags_extra` = 1,
    `gossip_menu_id` = 0,
    `lootid` = 0,
    `pickpocketloot` = 0,
    `skinloot` = 0,
    `mingold` = 0,
    `maxgold` = 0,
    `AIName` = '',
    `ScriptName` = 'boss_forge_master',
    `VerifiedBuild` = NULL
WHERE `entry` = 10899;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_forge_master`;
DROP TEMPORARY TABLE `tmp_stat_growth_forge_master`;

-- The forge-master a head taller than a smith
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930400, 0, 10222, 1.6, 1, NULL);

-- Goraluk's hammer
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
SELECT 930400, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, NULL FROM `creature_equip_template` WHERE `CreatureID` = 10899;

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930400, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930400, 'frFR', 'Vrogar', 'Maître-fondeur de la Geôle', NULL);

-- Type 14: yell, lore only. TextRange 3: the whole map.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930400, 0, 0, 'Vorhan sends me more iron. Into the mould with you.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vrogar - aggro'),
    (930400, 1, 0, 'Slag.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vrogar - kill'),
    (930400, 6, 0, 'The forge... goes cold...', 14, 0, 100, 0, 0, 0, 0, 3, 'Vrogar - death');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930400, 0, 0, 'frFR', 'Vorhan m''envoie encore du fer. Au moule.'),
    (930400, 1, 0, 'frFR', 'Scorie.'),
    (930400, 6, 0, 'frFR', 'La forge... refroidit...');

-- Where Keli'dan the Breaker channels, in the Blood Furnace's normal mode (spawn mask 1)
INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000600, 930400, 542, 0, 0, 1, 1, 1, 326.5, -86.0, -24.6, 0, 604800, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'Vrogar - the Blood Furnace (Défi board only)');
