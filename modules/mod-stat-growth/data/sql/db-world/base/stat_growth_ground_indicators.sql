-- The owner of a red ground indicator (GroundIndicators.cpp): an invisible stalker, scaled to the ability's size,
-- wearing the hidden aura that draws the red shape at its feet (spells 90700-90728; a circle that follows someone is
-- an aura on them instead).
--
-- A copy of the stock Invisible Stalker (All Phases), 32780: an invisible model (display 11686), not selectable, no
-- AI, a trigger. It walks on the ground rather than floating so that one following a target keeps to the floor, and
-- runs fast enough to keep up with a player.

DELETE FROM `creature_template_movement` WHERE `CreatureId` = 900104;
DELETE FROM `creature_template_locale` WHERE `entry` = 900104;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900104;
DELETE FROM `creature_template` WHERE `entry` = 900104;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_ground_indicator`;
CREATE TEMPORARY TABLE `tmp_stat_growth_ground_indicator` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_ground_indicator`
SELECT * FROM `creature_template` WHERE `entry` = 32780;
UPDATE `tmp_stat_growth_ground_indicator` SET
    `entry` = 900104,
    `name` = 'Danger Zone',
    `subname` = '',
    `speed_run` = 2.5,
    `AIName` = '',
    `ScriptName` = '',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_ground_indicator`;
DROP TEMPORARY TABLE `tmp_stat_growth_ground_indicator`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (900104, 0, 11686, 1, 1, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (900104, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (900104, 'frFR', 'Zone de danger', NULL, NULL);
