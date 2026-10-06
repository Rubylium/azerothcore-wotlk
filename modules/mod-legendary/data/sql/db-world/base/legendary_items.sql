-- Legendary items (modules/mod-legendary): their base items. A base gives the look, the slot and the quality; every
-- copy rolls its own item level, power and stats (character_legendary), so the template carries none.
--
-- 24567 Marque de l'Inquisiteur: the Scarlet Cathedral's legendary, a cloak. An id the client's Item.dbc already has
-- (a cloth cloak row with no template here); localTools/patchSinisterStrike.ps1 gives that row its own display
-- (71000): the Scarlet Onslaught cape's red (64326) with its icon. Made from the Recovered Scarlet Onslaught Cape
-- (50470), its stats stripped.
DELETE FROM `item_template` WHERE `entry` = 24567;
DELETE FROM `item_template_locale` WHERE `ID` = 24567;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 50470;
UPDATE `legendary_base` SET
    `entry` = 24567, `name` = 'Marque de l''Inquisiteur', `Quality` = 5, `ItemLevel` = 289, `RequiredLevel` = 80,
    `displayid` = 71000, `bonding` = 1, `armor` = 0, `ScalingStatDistribution` = 0,
    `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '';
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
-- Its lore is written by the client (FrameXML Legendary.lua), under the copy's rolls
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(24567, 'frFR', 'Marque de l''Inquisiteur', '', 0);
