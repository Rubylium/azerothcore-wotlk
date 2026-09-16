DELETE FROM `item_template` WHERE (`entry` IN (23656, 41606));
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`,
 `description`, `Material`, `SoundOverrideSubclass`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`,
 `ScriptName`, `VerifiedBuild`)
VALUES
(41606, 0, 6, 'Essence of Vitality', 42717, 3, 0, 1, 0, -1, -1, 1, 1, 0, 200, 1,
 'Use: Permanently gain 1% maximum health. Stacks without a gameplay limit.', 4, -1,
 5735, 0, -1, 'item_vitality_boost_essence', 12340),
(23656, 0, 8, 'Essence of Fortune', 34744, 3, 0, 1, 0, -1, -1, 1, 1, 0, 200, 1,
 'Use: Permanently gain 1% gold and normal-loot quantity. Stacks without a gameplay limit.', 0, -1,
 5735, 0, -1, 'item_fortune_boost_essence', 12340);

DELETE FROM `item_template_locale` WHERE (`ID` IN (23656, 41606));
