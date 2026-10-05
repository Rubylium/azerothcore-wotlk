-- The ground loot (GroundLoot.cpp): what a boss throws out on the floor for each player, summoned by that player and
-- seen by them alone. 900120 a bag of loot, 900121 a pile of gold - friendly, out of combat, clicked (gossip) to be
-- picked up as walking over them does - and 900122 the light beam over either, never selected, whose display the
-- server sets to the drop's quality (60004-60009). Their models are imported from Ascension and their displays added
-- by localTools/patchSinisterStrike.ps1 (60002 the bag, 60003 the gold).
--
-- Copies of the stock Invisible Stalker (All Phases), 32780: no AI, standing on the ground; not a trigger (a trigger
-- would wear an invisible model).

DELETE FROM `creature_model_info` WHERE `DisplayID` BETWEEN 60002 AND 60009;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 900120 AND 900122;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 900120 AND 900122;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 900120 AND 900122;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 900120 AND 900122;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_ground_loot`;
CREATE TEMPORARY TABLE `tmp_stat_growth_ground_loot` LIKE `creature_template`;
-- Three copies, each renumbered before the next. unit_flags: 770 non attackable, immune to players and creatures;
-- 33554434 the same, never selected.
INSERT INTO `tmp_stat_growth_ground_loot` SELECT * FROM `creature_template` WHERE `entry` = 32780;
UPDATE `tmp_stat_growth_ground_loot` SET `entry` = 900120 WHERE `entry` = 32780;
INSERT INTO `tmp_stat_growth_ground_loot` SELECT * FROM `creature_template` WHERE `entry` = 32780;
UPDATE `tmp_stat_growth_ground_loot` SET `entry` = 900121 WHERE `entry` = 32780;
INSERT INTO `tmp_stat_growth_ground_loot` SELECT * FROM `creature_template` WHERE `entry` = 32780;
UPDATE `tmp_stat_growth_ground_loot` SET `entry` = 900122 WHERE `entry` = 32780;
UPDATE `tmp_stat_growth_ground_loot` SET
    `name` = 'Loot',
    `subname` = '',
    `npcflag` = 1,
    `unit_flags` = 770,
    `flags_extra` = 0,
    `ScriptName` = 'npc_ground_loot'
WHERE `entry` = 900120;
UPDATE `tmp_stat_growth_ground_loot` SET
    `name` = 'Gold',
    `subname` = '',
    `npcflag` = 1,
    `unit_flags` = 770,
    `flags_extra` = 0,
    `ScriptName` = 'npc_ground_loot'
WHERE `entry` = 900121;
UPDATE `tmp_stat_growth_ground_loot` SET
    `name` = 'Loot Light',
    `subname` = '',
    `npcflag` = 0,
    `unit_flags` = 33554434,
    `flags_extra` = 0,
    `ScriptName` = ''
WHERE `entry` = 900122;
UPDATE `tmp_stat_growth_ground_loot` SET `AIName` = '', `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_ground_loot`;
DROP TEMPORARY TABLE `tmp_stat_growth_ground_loot`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (900120, 0, 60002, 1, 1, NULL),
    (900121, 0, 60003, 1, 1, NULL),
    (900122, 0, 60004, 1, 1, NULL);

INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`,
    `VerifiedBuild`)
VALUES
    (60002, 0.5, 1, 2, 0, NULL),
    (60003, 0.5, 1, 2, 0, NULL),
    (60004, 0.3, 0.5, 2, 0, NULL),
    (60005, 0.3, 0.5, 2, 0, NULL),
    (60006, 0.3, 0.5, 2, 0, NULL),
    (60007, 0.3, 0.5, 2, 0, NULL),
    (60008, 0.3, 0.5, 2, 0, NULL),
    (60009, 0.3, 0.5, 2, 0, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (900120, 1, 0, 0, 0, 0, 0, NULL),
    (900121, 1, 0, 0, 0, 0, 0, NULL),
    (900122, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (900120, 'frFR', 'Butin', NULL, NULL),
    (900121, 'frFR', 'Or', NULL, NULL),
    (900122, 'frFR', 'Lueur du butin', NULL, NULL);
