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
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0,
    -- Nothing of the item it is made from may limit it: every class and race (a tier piece was its class's and
    -- faction's), no flag (heroic, unique, refundable), no limit category (a ring's was the Ashen Verdict's)
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
-- Its lore is written by the client (FrameXML Legendary.lua), under the copy's rolls
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(24567, 'frFR', 'Marque de l''Inquisiteur', '', 0);

-- 996 Serment de Whitemane: the Scarlet Cathedral's second legendary, a ring: a ring row the client's Item.dbc already
-- has, with no template here, given its own display (71001): its row's look (3453) with its icon. Made from the Ring of
-- Phased Regeneration (53490), its stats stripped.
DELETE FROM `item_template` WHERE `entry` = 996;
DELETE FROM `item_template_locale` WHERE `ID` = 996;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 53490;
UPDATE `legendary_base` SET
    `entry` = 996, `name` = 'Serment de Whitemane', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = -1, `displayid` = 71001, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0,
    `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0,
    -- Nothing of the item it is made from may limit it: every class and race (a tier piece was its class's and
    -- faction's), no flag (heroic, unique, refundable), no limit category (a ring's was the Ashen Verdict's)
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(996, 'frFR', 'Serment de Whitemane', '', 0);

-- 21428 Consécration de Mograine: the third, gloves: a hands row of the client's of the
-- "misc" armour subclass, so every class wears them (mod-legendary rolls their armour for the looter's armour type);
-- localTools/patchSinisterStrike.ps1 gives it its own display (71002): Turalyon's red and gold gauntlets (62062) with
-- its icon. Made from Turalyon's Gauntlets of Triumph (48615), its stats stripped.
DELETE FROM `item_template` WHERE `entry` = 21428;
DELETE FROM `item_template_locale` WHERE `ID` = 21428;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 48615;
UPDATE `legendary_base` SET
    `entry` = 21428, `name` = 'Consécration de Mograine', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `displayid` = 71002, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0,
    `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0,
    -- Nothing of the item it is made from may limit it: every class and race (a tier piece was its class's and
    -- faction's), no flag (heroic, unique, refundable), no limit category (a ring's was the Ashen Verdict's)
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21428, 'frFR', 'Consécration de Mograine', '', 0);

-- BEGIN generated by localTools/legendary/buildLegendaryItemSql.py
-- The other dungeons' legendaries (mod-legendary Definitions 4-24): see the script for how each is made.
-- 21424 Épaulières de Capacitus: copied from 31294, its look's item
DELETE FROM `item_template` WHERE `entry` = 21424;
DELETE FROM `item_template_locale` WHERE `ID` = 21424;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 31294;
UPDATE `legendary_base` SET
    `entry` = 21424, `name` = 'Épaulières de Capacitus', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 3,
    `sheath` = 0,
    `displayid` = 71003, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21424, 'frFR', 'Épaulières de Capacitus', '', 0),
(21424, 'enUS', 'Capacitus'' Spaulders', '', 0);

-- 1258 Abaque de Pathaleon: copied from 28288, its look's item
DELETE FROM `item_template` WHERE `entry` = 1258;
DELETE FROM `item_template_locale` WHERE `ID` = 1258;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 28288;
UPDATE `legendary_base` SET
    `entry` = 1258, `name` = 'Abaque de Pathaleon', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 1, `InventoryType` = 12,
    `sheath` = 0,
    `displayid` = 71004, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(1258, 'frFR', 'Abaque de Pathaleon', '', 0),
(1258, 'enUS', 'Pathaleon''s Abacus', '', 0);

-- 21432 Brassards de Sepethrea: copied from 31284, its look's item
DELETE FROM `item_template` WHERE `entry` = 21432;
DELETE FROM `item_template_locale` WHERE `ID` = 21432;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 31284;
UPDATE `legendary_base` SET
    `entry` = 21432, `name` = 'Brassards de Sepethrea', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 9,
    `sheath` = 0,
    `displayid` = 71005, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21432, 'frFR', 'Brassards de Sepethrea', '', 0),
(21432, 'enUS', 'Sepethrea''s Bracers', '', 0);

-- 21425 Ceinture d'Ingvar: copied from 37785, its look's item
DELETE FROM `item_template` WHERE `entry` = 21425;
DELETE FROM `item_template_locale` WHERE `ID` = 21425;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 37785;
UPDATE `legendary_base` SET
    `entry` = 21425, `name` = 'Ceinture d''Ingvar', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 6,
    `sheath` = 0,
    `displayid` = 71006, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21425, 'frFR', 'Ceinture d''Ingvar', '', 0),
