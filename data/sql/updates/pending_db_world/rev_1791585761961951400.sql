-- Cœur d'étoile captive (modules/mod-legendary Legendary.h, UpgradeMaterial): the only thing that reinforces a
-- legendary or a Unique at the Forge. Dropped by high-end content (item level 250 and up), on top of its loot. Bound
-- to whoever finds it; a free Item.dbc row made a trade good (localTools/patchSinisterStrike.ps1), copied from the
-- Frozen Orb (43102).
DROP TEMPORARY TABLE IF EXISTS `upgrade_material`;
CREATE TEMPORARY TABLE `upgrade_material` SELECT * FROM `item_template` WHERE `entry` = 43102;
UPDATE `upgrade_material` SET
    `entry` = 17854, `name` = 'Cœur d''étoile captive', `class` = 7, `subclass` = 11, `Material` = 4,
    `displayid` = 56461, `Quality` = 5, `bonding` = 1, `stackable` = 200, `maxcount` = 0, `ItemLevel` = 80,
    `RequiredLevel` = 0, `Flags` = 0, `BuyPrice` = 0, `SellPrice` = 0,
    `description` = 'Seul le feu d''une étoile peut reforger un objet légendaire. Confiez-le au maître forgeron.';
DELETE FROM `item_template` WHERE `entry` = 17854;
INSERT INTO `item_template` SELECT * FROM `upgrade_material`;
DROP TEMPORARY TABLE `upgrade_material`;
DELETE FROM `item_template_locale` WHERE `ID` = 17854;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(17854, 'frFR', 'Cœur d''étoile captive', 'Seul le feu d''une étoile peut reforger un objet légendaire. Confiez-le au maître forgeron.', 0),
(17854, 'enUS', 'Captive Star Heart', 'Only a star''s fire can reforge a legendary item. Bring it to the master blacksmith.', 0);
