-- Prestige count on the paragon points row. Idempotent: a database created from the current base already
-- has the column, and this file still has to run there because the updater applies every unseen file.
SET @prestige_column := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'character_paragon_points'
      AND COLUMN_NAME = 'prestige'
);
SET @prestige_sql := IF(@prestige_column = 0,
    'ALTER TABLE `character_paragon_points` ADD COLUMN `prestige` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `earned`',
    'SELECT 1');
PREPARE prestige_stmt FROM @prestige_sql;
EXECUTE prestige_stmt;
DEALLOCATE PREPARE prestige_stmt;