(21425, 'enUS', 'Ingvar''s Girdle', '', 0);

-- 21420 Cuirasse de Keleseth: copied from 35574, its look's item
DELETE FROM `item_template` WHERE `entry` = 21420;
DELETE FROM `item_template_locale` WHERE `ID` = 21420;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 35574;
UPDATE `legendary_base` SET
    `entry` = 21420, `name` = 'Cuirasse de Keleseth', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 5,
    `sheath` = 0,
    `displayid` = 71007, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21420, 'frFR', 'Cuirasse de Keleseth', '', 0),
(21420, 'enUS', 'Keleseth''s Breastplate', '', 0);

-- 26541 Collier d'Annhylde: copied from 37748, its look's item
DELETE FROM `item_template` WHERE `entry` = 26541;
DELETE FROM `item_template_locale` WHERE `ID` = 26541;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 37748;
UPDATE `legendary_base` SET
    `entry` = 26541, `name` = 'Collier d''Annhylde', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 3, `InventoryType` = 2,
    `sheath` = 0,
    `displayid` = 71008, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(26541, 'frFR', 'Collier d''Annhylde', '', 0),
(26541, 'enUS', 'Annhylde''s Necklace', '', 0);

-- 21437 Poignes de Kargath: copied from 27528, its look's item
DELETE FROM `item_template` WHERE `entry` = 21437;
DELETE FROM `item_template_locale` WHERE `ID` = 21437;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 27528;
UPDATE `legendary_base` SET
    `entry` = 21437, `name` = 'Poignes de Kargath', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 10,
    `sheath` = 0,
    `displayid` = 71009, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21437, 'frFR', 'Poignes de Kargath', '', 0),
(21437, 'enUS', 'Kargath''s Grips', '', 0);

-- 21433 Bandelettes de Nethekurse: copied from 27517, its look's item
DELETE FROM `item_template` WHERE `entry` = 21433;
DELETE FROM `item_template_locale` WHERE `ID` = 21433;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 27517;
UPDATE `legendary_base` SET
    `entry` = 21433, `name` = 'Bandelettes de Nethekurse', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 9,
    `sheath` = 0,
    `displayid` = 71010, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21433, 'frFR', 'Bandelettes de Nethekurse', '', 0),
(21433, 'enUS', 'Nethekurse''s Wraps', '', 0);

-- 5828 Chevalière de Porung: copied from 31290, its look's item
DELETE FROM `item_template` WHERE `entry` = 5828;
DELETE FROM `item_template_locale` WHERE `ID` = 5828;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 31290;
UPDATE `legendary_base` SET
    `entry` = 5828, `name` = 'Chevalière de Porung', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 5, `InventoryType` = 11,
    `sheath` = 0,
    `displayid` = 71011, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(5828, 'frFR', 'Chevalière de Porung', '', 0),
(5828, 'enUS', 'Porung''s Signet', '', 0);

-- 21421 Plastron de VanCleef: copied from 10399, its look's item
DELETE FROM `item_template` WHERE `entry` = 21421;
DELETE FROM `item_template_locale` WHERE `ID` = 21421;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 10399;
UPDATE `legendary_base` SET
    `entry` = 21421, `name` = 'Plastron de VanCleef', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 5,
    `sheath` = 0,
    `displayid` = 71012, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21421, 'frFR', 'Plastron de VanCleef', '', 0),
(21421, 'enUS', 'VanCleef''s Chestguard', '', 0);

-- 21429 Ceinture à poudre de Gilnid: copied from 10403, its look's item
DELETE FROM `item_template` WHERE `entry` = 21429;
DELETE FROM `item_template_locale` WHERE `ID` = 21429;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 10403;
UPDATE `legendary_base` SET
    `entry` = 21429, `name` = 'Ceinture à poudre de Gilnid', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 6,
    `sheath` = 0,
    `displayid` = 71013, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21429, 'frFR', 'Ceinture à poudre de Gilnid', '', 0),
(21429, 'enUS', 'Gilnid''s Powder Belt', '', 0);

-- 21444 Moufles de Cookie: copied from 12977, its look's item
DELETE FROM `item_template` WHERE `entry` = 21444;
DELETE FROM `item_template_locale` WHERE `ID` = 21444;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 12977;
UPDATE `legendary_base` SET
    `entry` = 21444, `name` = 'Moufles de Cookie', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 10,
    `sheath` = 0,
    `displayid` = 71014, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21444, 'frFR', 'Moufles de Cookie', '', 0),
