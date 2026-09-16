DELETE FROM `item_template` WHERE (`entry` IN (39163, 42590, 900000));
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`,
 `description`, `Material`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `ScriptName`, `VerifiedBuild`)
VALUES
(42590, 0, 0, 'Essence of Growth', 33681, 3, 0, 1, 0, -1, -1, 1, 1, 0, 200, 1,
 'Use: Permanently gain a class-compatible stat. No growth limit.', -1, 483, 0, -1,
 'item_stat_growth_essence', 12340),
(39163, 0, 0, 'Essence of Experience', 51565, 3, 0, 1, 0, -1, -1, 1, 1, 0, 200, 1,
 'Use: Permanently gain 10% experience. Stacks without a gameplay limit.', 4, 483, 0, -1,
 'item_experience_boost_essence', 12340);
