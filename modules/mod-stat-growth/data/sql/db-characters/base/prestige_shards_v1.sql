-- Éclats de prestige (PrestigeShop.cpp): the currency a character earns while it levels again after a prestige,
-- spent on heirlooms at the prestige keeper. Heirlooms are bound to the account, so the éclats are too: one row
-- per account, whichever of its characters earned them.
CREATE TABLE IF NOT EXISTS `account_prestige_shards` (
    `account` INT UNSIGNED NOT NULL,
    `shards` INT UNSIGNED NOT NULL DEFAULT 0,          -- to spend
    `earned` INT UNSIGNED NOT NULL DEFAULT 0,          -- ever earned
    `kill_progress` INT UNSIGNED NOT NULL DEFAULT 0,   -- kills towards the next éclat
    PRIMARY KEY (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
