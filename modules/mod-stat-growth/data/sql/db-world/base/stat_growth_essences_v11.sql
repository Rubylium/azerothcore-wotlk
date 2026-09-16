CREATE TABLE IF NOT EXISTS `mod_stat_growth_item_backup` LIKE `item_template`;
INSERT IGNORE INTO `mod_stat_growth_item_backup`
SELECT * FROM `item_template`
WHERE `entry` IN (1444, 1704, 1950, 2050, 2461, 2478, 2803, 2804, 2948, 3015);

CREATE TABLE IF NOT EXISTS `mod_stat_growth_item_locale_backup` LIKE `item_template_locale`;
INSERT IGNORE INTO `mod_stat_growth_item_locale_backup`
SELECT * FROM `item_template_locale`
WHERE `ID` IN (1444, 1704, 1950, 2050, 2461, 2478, 2803, 2804, 2948, 3015);

UPDATE `item_template`
SET `class` = 0,
    `subclass` = 0,
    `name` = CASE `entry`
        WHEN 1444 THEN 'Faint Essence of Growth'
        WHEN 3015 THEN 'Greater Essence of Growth'
        WHEN 2803 THEN 'Faint Essence of Wisdom'
        WHEN 2948 THEN 'Greater Essence of Wisdom'
        WHEN 2461 THEN 'Faint Essence of Flow'
        WHEN 2478 THEN 'Greater Essence of Flow'
        WHEN 1704 THEN 'Faint Essence of Vitality'
        WHEN 2804 THEN 'Greater Essence of Vitality'
        WHEN 2050 THEN 'Faint Essence of Fortune'
        WHEN 1950 THEN 'Greater Essence of Fortune'
    END,
    `displayid` = CASE `entry`
        WHEN 1444 THEN 6504 WHEN 3015 THEN 18109 WHEN 2803 THEN 6514 WHEN 2948 THEN 6515
        WHEN 2461 THEN 2357 WHEN 2478 THEN 1805 WHEN 1704 THEN 1504 WHEN 2804 THEN 6502
        WHEN 2050 THEN 7355 WHEN 1950 THEN 7352
    END,
    `Quality` = CASE WHEN `entry` IN (1444, 1704, 2050, 2461, 2803) THEN 3 ELSE 4 END,
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
        WHEN 1444 THEN 'A dim shard of living potential. Consume it to permanently gain +1 class-compatible stat.'
        WHEN 3015 THEN 'Raw potential churns beneath its surface. Consume it to permanently gain +3 class-compatible stats.'
        WHEN 2803 THEN 'A fading memory from another lifetime. Consume it to permanently increase experience gained by 10%.'
        WHEN 2948 THEN 'The knowledge of forgotten heroes burns within. Consume it to permanently increase experience gained by 30%.'
        WHEN 2461 THEN 'A weak current of primal energy. Consume it to permanently increase primary-resource regeneration by 1%.'
        WHEN 2478 THEN 'Primal power surges through this vessel. Consume it to permanently increase primary-resource regeneration by 3%.'
        WHEN 1704 THEN 'A distant heartbeat lingers within. Consume it to permanently increase maximum health by 1%.'
        WHEN 2804 THEN 'The relentless pulse of a giant shakes this talisman. Consume it to permanently increase maximum health by 3%.'
        WHEN 2050 THEN 'A silver thread of favorable fate. Consume it to permanently increase gold and normal-loot gains by 1%.'
        WHEN 1950 THEN 'Fate twists around this golden fragment. Consume it to permanently increase gold and normal-loot gains by 3%.'
    END,
    `SoundOverrideSubclass` = -1,
    `spellid_1` = 5735,
    `spelltrigger_1` = 0,
    `spellcharges_1` = -1,
    `ScriptName` = CASE
        WHEN `entry` IN (1444, 3015) THEN 'item_stat_growth_essence'
        WHEN `entry` IN (2803, 2948) THEN 'item_experience_boost_essence'
        WHEN `entry` IN (2461, 2478) THEN 'item_resource_boost_essence'
        WHEN `entry` IN (1704, 2804) THEN 'item_vitality_boost_essence'
        WHEN `entry` IN (1950, 2050) THEN 'item_fortune_boost_essence'
    END,
    `VerifiedBuild` = 12340
WHERE `entry` IN (1444, 1704, 1950, 2050, 2461, 2478, 2803, 2804, 2948, 3015);

UPDATE `item_template`
SET `Quality` = 5,
    `description` = CASE `entry`
        WHEN 42590 THEN 'A living shard of limitless potential. Consume it to permanently gain +10 class-compatible stats.'
        WHEN 39163 THEN 'Knowledge gathered across countless lifetimes. Consume it to permanently increase experience gained by 100%.'
        WHEN 21238 THEN 'An inexhaustible torrent of primal energy. Consume it to permanently increase primary-resource regeneration by 10%.'
        WHEN 41606 THEN 'The heartbeat of an undying force echoes within. Consume it to permanently increase maximum health by 10%.'
        WHEN 23656 THEN 'Fate kneels before this gilded fragment. Consume it to permanently increase gold and normal-loot gains by 10%.'
    END
WHERE `entry` IN (21238, 23656, 39163, 41606, 42590);

UPDATE `item_template_locale` AS `locale`
INNER JOIN `item_template` AS `item` ON `item`.`entry` = `locale`.`ID`
SET `locale`.`Name` = `item`.`name`, `locale`.`Description` = `item`.`description`
WHERE `locale`.`ID` IN (1444, 1704, 1950, 2050, 2461, 2478, 2803, 2804, 2948, 3015,
                        21238, 23656, 39163, 41606, 42590);