(21444, 'enUS', 'Cookie''s Mitts', '', 0);

-- 18161 Bottes du roi Dred: copied from 35641, its look's item
DELETE FROM `item_template` WHERE `entry` = 18161;
DELETE FROM `item_template_locale` WHERE `ID` = 18161;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 35641;
UPDATE `legendary_base` SET
    `entry` = 18161, `name` = 'Bottes du roi Dred', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 7, `InventoryType` = 8,
    `sheath` = 0,
    `displayid` = 71015, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(18161, 'frFR', 'Bottes du roi Dred', '', 0),
(18161, 'enUS', 'King Dred''s Boots', '', 0);

-- 21430 Robe de Novos: copied from 35632, its look's item
DELETE FROM `item_template` WHERE `entry` = 21430;
DELETE FROM `item_template_locale` WHERE `ID` = 21430;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 35632;
UPDATE `legendary_base` SET
    `entry` = 21430, `name` = 'Robe de Novos', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 20,
    `sheath` = 0,
    `displayid` = 71016, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21430, 'frFR', 'Robe de Novos', '', 0),
(21430, 'enUS', 'Novos'' Robe', '', 0);

-- 27218 Pendentif de Tharon'ja: copied from 35631, its look's item
DELETE FROM `item_template` WHERE `entry` = 27218;
DELETE FROM `item_template_locale` WHERE `ID` = 27218;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 35631;
UPDATE `legendary_base` SET
    `entry` = 27218, `name` = 'Pendentif de Tharon''ja', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 3, `InventoryType` = 2,
    `sheath` = 0,
    `displayid` = 71017, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(27218, 'frFR', 'Pendentif de Tharon''ja', '', 0),
(27218, 'enUS', 'Tharon''ja''s Pendant', '', 0);

-- 21423 Jambières du Dévoreur: copied from 49794, its look's item
DELETE FROM `item_template` WHERE `entry` = 21423;
DELETE FROM `item_template_locale` WHERE `ID` = 21423;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 49794;
UPDATE `legendary_base` SET
    `entry` = 21423, `name` = 'Jambières du Dévoreur', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 7,
    `sheath` = 0,
    `displayid` = 71018, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21423, 'frFR', 'Jambières du Dévoreur', '', 0),
(21423, 'enUS', 'Devourer''s Legplates', '', 0);

-- 21434 Heaume de Bronjahm: copied from 37793, its look's item
DELETE FROM `item_template` WHERE `entry` = 21434;
DELETE FROM `item_template_locale` WHERE `ID` = 21434;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 37793;
UPDATE `legendary_base` SET
    `entry` = 21434, `name` = 'Heaume de Bronjahm', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 1,
    `sheath` = 0,
    `displayid` = 71019, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21434, 'frFR', 'Heaume de Bronjahm', '', 0),
(21434, 'enUS', 'Bronjahm''s Helm', '', 0);

-- 6673 Anneau de l'âme reflétée: copied from 49800, its look's item
DELETE FROM `item_template` WHERE `entry` = 6673;
DELETE FROM `item_template_locale` WHERE `ID` = 6673;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 49800;
UPDATE `legendary_base` SET
    `entry` = 6673, `name` = 'Anneau de l''âme reflétée', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 5, `InventoryType` = 11,
    `sheath` = 0,
    `displayid` = 71020, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(6673, 'frFR', 'Anneau de l''âme reflétée', '', 0),
(6673, 'enUS', 'Ring of the Mirrored Soul', '', 0);

-- 8688 Étincelle d'Ionar: copied from 43573, its look's item
DELETE FROM `item_template` WHERE `entry` = 8688;
DELETE FROM `item_template_locale` WHERE `ID` = 8688;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 43573;
UPDATE `legendary_base` SET
    `entry` = 8688, `name` = 'Étincelle d''Ionar', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 1, `InventoryType` = 12,
    `sheath` = 0,
    `displayid` = 71021, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(8688, 'frFR', 'Étincelle d''Ionar', '', 0),
(8688, 'enUS', 'Ionar''s Spark', '', 0);

-- 21450 Poings de Loken: copied from 36995, its look's item
DELETE FROM `item_template` WHERE `entry` = 21450;
DELETE FROM `item_template_locale` WHERE `ID` = 21450;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 36995;
UPDATE `legendary_base` SET
    `entry` = 21450, `name` = 'Poings de Loken', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 0, `InventoryType` = 10,
    `sheath` = 0,
    `displayid` = 71022, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(21450, 'frFR', 'Poings de Loken', '', 0),
