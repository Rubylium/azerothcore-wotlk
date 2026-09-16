-- Fortune now improves loot quality and gear bonuses instead of multiplying item counts
UPDATE `item_template`
SET `description` = CASE `entry`
        WHEN 2050 THEN 'A silver thread of favorable fate. Consume it to permanently increase gold gains and loot quality by 1%.'
        WHEN 1950 THEN 'Fate twists around this golden fragment. Consume it to permanently increase gold gains and loot quality by 3%.'
        WHEN 23656 THEN 'Fate kneels before this gilded fragment. Consume it to permanently increase gold gains and loot quality by 10%.'
    END
WHERE `entry` IN (1950, 2050, 23656);

UPDATE `item_template_locale` AS `locale`
INNER JOIN `item_template` AS `item` ON `item`.`entry` = `locale`.`ID`
SET `locale`.`Description` = `item`.`description`
WHERE `locale`.`ID` IN (1950, 2050, 23656);
