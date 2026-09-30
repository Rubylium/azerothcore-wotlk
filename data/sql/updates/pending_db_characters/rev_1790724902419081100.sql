-- Normalize existing Resource/Fortune bonuses to the hard limits in EssenceTuning.h.
-- Run with worldserver stopped: player settings and personal-loot rolls are cached in memory.
-- Item templates, other affixes, unspent essences and currency balances are not changed.
-- Only setting index 0 is capped; preserve any later settings and their serialization.
UPDATE `character_settings`
SET `data` = CONCAT('500', SUBSTRING(LTRIM(`data`), LENGTH(SUBSTRING_INDEX(LTRIM(`data`), ' ', 1)) + 1))
WHERE `source` = 'mod_stat_growth_resource'
    AND SUBSTRING_INDEX(LTRIM(`data`), ' ', 1) REGEXP '^[0-9]+$'
    AND CAST(SUBSTRING_INDEX(LTRIM(`data`), ' ', 1) AS UNSIGNED) > 500;

UPDATE `character_settings`
SET `data` = CONCAT('100', SUBSTRING(LTRIM(`data`), LENGTH(SUBSTRING_INDEX(LTRIM(`data`), ' ', 1)) + 1))
WHERE `source` = 'mod_stat_growth_fortune'
    AND SUBSTRING_INDEX(LTRIM(`data`), ' ', 1) REGEXP '^[0-9]+$'
    AND CAST(SUBSTRING_INDEX(LTRIM(`data`), ' ', 1) AS UNSIGNED) > 100;

-- mod-stat-growth is optional and its table may not exist yet on a fresh installation.
-- PersonalLootAffix stores ResourceRegeneration=19 and Fortune=21; never renumber these IDs.
SET @statGrowthBonusMigration = IF(EXISTS (
    SELECT 1 FROM `information_schema`.`TABLES`
    WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'mod_personal_loot_roll'
), 'UPDATE `mod_personal_loot_roll`
SET `affix1_value` = CASE `affix1_type`
        WHEN 21 THEN LEAST(`affix1_value`, 2) WHEN 19 THEN LEAST(`affix1_value`, 500) ELSE `affix1_value` END,
    `affix2_value` = CASE `affix2_type`
        WHEN 21 THEN LEAST(`affix2_value`, 2) WHEN 19 THEN LEAST(`affix2_value`, 500) ELSE `affix2_value` END,
    `affix3_value` = CASE `affix3_type`
        WHEN 21 THEN LEAST(`affix3_value`, 2) WHEN 19 THEN LEAST(`affix3_value`, 500) ELSE `affix3_value` END,
    `affix4_value` = CASE `affix4_type`
        WHEN 21 THEN LEAST(`affix4_value`, 2) WHEN 19 THEN LEAST(`affix4_value`, 500) ELSE `affix4_value` END
WHERE (`affix1_type` = 21 AND `affix1_value` > 2) OR (`affix1_type` = 19 AND `affix1_value` > 500)
    OR (`affix2_type` = 21 AND `affix2_value` > 2) OR (`affix2_type` = 19 AND `affix2_value` > 500)
    OR (`affix3_type` = 21 AND `affix3_value` > 2) OR (`affix3_type` = 19 AND `affix3_value` > 500)
    OR (`affix4_type` = 21 AND `affix4_value` > 2) OR (`affix4_type` = 19 AND `affix4_value` > 500)', 'DO 0');
PREPARE statGrowthBonusMigration FROM @statGrowthBonusMigration;
EXECUTE statGrowthBonusMigration;
DEALLOCATE PREPARE statGrowthBonusMigration;
