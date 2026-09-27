-- The Infinite Dungeon (Donjon infini, src/infinite/InfiniteDungeon.cpp): its keeper in every capital, the portal
-- down to the next floor, the healing heart and the checkpoint chest. The creatures of the floors are in
-- stat_growth_infinite_dungeon_creatures.sql.
--
-- Ids: creature 920000 (the keeper), 920010-920299 (floor creatures); spawns 9000300-9000309; gameobjects
-- 920100-920102; npc_text 920000-920001.
--
-- The keeper is Chromie's gnome (display 10008), the bronze dragonflight minding an endless descent. Each spawn stands
-- beside the city's bankers (or the other keepers in Stormwind's Trade District, and the master smith in Ironforge
-- and Orgrimmar), where everyone passes; the client puts a pin on the world map for each (InfiniteDungeon.lua).

DELETE FROM `creature` WHERE `guid` BETWEEN 9000300 AND 9000309;
DELETE FROM `creature_template_locale` WHERE `entry` = 920000;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 920000;
DELETE FROM `creature_template` WHERE `entry` = 920000;

DROP TEMPORARY TABLE IF EXISTS `tmp_infinite_dungeon_keeper`;
CREATE TEMPORARY TABLE `tmp_infinite_dungeon_keeper` LIKE `creature_template`;
INSERT INTO `tmp_infinite_dungeon_keeper`
SELECT * FROM `creature_template` WHERE `entry` = 328;
UPDATE `tmp_infinite_dungeon_keeper` SET
    `entry` = 920000,
    `name` = 'Eternia',
    `subname` = 'Infinite Dungeon',
    `gossip_menu_id` = 0,
    `npcflag` = 1,
    `faction` = 35,
    `minlevel` = 80,
    `maxlevel` = 80,
    `AIName` = '',
    `ScriptName` = 'npc_infinite_dungeon_keeper',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_infinite_dungeon_keeper`;
DROP TEMPORARY TABLE `tmp_infinite_dungeon_keeper`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (920000, 0, 10008, 1, 1, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (920000, 'frFR', 'Eternia', 'Donjon infini', NULL);

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000300, 920000, 0, 0, 0, 1, 1, 0, -8820.380, 624.320, 93.833, 3.3992, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Infinite Dungeon keeper - Stormwind'),
    (9000301, 920000, 0, 0, 0, 1, 1, 0, -4798.600, -1104.150, 498.820, 5.3880, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL,
     0, 'Infinite Dungeon keeper - Ironforge'),
    (9000302, 920000, 1, 0, 0, 1, 1, 0, 9940.170, 2514.600, 1317.660, 1.1868, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL,
     0, 'Infinite Dungeon keeper - Darnassus'),
    (9000303, 920000, 530, 0, 0, 1, 1, 0, -3919.570, -11549.660, -150.039, 1.4462, 300, 0, 0, 1, 0, 0, 0, 0, 0, '',
     NULL, 0, 'Infinite Dungeon keeper - The Exodar'),
    (9000304, 920000, 1, 0, 0, 1, 1, 0, 2072.970, -4824.270, 23.570, 3.9603, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Infinite Dungeon keeper - Orgrimmar'),
    (9000305, 920000, 0, 0, 0, 1, 1, 0, 1596.670, 231.130, -52.060, 1.7279, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Infinite Dungeon keeper - Undercity'),
    (9000306, 920000, 1, 0, 0, 1, 1, 0, -1257.700, 24.300, 128.270, 4.8700, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Infinite Dungeon keeper - Thunder Bluff'),
    (9000307, 920000, 530, 0, 0, 1, 1, 0, 9525.230, -7216.820, 16.214, 4.7124, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL,
     0, 'Infinite Dungeon keeper - Silvermoon City'),
    (9000308, 920000, 571, 0, 0, 1, 1, 0, 5614.280, 693.160, 652.092, 0.3317, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL,
     0, 'Infinite Dungeon keeper - Dalaran'),
    (9000309, 920000, 530, 0, 0, 1, 1, 0, -2013.000, 5365.550, -9.268, 0.6634, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL,
     0, 'Infinite Dungeon keeper - Shattrath');

-- What the keeper says before the options (920000), and the portal once a floor is cleared (920001)
DELETE FROM `npc_text_locale` WHERE `ID` IN (920000, 920001);
DELETE FROM `npc_text` WHERE `ID` IN (920000, 920001);
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `BroadcastTextID0`, `lang0`, `Probability0`) VALUES
    (920000,
     'The Infinite Dungeon goes down without end. Each floor: a few foes, then a guardian; its portal takes you one floor deeper.$B$BYou arrive in a circle of light where nothing can touch you. Step out of it when you are ready.$B$BEvery tenth floor keeps your progress.',
     'The Infinite Dungeon goes down without end. Each floor: a few foes, then a guardian; its portal takes you one floor deeper.$B$BYou arrive in a circle of light where nothing can touch you. Step out of it when you are ready.$B$BEvery tenth floor keeps your progress.',
     0, 0, 1),
    (920001,
     'The guardian has fallen. The way down is open.',
     'The guardian has fallen. The way down is open.',
     0, 0, 1);
INSERT INTO `npc_text_locale` (`ID`, `Locale`, `Text0_0`, `Text0_1`) VALUES
    (920000, 'frFR',
     'Le Donjon infini descend sans fin. À chaque étage : quelques ennemis, puis un gardien ; son portail vous mène un étage plus bas.$B$BVous arrivez dans un cercle de lumière où rien ne peut vous atteindre. Sortez-en quand vous êtes prêt.$B$BTous les dix étages, votre progression est gardée.',
     'Le Donjon infini descend sans fin. À chaque étage : quelques ennemis, puis un gardien ; son portail vous mène un étage plus bas.$B$BVous arrivez dans un cercle de lumière où rien ne peut vous atteindre. Sortez-en quand vous êtes prête.$B$BTous les dix étages, votre progression est gardée.'),
    (920001, 'frFR', 'Le gardien est tombé. La voie vers le bas est ouverte.',
     'Le gardien est tombé. La voie vers le bas est ouverte.');

-- The portal (a mage portal's swirl), the heart (the Heart of Hakkar's model, small) and the checkpoint chest
DELETE FROM `gameobject_template_locale` WHERE `entry` BETWEEN 920100 AND 920102;
DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 920100 AND 920102;
INSERT INTO `gameobject_template`
    (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `AIName`, `ScriptName`,
     `VerifiedBuild`)
VALUES
    (920100, 10, 4396, 'Descent', 'Interact', '', '', 1, '', 'go_infinite_dungeon_portal', NULL),
    (920101, 5, 6395, 'Healing Heart', '', '', '', 0.25, '', '', NULL),
    (920102, 10, 9069, 'Checkpoint Cache', 'Interact', '', '', 1, '', 'go_infinite_dungeon_chest', NULL);

INSERT INTO `gameobject_template_locale` (`entry`, `locale`, `name`, `castBarCaption`, `VerifiedBuild`) VALUES
    (920100, 'frFR', 'Descente', '', NULL),
    (920101, 'frFR', 'Cœur guérisseur', '', NULL),
    (920102, 'frFR', 'Coffre du point de passage', '', NULL);
