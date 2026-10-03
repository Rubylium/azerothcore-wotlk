-- Points unlocked past Paragon.PointCap by the prestiges (each one now unlocks everything banked, rather than
-- raising the cap by a fixed 20). Idempotent, as 2026_09_22_00.sql: a database created from the current base already
-- has the column.
SET @unlocked_column := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'character_paragon_points'
      AND COLUMN_NAME = 'unlocked'
);
SET @unlocked_sql := IF(@unlocked_column = 0,
    'ALTER TABLE `character_paragon_points` ADD COLUMN `unlocked` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `prestige`',
    'SELECT 1');
PREPARE unlocked_stmt FROM @unlocked_sql;
EXECUTE unlocked_stmt;
DEALLOCATE PREPARE unlocked_stmt;

-- Nobody loses a point they could spend: the old cap was 10 + 20 per prestige, the new base is 20
UPDATE `character_paragon_points` SET `unlocked` = GREATEST(`unlocked`, `prestige` * 20 - 10) WHERE `prestige` > 0;
