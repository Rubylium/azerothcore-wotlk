-- Paragon levels (ParagonSystem.cpp): experience earned at the level cap fills a bar, and every level of it is a
-- paragon point. `level` is how many have been reached, `experience` the progress towards the next.
CREATE TABLE IF NOT EXISTS `character_paragon_experience` (
    `guid` INT UNSIGNED NOT NULL,
    `level` INT UNSIGNED NOT NULL DEFAULT 0,
    `experience` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
