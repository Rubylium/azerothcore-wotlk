-- Ingvar the Plunderer reworked (mod-stat-growth mythic/UtgardeKeep.cpp, boss_ingvar_evolutions), in every mode.
-- 930301 Âme vrykule: the souls that walk to his body while Annhylde raises him; killed on the way, or each one adds to
--        his damage. The Vrykul Soul's look (24262), hostile as he is, slow; the script walks it and keeps it from
--        fighting.
UPDATE `creature_template` SET `ScriptName` = 'boss_ingvar_evolutions' WHERE `entry` = 23954;

DELETE FROM `creature_template_locale` WHERE `entry` = 930301;
DELETE FROM `creature_template_movement` WHERE `CreatureId` = 930301;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 930301;
DELETE FROM `creature_template` WHERE `entry` = 930301;

DROP TEMPORARY TABLE IF EXISTS `tmp_ingvar_spirit`;
CREATE TEMPORARY TABLE `tmp_ingvar_spirit` LIKE `creature_template`;
INSERT INTO `tmp_ingvar_spirit` SELECT * FROM `creature_template` WHERE `entry` = 24262;
UPDATE `tmp_ingvar_spirit` SET
    `entry` = 930301,
    `name` = 'Vrykul Soul',
    `subname` = '',
    `minlevel` = 80,
    `maxlevel` = 80,
    `faction` = 1885,
    `unit_class` = 1,
    `rank` = 1,
    `unit_flags` = 0,
    `HealthModifier` = 0.8,
    `DamageModifier` = 0,
    `speed_walk` = 1,
    `RegenHealth` = 0,
    `flags_extra` = 0x00000040,
    `AIName` = '',
    `ScriptName` = '',
    `VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_ingvar_spirit`;
DROP TEMPORARY TABLE `tmp_ingvar_spirit`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (930301, 0, 20089, 1.2, 1, NULL);

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
VALUES
    (930301, 1, 0, 0, 0, 0, 0, NULL);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
(930301, 'frFR', 'Âme vrykule', '', 0);
