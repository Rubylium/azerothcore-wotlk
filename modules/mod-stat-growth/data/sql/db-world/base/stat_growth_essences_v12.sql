INSERT IGNORE INTO `mod_stat_growth_item_backup`
SELECT * FROM `item_template`
WHERE `entry` IN (1267, 1533, 1612, 3338, 3441, 3507);

INSERT IGNORE INTO `mod_stat_growth_item_locale_backup`
SELECT * FROM `item_template_locale`
WHERE `ID` IN (1267, 1533, 1612, 3338, 3441, 3507);

UPDATE `item_template` AS `item`
INNER JOIN `mod_stat_growth_item_backup` AS `backup` ON `backup`.`entry` = `item`.`entry`
SET `item`.`class` = `backup`.`class`,
    `item`.`subclass` = `backup`.`subclass`,
    `item`.`name` = `backup`.`name`,
    `item`.`displayid` = `backup`.`displayid`,
    `item`.`Quality` = `backup`.`Quality`,
    `item`.`Flags` = `backup`.`Flags`,
    `item`.`BuyCount` = `backup`.`BuyCount`,
    `item`.`InventoryType` = `backup`.`InventoryType`,
    `item`.`AllowableClass` = `backup`.`AllowableClass`,
    `item`.`AllowableRace` = `backup`.`AllowableRace`,
    `item`.`ItemLevel` = `backup`.`ItemLevel`,
    `item`.`RequiredLevel` = `backup`.`RequiredLevel`,
    `item`.`maxcount` = `backup`.`maxcount`,
    `item`.`stackable` = `backup`.`stackable`,
    `item`.`bonding` = `backup`.`bonding`,
    `item`.`description` = `backup`.`description`,
    `item`.`SoundOverrideSubclass` = `backup`.`SoundOverrideSubclass`,
    `item`.`spellid_1` = `backup`.`spellid_1`,
    `item`.`spelltrigger_1` = `backup`.`spelltrigger_1`,
    `item`.`spellcharges_1` = `backup`.`spellcharges_1`,
    `item`.`ScriptName` = `backup`.`ScriptName`,
    `item`.`VerifiedBuild` = `backup`.`VerifiedBuild`
WHERE `item`.`entry` IN (1444, 2478, 2803, 2804, 2948, 3015);

UPDATE `item_template_locale` AS `locale`
INNER JOIN `mod_stat_growth_item_locale_backup` AS `backup`
    ON `backup`.`ID` = `locale`.`ID` AND `backup`.`locale` = `locale`.`locale`
SET `locale`.`Name` = `backup`.`Name`, `locale`.`Description` = `backup`.`Description`
WHERE `locale`.`ID` IN (1444, 2478, 2803, 2804, 2948, 3015);

UPDATE `item_template`
SET `class` = CASE WHEN `entry` IN (1950, 2050) THEN 7 WHEN `entry` = 3338 THEN 12 ELSE 0 END,
    `subclass` = CASE
        WHEN `entry` IN (1950, 2050) THEN 7 WHEN `entry` = 2461 THEN 1
        WHEN `entry` IN (1267, 1533, 1612, 1704, 3441, 3507) THEN 8 ELSE 0
    END,
    `name` = CASE `entry`
        WHEN 1533 THEN 'Faint Essence of Growth' WHEN 1612 THEN 'Greater Essence of Growth'
        WHEN 3338 THEN 'Faint Essence of Wisdom' WHEN 1267 THEN 'Greater Essence of Wisdom'
        WHEN 2461 THEN 'Faint Essence of Flow' WHEN 3441 THEN 'Greater Essence of Flow'
        WHEN 1704 THEN 'Faint Essence of Vitality' WHEN 3507 THEN 'Greater Essence of Vitality'
        WHEN 2050 THEN 'Faint Essence of Fortune' WHEN 1950 THEN 'Greater Essence of Fortune'
    END,
    `displayid` = CASE `entry`
        WHEN 1533 THEN 6358 WHEN 1612 THEN 6368 WHEN 3338 THEN 7094 WHEN 1267 THEN 6359
        WHEN 2461 THEN 2357 WHEN 3441 THEN 2947 WHEN 1704 THEN 1504 WHEN 3507 THEN 983
        WHEN 2050 THEN 7355 WHEN 1950 THEN 7352
    END,
    `Quality` = CASE WHEN `entry` IN (1533, 1704, 2050, 2461, 3338) THEN 3 ELSE 4 END,
    `Flags` = 0,
    `BuyCount` = 1,
    `InventoryType` = 0,
    `AllowableClass` = -1,
    `AllowableRace` = -1,
    `ItemLevel` = 1,
    `RequiredLevel` = 1,
    `maxcount` = 0,
    `stackable` = 200,
    `bonding` = 1,
    `description` = CASE `entry`
        WHEN 1533 THEN 'A dim shard of living potential. Consume it to permanently gain +1 class-compatible stat.'
        WHEN 1612 THEN 'Raw potential churns beneath its surface. Consume it to permanently gain +3 class-compatible stats.'
        WHEN 3338 THEN 'A fading memory from another lifetime. Consume it to permanently increase experience gained by 10%.'
        WHEN 1267 THEN 'The knowledge of forgotten heroes burns within. Consume it to permanently increase experience gained by 30%.'
        WHEN 2461 THEN 'A weak current of primal energy. Consume it to permanently increase primary-resource regeneration by 1%.'
        WHEN 3441 THEN 'Primal power surges through this vessel. Consume it to permanently increase primary-resource regeneration by 3%.'
        WHEN 1704 THEN 'A distant heartbeat lingers within. Consume it to permanently increase maximum health by 1%.'
        WHEN 3507 THEN 'The relentless pulse of a giant fills this vessel. Consume it to permanently increase maximum health by 3%.'
        WHEN 2050 THEN 'A silver thread of favorable fate. Consume it to permanently increase gold and normal-loot gains by 1%.'
        WHEN 1950 THEN 'Fate twists around this golden fragment. Consume it to permanently increase gold and normal-loot gains by 3%.'
    END,
    `SoundOverrideSubclass` = -1,
    `spellid_1` = 5735,
    `spelltrigger_1` = 0,
    `spellcharges_1` = -1,
    `ScriptName` = CASE
        WHEN `entry` IN (1533, 1612) THEN 'item_stat_growth_essence'
        WHEN `entry` IN (1267, 3338) THEN 'item_experience_boost_essence'
        WHEN `entry` IN (2461, 3441) THEN 'item_resource_boost_essence'
        WHEN `entry` IN (1704, 3507) THEN 'item_vitality_boost_essence'
        WHEN `entry` IN (1950, 2050) THEN 'item_fortune_boost_essence'
    END,
    `VerifiedBuild` = 12340
WHERE `entry` IN (1267, 1533, 1612, 1704, 1950, 2050, 2461, 3338, 3441, 3507);

UPDATE `item_template_locale` AS `locale`
INNER JOIN `item_template` AS `item` ON `item`.`entry` = `locale`.`ID`
SET `locale`.`Name` = `item`.`name`, `locale`.`Description` = `item`.`description`
WHERE `locale`.`ID` IN (1267, 1533, 1612, 1704, 1950, 2050, 2461, 3338, 3441, 3507);
