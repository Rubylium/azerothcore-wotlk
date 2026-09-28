-- Paragon glyphs (ParagonSystem.cpp): the glyphs each character has learnt. A row is a glyph in the collection, with
-- its level (1 to 25), its experience towards the next level, and the board node of the socket holding it (0: none).
-- A glyph gains experience only while socketed; a copy of one already learnt is absorbed as experience.
CREATE TABLE IF NOT EXISTS `character_paragon_glyph` (
    `guid` INT UNSIGNED NOT NULL,
    `glyph` TINYINT UNSIGNED NOT NULL,
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `experience` INT UNSIGNED NOT NULL DEFAULT 0,
    `socket` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `glyph`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
