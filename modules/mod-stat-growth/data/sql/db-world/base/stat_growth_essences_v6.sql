DELETE FROM `item_template` WHERE (`entry` = 21238);
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`,
 `description`, `Material`, `SoundOverrideSubclass`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`,
 `ScriptName`, `VerifiedBuild`)
VALUES
(21238, 0, 0, 'Essence of Resource', 33555, 3, 0, 1, 0, -1, -1, 1, 1, 0, 200, 1,
 'Use: Permanently gain 1% primary-resource regeneration. Stacks without a gameplay limit.', -1, -1,
 46168, 0, -1, 'item_resource_boost_essence', 12340);
