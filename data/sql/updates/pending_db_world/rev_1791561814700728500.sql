-- Gardien-chef Vorhan's third phase, "Exécution des peines" (mod-stat-growth WardenVorhan.cpp).
-- 930203 Cachot: the cage Mise au cachot locks a player in. An invisible stalker's look (11686) wearing the cell's
--        cage (spell 94451); targetable, its health set by the script (a few seconds of the group's damage). Built
--        from the riot's prisoner (930201), it neither moves nor fights.
-- His yell as the phase begins (creature_text group 7, SAY_PHASE_3).

DELETE FROM `creature_template_locale` WHERE `entry` = 930203;
DELETE FROM `creature_template_movement` WHERE `CreatureId` = 930203;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 930203;
DELETE FROM `creature_template` WHERE `entry` = 930203;

DROP TEMPORARY TABLE IF EXISTS `tmp_warden_vorhan_cage`;
CREATE TEMPORARY TABLE `tmp_warden_vorhan_cage` LIKE `creature_template`;
INSERT INTO `tmp_warden_vorhan_cage` SELECT * FROM `creature_template` WHERE `entry` = 930201;
UPDATE `tmp_warden_vorhan_cage` SET
    `entry` = 930203,
    `name` = 'Solitary Cage',
    `subname` = '',
    `unit_class` = 1,
    `rank` = 1,
    `DamageModifier` = 0,
    `HealthModifier` = 20,
    `RegenHealth` = 0,
    -- Civilian (never aggroes), no parry, no block, no experience
    `flags_extra` = 0x00000002 | 0x00000004 | 0x00000010 | 0x00000040,
    `AIName` = '',
    `ScriptName` = 'npc_warden_vorhan_cage',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_warden_vorhan_cage`;
DROP TEMPORARY TABLE `tmp_warden_vorhan_cage`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930203, 0, 11686, 1, 1, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930203, 1, 0, 0, 1, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930203, 'frFR', 'Cachot', NULL, NULL);

DELETE FROM `creature_text_locale` WHERE `CreatureID` = 930200 AND `GroupID` = 7;
DELETE FROM `creature_text` WHERE `CreatureID` = 930200 AND `GroupID` = 7;
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930200, 7, 0, 'The trial is over. Now the sentences are carried out.', 14, 0, 100, 0, 0, 0, 0, 3,
     'Vorhan - phase 3');
INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930200, 7, 0, 'frFR', 'Le procès est terminé. Place à l''exécution des peines.');
