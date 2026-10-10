-- Le Traqueur d'évadés, the Hellfire Gaol's second gate, in the Blood Furnace (EscapeHunter.cpp; the board holds it from
-- mod-playerbots ChallengeBoard.cpp, 8 players - 2 tanks, 2 healers, 4 damage dealers - as Gardien-chef Vorhan;
-- plan: .agents/plans/escape-hunter). His fight runs on his music.
-- 930400 Le Traqueur d'évadés: a Shadowmoon Houndmaster's look (23018, display 21378), a level 83 boss. His health is
--        set by his script from the power model when his instance is known to be a challenge's.
-- 930401 Gangrechien de la Geôle: a Felhound Manastalker's look (17401, display 1913), his pack.
-- 930402 Rabatteur archer: a Bleeding Hollow Archer's look (17270, display 17050), his beaters' archers.
-- 930403 Rabatteur: a Shattered Hand Scout's look (17693, display 17727), the beaters of his Battue.
--        His pack and beaters are sized by the script from the profile's group.
-- The Traqueur's static spawn stands where Keli'dan the Breaker channels, in every Blood Furnace; his script hides him
-- and removes him from any instance that is not a challenge's, and clears the room of its own occupants.

DELETE FROM `creature` WHERE `guid` = 9000600;
DELETE FROM `creature_equip_template` WHERE `CreatureID` = 930400;
DELETE FROM `creature_text_locale` WHERE `CreatureID` = 930400;
DELETE FROM `creature_text` WHERE `CreatureID` = 930400;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 930400 AND 930403;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 930400 AND 930403;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 930400 AND 930403;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 930400 AND 930403;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_escape_hunter`;
CREATE TEMPORARY TABLE `tmp_stat_growth_escape_hunter` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_escape_hunter`
SELECT * FROM `creature_template` WHERE `entry` IN (23018, 17401, 17270, 17693);
UPDATE `tmp_stat_growth_escape_hunter` SET
    `entry` = 930400,
    `difficulty_entry_1` = 0,
    `name` = 'The Escape-Hunter',
    `subname` = 'Hellfire Gaol',
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
    `ScriptName` = 'boss_escape_hunter',
    `VerifiedBuild` = NULL
WHERE `entry` = 23018;
UPDATE `tmp_stat_growth_escape_hunter` SET
    `entry` = 930401,
    `difficulty_entry_1` = 0,
    `name` = 'Gaol Felhound',
    `subname` = '',
    `minlevel` = 82,
    `maxlevel` = 82,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 1,
    `npcflag` = 0,
    `unit_flags` = 0,
    `DamageModifier` = 20,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 20,
    `RegenHealth` = 0,
    `flags_extra` = 0,
    `gossip_menu_id` = 0,
    `lootid` = 0,
    `pickpocketloot` = 0,
    `skinloot` = 0,
    `mingold` = 0,
    `maxgold` = 0,
    `AIName` = '',
    `ScriptName` = '',
    `VerifiedBuild` = NULL
WHERE `entry` = 17401;
UPDATE `tmp_stat_growth_escape_hunter` SET
    `entry` = 930402,
    `difficulty_entry_1` = 0,
    `name` = 'Archer Beater',
    `subname` = '',
    `minlevel` = 82,
    `maxlevel` = 82,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 1,
    `npcflag` = 0,
    `unit_flags` = 0,
    `DamageModifier` = 20,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 15,
    `RegenHealth` = 0,
    `flags_extra` = 0,
    `gossip_menu_id` = 0,
    `lootid` = 0,
    `pickpocketloot` = 0,
    `skinloot` = 0,
    `mingold` = 0,
    `maxgold` = 0,
    `AIName` = '',
    `ScriptName` = '',
    `VerifiedBuild` = NULL
WHERE `entry` = 17270;
UPDATE `tmp_stat_growth_escape_hunter` SET
    `entry` = 930403,
    `difficulty_entry_1` = 0,
    `name` = 'Beater',
    `subname` = '',
    `minlevel` = 82,
    `maxlevel` = 82,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 1,
    `npcflag` = 0,
    `unit_flags` = 33554432,
    `DamageModifier` = 1,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 1,
    `RegenHealth` = 0,
    `flags_extra` = 2,
    `gossip_menu_id` = 0,
    `lootid` = 0,
    `pickpocketloot` = 0,
    `skinloot` = 0,
    `mingold` = 0,
    `maxgold` = 0,
    `AIName` = '',
    `ScriptName` = '',
    `VerifiedBuild` = NULL
WHERE `entry` = 17693;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_escape_hunter`;
DROP TEMPORARY TABLE `tmp_stat_growth_escape_hunter`;

-- The houndmaster a head taller than his beaters; the beaters of the Battue unselectable (unit flag 0x2000000), never
-- fighting (flags_extra 2: civilian): they are the sweep's look, the script deals its blows
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930400, 0, 21378, 1.4, 1, NULL),
    (930401, 0, 1913, 1, 1, NULL),
    (930402, 0, 17050, 1, 1, NULL),
    (930403, 0, 17727, 1, 1, NULL);

-- The houndmaster's own weapons: his one-hander and his bow
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
SELECT 930400, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, NULL FROM `creature_equip_template` WHERE `CreatureID` = 23018;

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930400, 1, 0, 0, 0, 0, 0, NULL),
    (930401, 1, 0, 0, 0, 0, 0, NULL),
    (930402, 1, 0, 0, 0, 0, 0, NULL),
    (930403, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930400, 'frFR', 'Le Traqueur d''évadés', 'Geôle des Flammes infernales', NULL),
    (930401, 'frFR', 'Gangrechien de la Geôle', NULL, NULL),
    (930402, 'frFR', 'Rabatteur archer', NULL, NULL),
    (930403, 'frFR', 'Rabatteur', NULL, NULL);

-- Type 14: yell, lore only. TextRange 3: the whole map.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930400, 0, 0, 'Vorhan let you through? Then you are escapees. And escapees are mine.', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - aggro'),
    (930400, 1, 0, 'Caught.', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - kill'),
    (930400, 2, 0, 'Sound the horn! Loose the pack!', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - horn'),
    (930400, 3, 0, 'Beaters! Drive them to me!', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - battue'),
    (930400, 4, 0, 'Douse the torches. The hunt begins.', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - hunt'),
    (930400, 5, 0, 'The quarry is cornered. Finish it!', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - curee'),
    (930400, 6, 0, 'No one... escapes... the Gaol...', 14, 0, 100, 0, 0, 0, 0, 3, 'Traqueur - death');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930400, 0, 0, 'frFR', 'Vorhan vous a laissés passer ? Alors vous êtes des évadés. Et les évadés sont à moi.'),
    (930400, 1, 0, 'frFR', 'Rattrapé.'),
    (930400, 2, 0, 'frFR', 'Sonnez le cor ! Lâchez la meute !'),
    (930400, 3, 0, 'frFR', 'Rabatteurs ! Rabattez-les vers moi !'),
    (930400, 4, 0, 'frFR', 'Éteignez les torches. La traque commence.'),
    (930400, 5, 0, 'frFR', 'La proie est aux abois. À la curée !'),
    (930400, 6, 0, 'frFR', 'Personne... ne s''évade... de la Geôle...');

-- Where Keli'dan the Breaker channels, in the Blood Furnace's normal mode (spawn mask 1)
INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000600, 930400, 542, 0, 0, 1, 1, 1, 326.5, -86.0, -24.6, 0, 604800, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'Le Traqueur d''évadés - the Blood Furnace (Défi board only)');