(21450, 'enUS', 'Fists of Loken', '', 0);

-- 6674 Chevalière de Bjarngrim: copied from 36979, its look's item
DELETE FROM `item_template` WHERE `entry` = 6674;
DELETE FROM `item_template_locale` WHERE `ID` = 6674;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 36979;
UPDATE `legendary_base` SET
    `entry` = 6674, `name` = 'Chevalière de Bjarngrim', `Quality` = 5, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = 5, `InventoryType` = 11,
    `sheath` = 0,
    `displayid` = 71023, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(6674, 'frFR', 'Chevalière de Bjarngrim', '', 0),
(6674, 'enUS', 'Bjarngrim''s Signet', '', 0);

-- 10555 Écho du Néant: copied from 49800, its look's item
DELETE FROM `item_template` WHERE `entry` = 10555;
DELETE FROM `item_template_locale` WHERE `ID` = 10555;
DROP TEMPORARY TABLE IF EXISTS `legendary_base`;
CREATE TEMPORARY TABLE `legendary_base` SELECT * FROM `item_template` WHERE `entry` = 49800;
UPDATE `legendary_base` SET
    `entry` = 10555, `name` = 'Écho du Néant', `Quality` = 6, `ItemLevel` = 227, `RequiredLevel` = 80,
    `class` = 4, `subclass` = 0, `Material` = -1, `InventoryType` = 11,
    `sheath` = 0,
    `displayid` = 71024, `bonding` = 1, `armor` = 0,
    `ScalingStatDistribution` = 0, `ScalingStatValue` = 0,
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0, `stat_type3` = 0, `stat_value3` = 0,
    `stat_type4` = 0, `stat_value4` = 0, `stat_type5` = 0, `stat_value5` = 0, `stat_type6` = 0, `stat_value6` = 0,
    `stat_type7` = 0, `stat_value7` = 0, `stat_type8` = 0, `stat_value8` = 0, `stat_type9` = 0, `stat_value9` = 0,
    `stat_type10` = 0, `stat_value10` = 0,
    `spellid_1` = 0, `spellid_2` = 0, `spellid_3` = 0, `spellid_4` = 0, `spellid_5` = 0,
    `spelltrigger_1` = 0, `spelltrigger_2` = 0, `spelltrigger_3` = 0, `spelltrigger_4` = 0, `spelltrigger_5` = 0,
    `itemset` = 0, `SellPrice` = 0, `BuyPrice` = 0, `MaxDurability` = 0, `description` = '',
    `socketColor_1` = 0, `socketContent_1` = 0, `socketColor_2` = 0, `socketContent_2` = 0, `socketColor_3` = 0,
    `socketContent_3` = 0, `socketBonus` = 0, `GemProperties` = 0, `RandomProperty` = 0, `RandomSuffix` = 0,
    `AllowableClass` = -1, `AllowableRace` = -1, `Flags` = 0, `FlagsExtra` = 0, `ItemLimitCategory` = 0,
    `RequiredSkill` = 0, `RequiredSkillRank` = 0, `requiredspell` = 0, `RequiredReputationFaction` = 0,
    `RequiredReputationRank` = 0, `maxcount` = 0, `stackable` = 1;
INSERT INTO `item_template` SELECT * FROM `legendary_base`;
DROP TEMPORARY TABLE `legendary_base`;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(10555, 'frFR', 'Écho du Néant', '', 0),
(10555, 'enUS', 'Echo of the Void', '', 0);

-- Every legendary base's sockets: a stock epic's of its slot, with a stamina bonus
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 24567;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 996;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21428;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21424;
UPDATE `item_template` SET `socketColor_1` = 0, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 0 WHERE `entry` = 1258;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21432;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 21425;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 21420;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 26541;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21437;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21433;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 5828;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 21421;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 21429;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21444;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 18161;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 21430;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 27218;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 4, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3307 WHERE `entry` = 21423;
UPDATE `item_template` SET `socketColor_1` = 1, `socketColor_2` = 2, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 3766 WHERE `entry` = 21434;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 6673;
UPDATE `item_template` SET `socketColor_1` = 0, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 0 WHERE `entry` = 8688;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 21450;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 6674;
UPDATE `item_template` SET `socketColor_1` = 2, `socketColor_2` = 0, `socketColor_3` = 0,
    `socketContent_1` = 0, `socketContent_2` = 0, `socketContent_3` = 0, `socketBonus` = 2868 WHERE `entry` = 10555;
-- END generated
