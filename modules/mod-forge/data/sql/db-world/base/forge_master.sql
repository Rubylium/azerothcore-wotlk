-- The Forge's master smith (modules/mod-forge, npc_forge_master): a Thorium Brotherhood blacksmith, neutral to both
-- factions, at an anvil in each capital. Talking to him opens the Forge; while he works on a piece he strikes it
-- three times (spell 92403, localTools/forge/Spells.ps1).
--
-- Malyfous Darkhammer's look (the Brotherhood's smith in Everlook, display 9969), the trainers' smithing hammer (1903),
-- and the capitals' smiths' idle work at the anvil (emote 233). He stands 1.9 yards from the anvil, facing it, on the
-- side away from the trainers: Therum Deepforge's anvil in Stormwind's Dwarven District, the Burning Anvil in
-- Orgrimmar's Valley of Honor, the Great Anvil in Ironforge's Great Forge.

DELETE FROM `creature` WHERE `guid` BETWEEN 9000200 AND 9000202;
DELETE FROM `creature_equip_template` WHERE `CreatureID` = 910200;
DELETE FROM `creature_template_addon` WHERE `entry` = 910200;
DELETE FROM `creature_template_locale` WHERE `entry` = 910200;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 910200;
DELETE FROM `creature_template` WHERE `entry` = 910200;

DROP TEMPORARY TABLE IF EXISTS `tmp_forge_master`;
CREATE TEMPORARY TABLE `tmp_forge_master` LIKE `creature_template`;
INSERT INTO `tmp_forge_master` SELECT * FROM `creature_template` WHERE `entry` = 10637;
UPDATE `tmp_forge_master` SET
    `entry` = 910200,
    `name` = 'Durgan Emberstrike',
    `subname` = 'Master Blacksmith',
    `gossip_menu_id` = 0,
    `npcflag` = 1,
    `faction` = 35,
    `AIName` = '',
    `ScriptName` = 'npc_forge_master',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_forge_master`;
DROP TEMPORARY TABLE `tmp_forge_master`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (910200, 0, 9969, 1, 1, NULL);

INSERT INTO `creature_template_addon`
    (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
VALUES
    (910200, 0, 0, 0, 1, 233, 0, '');

INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`) VALUES
    (910200, 1, 1903, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
    (910200, 'frFR', 'Durgan Frappe-Braise', 'Maître forgeron', NULL);

INSERT INTO `creature`
    (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
     `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
    (9000200, 910200, 0, 0, 0, 1, 1, 1, -8418.887, 616.072, 95.45, 2.9734, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'The Forge: master smith, Stormwind Dwarven District'),
    (9000201, 910200, 1, 0, 0, 1, 1, 1, 2071.138, -4822.572, 23.57, 3.9603, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'The Forge: master smith, Orgrimmar Valley of Honor'),
    (9000202, 910200, 0, 0, 0, 1, 1, 1, -4800.548, -1105.697, 498.82, 5.388, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', NULL, 0,
     'The Forge: master smith, Ironforge Great Forge');
