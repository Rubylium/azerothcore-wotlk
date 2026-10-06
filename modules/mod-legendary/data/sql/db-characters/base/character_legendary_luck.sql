-- Legendary drops' bad luck protection (modules/mod-legendary): per character and per source (the Dungeon Finder
-- dungeon whose Mythic+ keys drop it), the keys completed there since its last legendary.
CREATE TABLE IF NOT EXISTS `character_legendary_luck` (
    `guid` INT UNSIGNED NOT NULL,
    `source` INT UNSIGNED NOT NULL,
    `misses` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `source`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
