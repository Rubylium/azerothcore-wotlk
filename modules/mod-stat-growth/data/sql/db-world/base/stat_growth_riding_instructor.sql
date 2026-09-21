-- A riding instructor in Stormwind's Trade District (RidingInstructor.cpp): anyone learns riding there for the stock
-- trainers' price, with a mount of that speed. Looks like the city's riding trainer, Randal Hunter (4732).

DELETE FROM `creature` WHERE `guid` = 9000010;
DELETE FROM `creature_template_locale` WHERE `entry` = 900101;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900101;
DELETE FROM `creature_template` WHERE `entry` = 900101;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_riding_instructor`;
CREATE TEMPORARY TABLE `tmp_stat_growth_riding_instructor` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_riding_instructor`
SELECT * FROM `creature_template` WHERE `entry` = 4732;
UPDATE `tmp_stat_growth_riding_instructor` SET
    `entry` = 900101,
    `name` = 'Tomas Brayden',
    `subname` = 'Riding Instructor',
    `gossip_menu_id` = 0,
    `npcflag` = 1,
    `faction` = 35,
    `AIName` = '',
    `ScriptName` = 'npc_stat_growth_riding_instructor',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_riding_instructor`;
DROP TEMPORARY TABLE `tmp_stat_growth_riding_instructor`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT 900101, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, NULL
FROM `creature_template_model`
WHERE `CreatureID` = 4732;

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (900101, 'frFR', 'Tomas Brayden', 'Instructeur de monte', NULL);

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000010, 900101, 0, 0, 0, 1, 1, 0, -8846.152, 626.574, 94.508, 0.45, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Stormwind Trade District riding instructor');
