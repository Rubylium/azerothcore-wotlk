-- L'Infini, the Défi board's god fight in Ulduar's Celestial Planetarium (InfiniteGod.cpp; the board posts it from
-- mod-playerbots ChallengeBoard.cpp, 10 players). Every creature is a copy of a stock one of Algalon's fight:
-- 930000 L'Infini: Algalon the Observer (32871), hostile to all, no longer immune to players, its health set for a
--        timeline fight of 5 minutes at Défi I, tuned on item level 300 gear and 51 paragon (InfiniteGod.cpp):
--        3 102 x 13 945 (level 83 elite) = 43.3 million, the tier multiplying it: a tight damage check at the 5 minute
--        mark. Measured in game (2026-09-29): its players' damage dealers do 20 000-30 000 on one target, so six of
--        them at 25 000 and two tanks at 8 000, on the god 85% of the time with the movement it asks (+50% in
--        intermission 1's window), deal about 42 million by 4:50. The first estimate (7 500 a damage dealer, 11.0
--        million) died far too early; 42.0 million, then 3% more (a kill came a little early).
--        Its melee: a 2 s swing at 45 damage
--        modifier. Algalon's own chest loot (Gift of the Observer, 10 players) on its corpse.
-- 930001 Fragment d'éternité: a Living Constellation (33052) that walks to the god in intermission 1, 0.5 million
--        health (90 x 13 933): the raid kills both in the 20 s they walk, if it turns on them.
-- 930002 Étoile effondrée: a Collapsing Star (32955) to share, friendly (nothing attacks it), rooted.
-- 930003 Singularité: a Black Hole (32953), the look of the pull, friendly and rooted.
-- The god's static spawn stands in the middle of the Planetarium in every 10-player Ulduar; its script hides it and
-- removes it from any instance that is not a challenge's.

DELETE FROM `creature` WHERE `guid` = 9000400;
DELETE FROM `creature_text_locale` WHERE `CreatureID` = 930000;
DELETE FROM `creature_text` WHERE `CreatureID` = 930000;
DELETE FROM `creature_loot_template` WHERE `Entry` = 930000;
DELETE FROM `creature_model_info` WHERE `DisplayID` = 60001;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 930000 AND 930003;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 930000 AND 930003;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 930000 AND 930003;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 930000 AND 930003;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_infinite_god`;
CREATE TEMPORARY TABLE `tmp_stat_growth_infinite_god` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_infinite_god`
SELECT * FROM `creature_template` WHERE `entry` IN (32871, 33052, 32955, 32953);
UPDATE `tmp_stat_growth_infinite_god` SET
    `entry` = 930000,
    `difficulty_entry_1` = 0,
    `name` = 'The Infinite',
    `subname` = 'God of the Endless Stars',
    `minlevel` = 83,
    `maxlevel` = 83,
    `faction` = 14,
    `unit_flags` = `unit_flags` & ~(0x100 | 0x200),
    -- Not Algalon's hard reset (a despawn on evade): the board follows the god by its guid through the wipes
    `flags_extra` = `flags_extra` & ~0x80000000,
    `DamageModifier` = 45,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 3102,
    `lootid` = 930000,
    `AIName` = '',
    `ScriptName` = 'boss_infinite_god',
    `VerifiedBuild` = NULL
WHERE `entry` = 32871;
UPDATE `tmp_stat_growth_infinite_god` SET
    `entry` = 930001,
    `difficulty_entry_1` = 0,
    `name` = 'Fragment of Eternity',
    `subname` = '',
    `minlevel` = 83,
    `maxlevel` = 83,
    `faction` = 14,
    `speed_walk` = 0.8,
    `unit_flags` = 0,
    `HealthModifier` = 36,
    `lootid` = 0,
    `AIName` = '',
    `ScriptName` = '',
    `VerifiedBuild` = NULL
WHERE `entry` = 33052;
UPDATE `tmp_stat_growth_infinite_god` SET
    `entry` = 930002,
    `difficulty_entry_1` = 0,
    `name` = 'Collapsing Star',
    `subname` = '',
    `faction` = 35,
    `AIName` = 'NullCreatureAI',
    `ScriptName` = '',
    `VerifiedBuild` = NULL
WHERE `entry` = 32955;
UPDATE `tmp_stat_growth_infinite_god` SET
    `entry` = 930003,
    `difficulty_entry_1` = 0,
    `name` = 'Singularity',
    `subname` = '',
    `faction` = 35,
    `AIName` = 'NullCreatureAI',
    `ScriptName` = '',
    `VerifiedBuild` = NULL
WHERE `entry` = 32953;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_infinite_god`;
DROP TEMPORARY TABLE `tmp_stat_growth_infinite_god`;

-- The god wears Algalon's display (28641) until its own recoloured one (60001, localTools/infiniteBoss) is in the
-- DBCs: the script switches to it then. Its model info copies Algalon's.
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930000, 0, 28641, 1, 1, NULL),
    (930001, 0, 28741, 1, 1, NULL),
    (930002, 0, 28988, 1, 1, NULL),
    (930003, 0, 28460, 1, 1, NULL);

INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`,
    `VerifiedBuild`)
