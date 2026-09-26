-- Talent loadouts (TalentTree.cpp): builds a character saved under a name, applied back as far as its level allows.
-- `build` is "node:value" pairs, comma separated, so a loadout outlives a reshaped tree (unknown nodes are dropped).
CREATE TABLE IF NOT EXISTS `character_talent_loadout` (
    `guid` INT UNSIGNED NOT NULL,
    `slot` TINYINT UNSIGNED NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `specialization` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `build` VARCHAR(1024) NOT NULL DEFAULT '',
    PRIMARY KEY (`guid`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
