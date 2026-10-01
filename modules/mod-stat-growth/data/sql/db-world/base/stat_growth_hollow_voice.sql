-- The Hollow Voice, the pinnacle of the Défi board, in Sunwell Plateau's M'uru chamber (HollowVoice.cpp; the board
-- holds it from mod-playerbots ChallengeBoard.cpp, 10 players, item level 460 and 650 paragon, ChallengeTiers.h).
-- 930100 Archbishop Aldric Dawnmantle: Archbishop Landgren's look (29542, display 26326), a level 83 boss (class 1:
--        13 945 base health a point, as L'Infini). 4 621 x 13 945 = 64.4 million (the model's 58.6 + 10%: with the
--        melee's paragon fixed, the bots took him too fast): the group of the profile (the raid
--        finder's five damage dealers, two tanks and three healers: Power::GroupDamageDealers 5.97 at 103 000 a second
--        each, PowerScaling.h) brings him to 1% in about 110 s of the 2:00 his track gives them - a hard check
--        (.agents/docs/systems/power-scaling.md). It was 72 million, on six damage dealers and healers at a third.
--        (First sized on the compounded paragon rule at 505 million, seven times what the bench measures.) His health
--        stops at 1%; he is the board's boss (its kill is the win), and stays hidden once the demon is out.
-- 930101 Vel'thazar, the Hollow Voice: Balnazzar's look (10813, display 10691), the demon inside him, summoned by his
--        script. 12 647 x 13 945 = 176.4 million, about 330 s of the group's damage outside the intermissions (the
--        model's 136.7 + 7.5%, then + 20% once the melee's paragon was fixed: he still fell too easily).
-- 930102 Dread Infernal: an Infernal (89), two crashing down in phase 3, an off-tank holding them: 287 x 13 945 =
--        4 million each, about 11 s of the group for the two.
-- The Archbishop's static spawn stands where M'uru floats, in every Sunwell (spawn mask 1: its only mode); his script
-- hides him and removes him from any instance that is not a challenge's, and clears the chamber of its own occupants.

DELETE FROM `creature` WHERE `guid` = 9000401;
DELETE FROM `creature_text_locale` WHERE `CreatureID` IN (930100, 930101);
DELETE FROM `creature_text` WHERE `CreatureID` IN (930100, 930101);
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 930100 AND 930102;
DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 930100 AND 930102;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 930100 AND 930102;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 930100 AND 930102;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_hollow_voice`;
CREATE TEMPORARY TABLE `tmp_stat_growth_hollow_voice` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_hollow_voice`
SELECT * FROM `creature_template` WHERE `entry` IN (29542, 10813, 89);
UPDATE `tmp_stat_growth_hollow_voice` SET
    `entry` = 930100,
    `difficulty_entry_1` = 0,
    `name` = 'Archbishop Aldric Dawnmantle',
    `subname` = '',
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
    `HealthModifier` = 4621,
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
    `ScriptName` = 'boss_hollow_voice_aldric',
    `VerifiedBuild` = NULL
WHERE `entry` = 29542;
UPDATE `tmp_stat_growth_hollow_voice` SET
    `entry` = 930101,
    `difficulty_entry_1` = 0,
    `name` = 'Vel''thazar',
    `subname` = 'The Hollow Voice',
    `minlevel` = 83,
    `maxlevel` = 83,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 3,
    `npcflag` = 0,
    `unit_flags` = 0,
    `DamageModifier` = 80,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 12647,
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
    `ScriptName` = 'boss_hollow_voice_velthazar',
    `VerifiedBuild` = NULL
WHERE `entry` = 10813;
UPDATE `tmp_stat_growth_hollow_voice` SET
    `entry` = 930102,
    `difficulty_entry_1` = 0,
    `name` = 'Dread Infernal',
    `subname` = '',
    `minlevel` = 83,
    `maxlevel` = 83,
    `exp` = 2,
    `faction` = 14,
    `unit_class` = 1,
    `rank` = 1,
    `npcflag` = 0,
    `unit_flags` = 0,
    `DamageModifier` = 40,
    `BaseAttackTime` = 2000,
    `HealthModifier` = 287,
    `RegenHealth` = 0,
    `flags_extra` = 0,
    `lootid` = 0,
    `mingold` = 0,
    `maxgold` = 0,
    `AIName` = '',
    `ScriptName` = 'npc_hollow_voice_infernal',
    `VerifiedBuild` = NULL
