-- Three permanent adaptive AoE test dummies beside Rubyrogue's Stormwind position on 2026-09-17.
-- The template inherits the Grandmaster dummy model/flags and uses module AI for player-level scaling.

DELETE FROM `creature` WHERE `guid` IN (9000001, 9000002, 9000003);
DELETE FROM `creature_template` WHERE `entry` = 900100;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_adaptive_dummy`;
CREATE TEMPORARY TABLE `tmp_stat_growth_adaptive_dummy` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_adaptive_dummy`
SELECT * FROM `creature_template` WHERE `entry` = 31144;
UPDATE `tmp_stat_growth_adaptive_dummy` SET
    `entry` = 900100,
    `name` = 'Adaptive AoE Training Dummy',
    `subname` = 'Matches nearby player level',
    `minlevel` = 46,
    `maxlevel` = 46,
    `ScriptName` = 'npc_adaptive_training_dummy',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_adaptive_dummy`;
DROP TEMPORARY TABLE `tmp_stat_growth_adaptive_dummy`;

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000001, 900100, 0, 0, 0, 1, 1, 0, -8820.32, 612.73, 94.6244, 2.38015, 30, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0, 'Rubyrogue adaptive AoE dummy 1'),
    (9000002, 900100, 0, 0, 0, 1, 1, 0, -8818.53, 614.62, 94.6244, 2.38015, 30, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0, 'Rubyrogue adaptive AoE dummy 2'),
    (9000003, 900100, 0, 0, 0, 1, 1, 0, -8816.74, 616.50, 94.6244, 2.38015, 30, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0, 'Rubyrogue adaptive AoE dummy 3');
