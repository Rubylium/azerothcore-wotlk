-- Valérian, le Zélote: the Hellfire Gaol's third gate, in the Blood Furnace (ZealotBoss.cpp; the board holds it
-- from mod-playerbots ChallengeBoard.cpp, 8 players - 2 tanks, 2 healers, 4 damage dealers - as the Gaol's other gates;
-- plan: .agents/plans/gaol-third). A fanatic of the Light locked in the Gaol for years; his fight runs on his music, in
-- a walled arena in Keli'dan the Breaker's hall - Vrogar's, the second gate's.
-- 930500 Valérian: a Captured Scarlet Zealot's look (1931, display 11428: a human fanatic of the Light in his
--        order's robes, a prisoner), twice a man's size, until his own; a level 83
--        boss, a paladin (his power bar: the orbs of light he has taken in). His health is set by his script from the
--        power model when his instance is known to be a challenge's.
-- 930501 Lumière captive: an orb of light drifting to him (Concentrated Light's look, 34630, display 29352), summoned
--        by his script; never selected, touched by walking into it.
-- 930503 Lumière à prendre: a light soak - retail's holy soak telegraph (display 60020, added by
--        localTools/patchSinisterStrike.ps1 from localTools/valerian/ascensionVisuals.json), a copy of the stock
--        Invisible Stalker (32780), never selected; its script plays the model's looks by emotes.
-- 930502 Geôlier: a jailer of the Gaol (a Shattered Hand Savage's look, as Vorhan's prisoners: 16523, display 16584),
--        walking in to chain him when the light was stolen from him.
-- His static spawn stands where Keli'dan channels, as Vrogar's does, in every Blood Furnace; his script hides him and
-- removes him from any instance that is not a challenge against him (Vrogar's own leaves his), and clears the room.

DELETE FROM `creature` WHERE `guid` = 9000700;
DELETE FROM `creature_equip_template` WHERE `CreatureID` = 930500;
DELETE FROM `creature_text_locale` WHERE `CreatureID` IN (930500, 930502);
DELETE FROM `creature_text` WHERE `CreatureID` IN (930500, 930502);
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 930500 AND 930503;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 930500 AND 930503;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 930500 AND 930503;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 930500 AND 930503;
DELETE FROM `creature_model_info` WHERE `DisplayID` = 60020;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_zealot`;
CREATE TEMPORARY TABLE `tmp_stat_growth_zealot` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_zealot`
SELECT * FROM `creature_template` WHERE `entry` = 5680;
UPDATE `tmp_stat_growth_zealot` SET
    `entry` = 930500,
    `difficulty_entry_1` = 0,
    `name` = 'Valerian',
    `subname` = 'the Zealot',
    `minlevel` = 83,
    `maxlevel` = 83,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 2,
    `rank` = 3,
    `type_flags` = `type_flags` | 8,    -- never his wound animation (a crit flinched him in a loop)
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
    `ScriptName` = 'boss_valerian',
    `VerifiedBuild` = NULL
WHERE `entry` = 5680;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_zealot`;
DROP TEMPORARY TABLE `tmp_stat_growth_zealot`;

-- The orbs: copies of the stock Invisible Stalker (32780) wearing the light's look, never selected
-- (unit_flags 33554434), no AI; they fly a little over the floor and drift slowly
DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_zealot_orb`;
CREATE TEMPORARY TABLE `tmp_stat_growth_zealot_orb` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_zealot_orb` SELECT * FROM `creature_template` WHERE `entry` = 32780;
UPDATE `tmp_stat_growth_zealot_orb` SET `entry` = 930501, `name` = 'Captive Light', `subname` = '', `npcflag` = 0,
    `unit_flags` = 33554434, `flags_extra` = 0, `speed_walk` = 1, `speed_run` = 1, `AIName` = '', `ScriptName` = '',
    `VerifiedBuild` = NULL WHERE `entry` = 32780;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_zealot_orb`;
UPDATE `tmp_stat_growth_zealot_orb` SET `entry` = 930503, `name` = 'Light to Take' WHERE `entry` = 930501;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_zealot_orb`;
DROP TEMPORARY TABLE `tmp_stat_growth_zealot_orb`;

-- The jailers: Vorhan's prisoners' look, friendly to no one, never attacked (they only walk in)
DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_zealot_jailer`;
CREATE TEMPORARY TABLE `tmp_stat_growth_zealot_jailer` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_zealot_jailer` SELECT * FROM `creature_template` WHERE `entry` = 16523;
UPDATE `tmp_stat_growth_zealot_jailer` SET `entry` = 930502, `difficulty_entry_1` = 0, `name` = 'Gaol Jailer',
    `subname` = '', `minlevel` = 82, `maxlevel` = 82, `faction` = 35, `npcflag` = 0, `unit_flags` = 33554434,
    `lootid` = 0, `pickpocketloot` = 0, `skinloot` = 0, `mingold` = 0, `maxgold` = 0, `AIName` = '',
    `ScriptName` = '', `VerifiedBuild` = NULL WHERE `entry` = 16523;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_zealot_jailer`;
DROP TEMPORARY TABLE `tmp_stat_growth_zealot_jailer`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930500, 0, 11428, 2.6, 1, NULL),
    (930501, 0, 29352, 0.6, 1, NULL),
    (930502, 0, 16584, 1.1, 1, NULL),
    (930503, 0, 60020, 1, 1, NULL);

INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`,
    `VerifiedBuild`)
VALUES
    (60020, 1.0, 1, 2, 0, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930500, 1, 0, 0, 0, 0, 0, NULL),
    (930501, 0, 0, 1, 0, 0, 0, NULL),
    (930502, 1, 0, 0, 0, 0, 0, NULL),
    (930503, 1, 0, 0, 1, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930500, 'frFR', 'Valérian', 'le Zélote', NULL),
    (930501, 'frFR', 'Lumière captive', '', NULL),
    (930502, 'frFR', 'Geôlier', '', NULL),
    (930503, 'frFR', 'Lumière à prendre', '', NULL);

-- Type 14: yell, lore only; type 12: say (a jailer's line). TextRange 3: the whole map.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930500, 0, 0, 'Years in the dark. Give me the light - all of it!', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - aggro'),
    (930500, 1, 0, 'Unworthy.', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - kill'),
    (930500, 2, 0, 'No... it was mine... MINE!', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - the light stolen'),
    (930500, 4, 0, 'At last... the Light answers!', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - ascension'),
    (930500, 6, 0, 'The Light... leaves me...', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - death'),
    (930500, 7, 0, 'Every chain... every lash... every year in the dark. I kept a spark. Only a spark.', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - prelude'),
    (930500, 8, 0, 'And you brought me the rest. All of it. Do you feel it? It is coming home!', 14, 0, 100, 0, 0, 0, 0, 3, 'Valerian - prelude'),
    (930502, 0, 0, 'Back in your cell, zealot.', 12, 0, 100, 0, 0, 0, 0, 3, 'Gaol Jailer - Valerian chained');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930500, 0, 0, 'frFR', 'Des années dans le noir. Donnez-moi la lumière - toute la lumière !'),
    (930500, 1, 0, 'frFR', 'Indigne.'),
    (930500, 2, 0, 'frFR', 'Non... elle était à moi... À MOI !'),
    (930500, 4, 0, 'frFR', 'Enfin... la Lumière répond !'),
    (930500, 6, 0, 'frFR', 'La Lumière... me quitte...'),
    (930500, 7, 0, 'frFR', 'Chaque chaîne... chaque coup de fouet... chaque année dans le noir. J''ai gardé une étincelle. Rien qu''une étincelle.'),
    (930500, 8, 0, 'frFR', 'Et vous m''avez apporté le reste. Tout le reste. Vous la sentez ? Elle rentre chez elle !'),
    (930502, 0, 0, 'frFR', 'Retour en cellule, fanatique.');

-- Where Keli'dan the Breaker channels, in the Blood Furnace's normal mode (spawn mask 1)
INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000700, 930500, 542, 0, 0, 1, 1, 0, 326.5, -86.0, -24.6, 0, 604800, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'Valerian - the Blood Furnace (Défi board only)');
