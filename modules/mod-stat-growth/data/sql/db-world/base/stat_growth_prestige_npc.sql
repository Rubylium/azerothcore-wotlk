-- The prestige keeper in Stormwind's Trade District (PrestigeSystem.cpp). Talking opens the frame.
-- Entry 900103 is also the mail sender for gear that did not fit in the bags, so the two stay the same.
--
-- Same ethereal as the paragon keeper (display 20986, scale 1, Spirit Particles): the two stand in the
-- same district and should read as a pair.

DELETE FROM `creature` WHERE `guid` = 9000012;
DELETE FROM `creature_template_addon` WHERE `entry` = 900103;
DELETE FROM `creature_template_locale` WHERE `entry` = 900103;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900103;
DELETE FROM `creature_template` WHERE `entry` = 900103;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_prestige_keeper`;
CREATE TEMPORARY TABLE `tmp_stat_growth_prestige_keeper` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_prestige_keeper`
SELECT * FROM `creature_template` WHERE `entry` = 328;
UPDATE `tmp_stat_growth_prestige_keeper` SET
    `entry` = 900103,
    `name` = 'Veylith',
    `subname` = 'Prestige Keeper',
    `gossip_menu_id` = 0,
    `npcflag` = 1,
    `faction` = 35,
    `AIName` = '',
    `ScriptName` = 'npc_stat_growth_prestige_keeper',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_prestige_keeper`;
DROP TEMPORARY TABLE `tmp_stat_growth_prestige_keeper`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (900103, 0, 20986, 1, 1, NULL);

INSERT INTO `creature_template_addon`
    (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
VALUES
    (900103, 0, 0, 0, 1, 0, 0, '17327');

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (900103, 'frFR', 'Veylith', 'Gardien du prestige', NULL);

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000012, 900103, 0, 0, 0, 1, 1, 0, -8821.023, 626.739, 93.833, 3.3992, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Stormwind Trade District prestige keeper');
