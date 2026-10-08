-- Gardien-chef Vorhan, the Défi board's prison warden in Magtheridon's Lair (WardenVorhan.cpp; the board holds it from
-- mod-playerbots ChallengeBoard.cpp, 8 players - 2 tanks, 2 healers, 4 damage dealers - for item level 450 and 600
-- paragon, ChallengeTiers.h; plan: .agents/plans/warden-vorhan). The first boss of la Geôle des Flammes infernales.
-- 930200 Gardien-chef Vorhan: Warchief Kargath Bladefist's look (16808, display 19799), a level 83 boss. His health is
--        set by his script from the power model when his instance is known to be a challenge's (the DPS check of the
--        profile's group, PowerScaling.h DpsCheckHealth): the modifier here is only what stands before that.
-- 930201 Prisonnier de la Geôle: a Shattered Hand Savage's look (16523, display 16584), the riot's fel orcs.
-- 930202 Abyssal enchaîné: a Burning Abyssal's look (17454, display 16874), from Magtheridon's own fight.
--        Both are sized by the script from the profile's group (an area damage moment).
-- The warden's static spawn stands where Magtheridon is chained, in every Magtheridon's Lair; his script hides him
-- and removes him from any instance that is not a challenge's, and clears the lair of its own occupants.

DELETE FROM `creature` WHERE `guid` = 9000501;
DELETE FROM `creature_equip_template` WHERE `CreatureID` = 930200;
DELETE FROM `creature_text_locale` WHERE `CreatureID` = 930200;
DELETE FROM `creature_text` WHERE `CreatureID` = 930200;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 930200 AND 930202;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 930200 AND 930202;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 930200 AND 930202;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 930200 AND 930202;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_warden_vorhan`;
CREATE TEMPORARY TABLE `tmp_stat_growth_warden_vorhan` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_warden_vorhan`
SELECT * FROM `creature_template` WHERE `entry` IN (16808, 16523, 17454);
UPDATE `tmp_stat_growth_warden_vorhan` SET
    `entry` = 930200,
    `difficulty_entry_1` = 0,
    `name` = 'Head Warden Vorhan',
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
    `ScriptName` = 'boss_warden_vorhan',
    `VerifiedBuild` = NULL
WHERE `entry` = 16808;
UPDATE `tmp_stat_growth_warden_vorhan` SET
    `entry` = 930201,
    `difficulty_entry_1` = 0,
    `name` = 'Gaol Prisoner',
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
    `ScriptName` = 'npc_warden_vorhan_prisoner',
    `VerifiedBuild` = NULL
WHERE `entry` = 16523;
UPDATE `tmp_stat_growth_warden_vorhan` SET
    `entry` = 930202,
    `difficulty_entry_1` = 0,
    `name` = 'Chained Abyssal',
    `subname` = '',
    `minlevel` = 82,
    `maxlevel` = 82,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 1,
    `npcflag` = 0,
    `unit_flags` = 0,
    `DamageModifier` = 30,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 40,
    `RegenHealth` = 0,
    `flags_extra` = 0,
    `gossip_menu_id` = 0,
    `lootid` = 0,
    `pickpocketloot` = 0,
    `skinloot` = 0,
    `mingold` = 0,
    `maxgold` = 0,
    `AIName` = '',
    `ScriptName` = 'npc_warden_vorhan_prisoner',
    `VerifiedBuild` = NULL
WHERE `entry` = 17454;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_warden_vorhan`;
DROP TEMPORARY TABLE `tmp_stat_growth_warden_vorhan`;

-- Kargath's build, a head taller: the warden towers over his prisoners
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930200, 0, 19799, 1.5, 1, NULL),
    (930201, 0, 16584, 1, 1, NULL),
    (930202, 0, 16874, 1, 1, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930200, 1, 0, 0, 0, 0, 0, NULL),
    (930201, 1, 0, 0, 0, 0, 0, NULL),
    (930202, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930200, 'frFR', 'Gardien-chef Vorhan', 'Geôle des Flammes infernales', NULL),
    (930201, 'frFR', 'Prisonnier de la Geôle', NULL, NULL),
    (930202, 'frFR', 'Abyssal enchaîné', NULL, NULL);

-- Type 14: yell, lore only (the rules show on the cast bar and the debuffs, never announced on screen). TextRange 3:
-- the whole map.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930200, 0, 0, 'New inmates. You will learn the rules, or you will learn the pit.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - aggro'),
    (930200, 1, 0, 'One less mouth to feed.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - kill'),
    (930200, 2, 0, 'The doors! Who opened the doors?! Back in your cells, vermin!', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - riot'),
    (930200, 3, 0, 'Enough leniency. From now on, every rule at once.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - phase 2'),
    (930200, 4, 0, 'Your sentence is life. However short that is.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - life sentence'),
    (930200, 5, 0, 'Time is up. Capital punishment.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - hard enrage'),
    (930200, 6, 0, 'The keys... the deeper cells... you have no idea what you just set free...', 14, 0, 100, 0, 0, 0, 0, 3, 'Vorhan - death');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930200, 0, 0, 'frFR', 'De nouveaux détenus. Vous apprendrez le règlement, ou vous apprendrez la fosse.'),
    (930200, 1, 0, 'frFR', 'Une bouche de moins à nourrir.'),
    (930200, 2, 0, 'frFR', 'Les portes ! Qui a ouvert les portes ?! Dans vos cellules, vermine !'),
    (930200, 3, 0, 'frFR', 'Assez d''indulgence. Désormais, toutes les règles à la fois.'),
    (930200, 4, 0, 'frFR', 'Votre peine est à perpétuité. Aussi courte soit-elle.'),
    (930200, 5, 0, 'frFR', 'Le temps est écoulé. Peine capitale.'),
    (930200, 6, 0, 'frFR', 'Les clés... les cellules profondes... vous ne savez pas ce que vous venez de libérer...');

-- Where Magtheridon is chained, in the lair's middle, in its only mode (spawn mask 1); Kargath's look has his blades
-- in its model (no equipment)
INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000501, 930200, 544, 0, 0, 1, 1, 0, -18.7, 2.2, -0.3, 0, 604800, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'Gardien-chef Vorhan - Magtheridon''s Lair (Défi board only)');
