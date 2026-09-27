-- The Infinite Dungeon (src/infinite/InfiniteDungeon.cpp): each character's progress on its two ladders, and where it
-- goes back to when a run ends.
-- - The levelling ladder is climbed below the level cap, the gearing ladder at it: a character that levelled to 80 in
--   the dungeon starts the gearing ladder at floor 1, and one that levels again after a prestige climbs the levelling
--   ladder again, from floor 1 if it asks.
-- - A checkpoint is the last tenth floor cleared: a run starts on the floor after it.
-- - in_run and return_*: a character that logs out during a run is sent back there when it logs in again.
CREATE TABLE IF NOT EXISTS `character_infinite_dungeon` (
    `guid` INT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `checkpoint` INT UNSIGNED NOT NULL DEFAULT 0,
    `best` INT UNSIGNED NOT NULL DEFAULT 0,
    `checkpoint_max` INT UNSIGNED NOT NULL DEFAULT 0,
    `best_max` INT UNSIGNED NOT NULL DEFAULT 0,
    `in_run` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `return_map` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `return_x` FLOAT NOT NULL DEFAULT 0,
    `return_y` FLOAT NOT NULL DEFAULT 0,
    `return_z` FLOAT NOT NULL DEFAULT 0,
    `return_o` FLOAT NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`),
    KEY `idx_best_max` (`best_max`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