VALUES
    (60001, 0.93, 9, 0, 0, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930000, 1, 0, 0, 0, 0, 0, NULL),
    (930001, 1, 0, 0, 0, 0, 0, NULL),
    (930002, 1, 0, 0, 1, 0, 0, NULL),
    (930003, 1, 0, 0, 1, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930000, 'frFR', 'L''Infini', 'Dieu des étoiles sans fin', NULL),
    (930001, 'frFR', 'Fragment d''éternité', NULL, NULL),
    (930002, 'frFR', 'Étoile effondrée', NULL, NULL),
    (930003, 'frFR', 'Singularité', NULL, NULL);

-- Algalon's chest loot (Gift of the Observer, 10 players: gameobject loot 27030), on the god's corpse
INSERT INTO `creature_loot_template`
    (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT 930000, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`,
    'L''Infini - Gift of the Observer (10)'
FROM `gameobject_loot_template` WHERE `Entry` = 27030;

-- Type 14: yell, 41: boss emote (the middle of the screen; %s is the god's name). TextRange 3: the whole map.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930000, 0, 0, 'This is what you face.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - aggro'),
    (930000, 1, 0, '%s gathers the stars into itself... Stand at its feet!', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - Big Bang warning'),
    (930000, 2, 0, 'Everything begins... and everything ends.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - Big Bang'),
    (930000, 3, 0, 'Mortals... still standing? Interesting.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - intermission 1'),
    (930000, 4, 0, '%s is exposed! Strike now, and stop the Fragments of Eternity!', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - exposed'),
    (930000, 5, 0, 'Then let the real fight begin.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - intermission 1 end'),
    (930000, 6, 0, 'A star collapses! Share it: three of you or more.', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - collapsing star'),
    (930000, 7, 0, 'A singularity opens! Break free of its pull.', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - singularity'),
    (930000, 8, 0, 'The heavens tear apart.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - intermission 2'),
    (930000, 9, 0, 'The void devours the edge of the world.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - setup'),
    (930000, 10, 0, 'The edge of the Planetarium turns deadly!', 41, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - edge'),
    (930000, 11, 0, 'Behold infinity.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - reveal'),
    (930000, 12, 0, '%s calls a Supernova! Find the golden lanes.', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - supernova'),
    (930000, 13, 0, 'Divine Judgement! Those marked, keep away from the others.', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - divine judgement'),
    (930000, 14, 0, 'The End of Times.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - final'),
    (930000, 15, 0, 'All returns to the void.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - hard enrage'),
    (930000, 16, 0, 'Stardust.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - kill'),
    (930000, 17, 0, 'So infinity... has an end.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - death'),
    (930000, 18, 0, 'A Fragment of Eternity merges with %s!', 41, 0, 100, 0, 0, 0, 0, 3,
     'L''Infini - fragment merges');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930000, 0, 0, 'frFR', 'Voici ce que vous affrontez.'),
    (930000, 1, 0, 'frFR', '%s rassemble les étoiles en lui... Rapprochez-vous de ses pieds !'),
    (930000, 2, 0, 'frFR', 'Tout commence... et tout finit.'),
    (930000, 3, 0, 'frFR', 'Des mortels... encore debout ? Intéressant.'),
    (930000, 4, 0, 'frFR', '%s est exposé ! Frappez maintenant, et arrêtez les Fragments d''éternité !'),
    (930000, 5, 0, 'frFR', 'Alors que le vrai combat commence.'),
    (930000, 6, 0, 'frFR', 'Une étoile s''effondre ! Partagez-la : à trois ou plus.'),
    (930000, 7, 0, 'frFR', 'Une singularité s''ouvre ! Échappez à son attraction.'),
    (930000, 8, 0, 'frFR', 'Les cieux se déchirent.'),
    (930000, 9, 0, 'frFR', 'Le néant dévore les bords du monde.'),
    (930000, 10, 0, 'frFR', 'Le bord du Planétarium devient mortel !'),
    (930000, 11, 0, 'frFR', 'Contemplez l''infini.'),
    (930000, 12, 0, 'frFR', '%s déclenche une Supernova ! Rejoignez les couloirs dorés.'),
    (930000, 13, 0, 'frFR', 'Jugement divin ! Les marqués, écartez-vous des autres.'),
    (930000, 14, 0, 'frFR', 'La Fin des Temps.'),
    (930000, 15, 0, 'frFR', 'Tout retourne au néant.'),
    (930000, 16, 0, 'frFR', 'Poussière d''étoiles.'),
    (930000, 17, 0, 'frFR', 'L''infini... a donc... une fin.'),
    (930000, 18, 0, 'frFR', 'Un Fragment d''éternité fusionne avec %s !');

-- In the middle of the Planetarium, facing the way in (Algalon's own spot, boss_algalon_the_observer.cpp), in the
-- 10-player Ulduar only (spawn mask 1)
INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000400, 930000, 603, 0, 0, 1, 1, 0, 1632.668, -302.7656, 417.3211, 1.5302, 604800, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'L''Infini - Celestial Planetarium (Défi board only)');
