-- Legendary items (modules/mod-legendary): every copy's own rolls, by its item guid. Its item level, its power's
-- strength (percent) and its stats ("type=value,..." ITEM_MOD_*); its owner when it was made, for reference.
CREATE TABLE IF NOT EXISTS `character_legendary` (
    `item_guid` INT UNSIGNED NOT NULL,
    `owner_guid` INT UNSIGNED NOT NULL DEFAULT 0,
    `legendary` INT UNSIGNED NOT NULL,
    `item_level` INT UNSIGNED NOT NULL,
    `power` FLOAT NOT NULL,
    `armor` INT NOT NULL DEFAULT 0,
    `stats` VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
