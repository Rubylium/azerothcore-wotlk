-- Legendaries reinforced at the Forge (modules/mod-legendary Legendary.cpp, Reinforce): each copy's attempts failed
-- since its last success, which make the next one likelier. Gone with the item, or on a success.
CREATE TABLE IF NOT EXISTS `character_legendary_upgrade` (
    `item_guid` INT UNSIGNED NOT NULL,
    `fails` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
