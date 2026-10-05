-- Faux usée (Worn Scythe): the Faucheur's starting weapon (localTools/customClasses/classes.json, class 15), a
-- two-hand polearm wearing the stock scythe model (display 33086). Item 4901 is a Blizzard test item removed by
-- rev_1789757199356343300 that the client's Item.dbc still knows as a two-hand polearm, so it shows its own icon.
-- Built on the Worn Battleaxe (12282), the warrior's level 1 two-hander.
DELETE FROM `item_template` WHERE `entry` = 4901;
DROP TEMPORARY TABLE IF EXISTS `reaper_scythe`;
CREATE TEMPORARY TABLE `reaper_scythe` SELECT * FROM `item_template` WHERE `entry` = 12282;
UPDATE `reaper_scythe` SET `entry` = 4901, `subclass` = 6, `name` = 'Worn Scythe', `displayid` = 33086,
    `AllowableClass` = 16384, `dmg_min1` = 3, `dmg_max1` = 6, `delay` = 3000;
INSERT INTO `item_template` SELECT * FROM `reaper_scythe`;
DROP TEMPORARY TABLE `reaper_scythe`;

DELETE FROM `item_template_locale` WHERE `ID` = 4901;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
    (4901, 'frFR', 'Faux usée', NULL, 0);
