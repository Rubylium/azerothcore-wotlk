-- The paragon keeper in Stormwind's Trade District (ParagonSystem.cpp): talking to them opens the board.
-- Looks like the city's archmage, Jennea Cannon (328), and offers nothing else.

DELETE FROM `creature` WHERE `guid` = 9000011;
DELETE FROM `creature_template_locale` WHERE `entry` = 900102;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900102;
DELETE FROM `creature_template` WHERE `entry` = 900102;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_paragon_keeper`;
CREATE TEMPORARY TABLE `tmp_stat_growth_paragon_keeper` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_paragon_keeper`
SELECT * FROM `creature_template` WHERE `entry` = 328;
UPDATE `tmp_stat_growth_paragon_keeper` SET
    `entry` = 900102,
    `name` = 'Aldessa Nightgale',
    `subname` = 'Paragon Keeper',
    `gossip_menu_id` = 0,
    `npcflag` = 1,
    `faction` = 35,
    `AIName` = '',
    `ScriptName` = 'npc_stat_growth_paragon_keeper',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_paragon_keeper`;
DROP TEMPORARY TABLE `tmp_stat_growth_paragon_keeper`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT 900102, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, NULL
FROM `creature_template_model`
WHERE `CreatureID` = 328;

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (900102, 'frFR', 'Aldessa Chantenuit', 'Gardienne du parangon', NULL);

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000011, 900102, 0, 0, 0, 1, 1, 0, -8850.140, 634.802, 98.221, 0.1731, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Stormwind Trade District paragon keeper');
