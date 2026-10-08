-- Set pieces' profiles (modules/mod-legendary SetPieces.cpp): for each of a set's rows, the raid item each profile is
-- made from. A profile's number is its items' entry (Mythic::GetSetPieceItemEntry): given once at startup and kept
-- for good, so a piece stays what it was whatever raid items come later. Never edit or delete rows.
CREATE TABLE IF NOT EXISTS `legendary_set_profile` (
    `item` INT UNSIGNED NOT NULL,
    `profile` TINYINT UNSIGNED NOT NULL,
    `donor` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`item`, `profile`),
    UNIQUE KEY `item_donor` (`item`, `donor`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
