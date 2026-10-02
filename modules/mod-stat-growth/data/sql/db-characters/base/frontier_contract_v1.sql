-- Le Front du Nord's daily contracts (src/frontier/FrontierQuartermaster.cpp): three per character and day, drawn when
-- the day turns, kept here so a restart keeps the day's progress.
CREATE TABLE IF NOT EXISTS `frontier_contract` (
    `guid` INT UNSIGNED NOT NULL,                   -- the character
    `slot` TINYINT UNSIGNED NOT NULL,               -- 0-2
    `day` INT UNSIGNED NOT NULL,                    -- the day it was drawn for (its turn, in days since 1970)
    `deed` TINYINT UNSIGNED NOT NULL,               -- 1 elite, 2 rift, 3 chest, 4 Colosse
    `zone` INT UNSIGNED NOT NULL DEFAULT 0,         -- where its deeds count; 0 anywhere
    `tier` TINYINT UNSIGNED NOT NULL,               -- 1-4: its pay
    `target` TINYINT UNSIGNED NOT NULL,
    `progress` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `done` TINYINT UNSIGNED NOT NULL DEFAULT 0,     -- paid
    PRIMARY KEY (`guid`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
