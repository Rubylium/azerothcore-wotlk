DELETE FROM `item_template` WHERE (`entry` = 900000);
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`,
 `description`, `Material`, `ScriptName`, `VerifiedBuild`)
VALUES
(900000, 0, 0, 'Essence of Growth', 40753, 3, 0, 1, 0, -1, -1, 1, 1, 0, 200, 1,
 'Use: Permanently gain a class-compatible stat. No growth limit.', 0, 'item_stat_growth_essence', 12340);