WHERE `entry` = 89;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_hollow_voice`;
DROP TEMPORARY TABLE `tmp_stat_growth_hollow_voice`;

-- Vel'thazar is Balnazzar recoloured later (his own display then); both at their model's size for now
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930100, 0, 26326, 1.3, 1, NULL),
    (930101, 0, 10691, 1.6, 1, NULL),
    (930102, 0, 169, 1.3, 1, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930100, 1, 0, 0, 0, 0, 0, NULL),
    (930101, 1, 0, 0, 0, 0, 0, NULL),
    (930102, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (930100, 'frFR', 'Archevêque Aldric Mantaube', NULL, NULL),
    (930101, 'frFR', 'Vel''thazar', 'La Voix creuse', NULL),
    (930102, 'frFR', 'Infernal de l''effroi', NULL, NULL);

-- Type 14: yell, lore only (no line announcing a mechanic). TextRange 3: the whole map. The Archbishop never names the
-- demon: his warning, fallen, in the silence before it tears out, is a frightened man's.
INSERT INTO `creature_text`
    (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`,
     `BroadcastTextId`, `TextRange`, `comment`)
VALUES
    (930100, 0, 0, 'The Light has judged you. I am only its hand.', 14, 0, 100, 0, 0, 0, 0, 3, 'Aldric - aggro'),
    (930100, 1, 0, 'Rest now. The Light keeps what it takes.', 14, 0, 100, 0, 0, 0, 0, 3, 'Aldric - kill'),
    (930100, 2, 0, 'Enough... it is... finished...', 14, 0, 100, 0, 0, 0, 0, 3, 'Aldric - falls'),
    (930100, 3, 0, 'Your souls are commended to the Light. Last Rites.', 14, 0, 100, 0, 0, 0, 0, 3, 'Aldric - too slow'),
    (930100, 4, 0, 'Free... at last...', 14, 0, 100, 0, 0, 0, 0, 3, 'Aldric - released'),
    (930100, 5, 0, 'The voice... it was never the Light. It wakes... Run!', 14, 0, 100, 0, 0, 0, 0, 3, 'Aldric - warning, before the demon'),
    (930101, 0, 0, 'Finished? The sermon has only begun.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vel''thazar - reveal'),
    (930101, 1, 0, 'Another voice for my choir.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vel''thazar - kill'),
    (930101, 2, 0, 'Silence.', 14, 0, 100, 0, 0, 0, 0, 3, 'Vel''thazar - hard enrage'),
    (930101, 3, 0, 'The voice... is... hollow...', 14, 0, 100, 0, 0, 0, 0, 3, 'Vel''thazar - death');

INSERT INTO `creature_text_locale` (`CreatureID`, `GroupID`, `ID`, `Locale`, `Text`) VALUES
    (930100, 0, 0, 'frFR', 'La Lumière vous a jugés. Je ne suis que sa main.'),
    (930100, 1, 0, 'frFR', 'Reposez en paix. La Lumière garde ce qu''elle prend.'),
    (930100, 2, 0, 'frFR', 'Assez... c''est... terminé...'),
    (930100, 3, 0, 'frFR', 'Vos âmes sont confiées à la Lumière. Derniers sacrements.'),
    (930100, 4, 0, 'frFR', 'Libre... enfin...'),
    (930100, 5, 0, 'frFR', 'Cette voix... ce n''était pas la Lumière. Elle s''éveille... Fuyez !'),
    (930101, 0, 0, 'frFR', 'Terminé ? Le sermon ne fait que commencer.'),
    (930101, 1, 0, 'frFR', 'Une voix de plus pour mon chœur.'),
    (930101, 2, 0, 'frFR', 'Silence.'),
    (930101, 3, 0, 'frFR', 'La voix... est... creuse...');

-- Where M'uru floats, on the chamber's floor (69.6 in its middle, 71.2 at its edge, 39 yards round: measured with
-- .hollow floor), facing the way in, in Sunwell's only mode (spawn mask 1)
INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000401, 930100, 580, 0, 0, 1, 1, 0, 1816.25, 625.484, 69.65, 5.62435, 604800, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'The Hollow Voice - M''uru''s chamber (Défi board only)');
