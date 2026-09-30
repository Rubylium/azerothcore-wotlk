-- L'Infini, the Défi board's god fight in Ulduar's Celestial Planetarium (InfiniteGod.cpp; the board posts it from
-- mod-playerbots ChallengeBoard.cpp, 10 players). Every creature is a copy of a stock one of Algalon's fight:
-- 930000 L'Infini: Algalon the Observer (32871), hostile to all, no longer immune to players, its health set for a
--        timeline fight of 5 minutes at Défi I, on the power model (PowerScaling.h) at its profile, item level 300
--        and 100 paragon (ChallengeTiers.h BossProfiles): 1 394 x 13 945 (level 83 elite) = 19.4 million, the tier
--        multiplying it - a hard damage check. Power::DpsCheckHealth(300, 100, GroupDamageDealers(5, 2, 3) = 5.97,
--        209.5): a damage dealer of the profile deals 15 550 on one target; the raid finder's ten are five damage
--        dealers, two tanks (a third each) and three healers (a tenth); on the god 85% of 4:50, and done in 85% of
--        that - a group at the profile, players or one player and its bots (at the profile too), kills it some 40 s
--        before the end. It was 47.6 million (set by play on bots scaled on their player), then 28.1 (six damage
--        dealers and the healers at a third, no margin: only a player far above the profile could carry it).
--        Its melee: a 1.5 s swing at 90 damage
--        modifier. No gear on its corpse: the challenge board gives its own (ChallengeBoard.cpp GodItemLevel), gold
--        only.
-- 930001 Fragment d'éternité: a Living Constellation (33052) that walks to the god in intermission 1, 0.5 million
--        health (90 x 13 933): the raid kills both in the 20 s they walk, if it turns on them.
-- 930002 Étoile effondrée: a Collapsing Star (32955) to share, friendly (nothing attacks it), rooted.
-- 930003 Singularité: a Black Hole (32953), the look of the pull, friendly and rooted.
-- The god's static spawn stands in the middle of the Planetarium in every 10-player Ulduar; its script hides it and
-- removes it from any instance that is not a challenge's.

DELETE FROM `creature` WHERE `guid` = 9000400;
DELETE FROM `spell_proc` WHERE `SpellId` IN (90757, 90759, 90761);
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
    `DamageModifier` = 90,
    `BaseAttackTime` = 1500,
    `HealthModifier` = 1394,
    `lootid` = 0,
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


-- Type 14: yell, its lines only (no boss emote announcing a mechanic). TextRange 3: the whole map.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930000, 0, 0, 'This is what you face.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - aggro'),
    (930000, 2, 0, 'Everything begins... and everything ends.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - Big Bang'),
    (930000, 3, 0, 'Mortals... still standing? Interesting.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - intermission 1'),
    (930000, 5, 0, 'Then let the real fight begin.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - intermission 1 end'),
    (930000, 8, 0, 'The heavens tear apart.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - intermission 2'),
    (930000, 9, 0, 'The void devours the edge of the world.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - setup'),
    (930000, 11, 0, 'Behold infinity.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - reveal'),
    (930000, 14, 0, 'The End of Times.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - final'),
    (930000, 15, 0, 'All returns to the void.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - hard enrage'),
    (930000, 16, 0, 'Stardust.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - kill'),
    (930000, 17, 0, 'So infinity... has an end.', 14, 0, 100, 0, 0, 0, 0, 3, 'L''Infini - death');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930000, 0, 0, 'frFR', 'Voici ce que vous affrontez.'),
    (930000, 2, 0, 'frFR', 'Tout commence... et tout finit.'),
    (930000, 3, 0, 'frFR', 'Des mortels... encore debout ? Intéressant.'),
    (930000, 5, 0, 'frFR', 'Alors que le vrai combat commence.'),
    (930000, 8, 0, 'frFR', 'Les cieux se déchirent.'),
    (930000, 9, 0, 'frFR', 'Le néant dévore les bords du monde.'),
    (930000, 11, 0, 'frFR', 'Contemplez l''infini.'),
    (930000, 14, 0, 'frFR', 'La Fin des Temps.'),
    (930000, 15, 0, 'frFR', 'Tout retourne au néant.'),
    (930000, 16, 0, 'frFR', 'Poussière d''étoiles.'),
    (930000, 17, 0, 'frFR', 'L''infini... a donc... une fin.');

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

-- Its gear's bonuses (localTools/infiniteBoss/Spells.ps1 90757-90762, put on the pieces by MythicDungeonSystem.cpp
-- TouchByInfiniteGod): the equip auras' proc chance stays their spell's own, the cooldowns are here. SpellTypeMask
-- 1 damage, 3 damage or heal; SpellPhaseMask 2 on the hit. As the stock trinkets they copy (60221, 60482, 60490).
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
    (90757, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 30000, 0),     -- Égide des astres
    (90759, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 8000, 0),      -- Éclat d'étoile filante
    (90761, 0, 0, 0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0, 45000, 0);     -- Étincelle d'éternité
