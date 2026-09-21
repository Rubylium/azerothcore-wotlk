-- The paragon keeper in Stormwind's Trade District (ParagonSystem.cpp): talking to it opens the board.
--
-- It wears Algalon the Observer's model (display 28641). The board is a constellation and Algalon is the
-- titans' astral observer, so the two read as the same idea, and nothing in a capital city looks less like an
-- ordinary quest giver. The template is still cloned from a harmless mage trainer, so it has a trainer's
-- flags and stats rather than a raid boss's.

DELETE FROM `creature` WHERE `guid` = 9000011;
DELETE FROM `creature_template_addon` WHERE `entry` = 900102;
DELETE FROM `creature_template_locale` WHERE `entry` = 900102;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900102;
DELETE FROM `creature_template` WHERE `entry` = 900102;

DROP TEMPORARY TABLE IF EXISTS `tmp_stat_growth_paragon_keeper`;
CREATE TEMPORARY TABLE `tmp_stat_growth_paragon_keeper` LIKE `creature_template`;
INSERT INTO `tmp_stat_growth_paragon_keeper`
SELECT * FROM `creature_template` WHERE `entry` = 328;
UPDATE `tmp_stat_growth_paragon_keeper` SET
    `entry` = 900102,
    `name` = 'Astralon',
    `subname` = 'Paragon Keeper',
    `gossip_menu_id` = 0,
    `npcflag` = 1,
    `faction` = 35,
    `AIName` = '',
    `ScriptName` = 'npc_stat_growth_paragon_keeper',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_stat_growth_paragon_keeper`;
DROP TEMPORARY TABLE `tmp_stat_growth_paragon_keeper`;

-- Algalon's model rather than the mage trainer's it was cloned from.
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (900102, 0, 28641, 1, 1, NULL);

-- Two auras, both of them purely cosmetic: each applies SPELL_AURA_DUMMY, which does nothing on its own, and
-- each lasts forever (duration index 21). Spirit Particles is the drifting mote effect a dozen stock NPCs
-- already wear; Arcane Power State lays an arcane charge over the model. Neither touches combat, stats or
-- threat - they only exist to be looked at.
INSERT INTO `creature_template_addon`
    (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
VALUES
    (900102, 0, 0, 0, 1, 0, 0, '17327 49411');

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (900102, 'frFR', 'Astralon', 'Gardien du parangon', NULL);

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000011, 900102, 0, 0, 0, 1, 1, 0, -8850.140, 634.802, 98.221, 0.1731, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'Stormwind Trade District paragon keeper');
