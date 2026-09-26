-- The Forge (modules/mod-forge): the ranks forged on a Mythic+ item. A forged Mythic+ item becomes the next
-- variant, which says nothing of how often it was forged; a real item's rank is its entry and needs no row.
CREATE TABLE IF NOT EXISTS `character_item_forge` (
    `item_guid` INT UNSIGNED NOT NULL,
    `forge_rank` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
