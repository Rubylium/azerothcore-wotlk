-- The loot collection (modules/mod-legendary LootCollection.cpp): the pieces a character ever looted from our own
-- bosses (a piece: its item row, an entry's low word), kept for good whatever became of the item.
CREATE TABLE IF NOT EXISTS `character_loot_collection` (
    `guid` INT UNSIGNED NOT NULL,
    `piece` INT UNSIGNED NOT NULL,
    `at` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `piece`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- The legendaries and Uniques an account's characters ever looted: seen, shown on all of them (not collected)
CREATE TABLE IF NOT EXISTS `account_loot_seen` (
    `account` INT UNSIGNED NOT NULL,
    `piece` INT UNSIGNED NOT NULL,
    `at` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`account`, `piece`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
