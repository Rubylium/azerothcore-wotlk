-- The Forge (modules/mod-forge), gold well spent: every forged item keeps what it cost, in copper (its tooltip
-- shows it), and every character its standing with the smith, the copper it has paid him in all, which earns
-- discounts and a chance of a masterwork. character_item_forge now has a row for every forged item, not only the
-- Mythic+ ones.
ALTER TABLE `character_item_forge` ADD COLUMN `money_invested` INT UNSIGNED NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS `character_forge_patron` (
    `guid` INT UNSIGNED NOT NULL,
    `money_spent` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
