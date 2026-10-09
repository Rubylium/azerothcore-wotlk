-- The paragon board's loadouts (mod-stat-growth ParagonSystem.cpp): boards a player saves under a name and switches to
-- for free, as talents keep theirs. Nodes and sockets as comma lists ("glyph:socket" for a socket).
CREATE TABLE IF NOT EXISTS `character_paragon_loadout` (
  `guid` INT UNSIGNED NOT NULL,
  `slot` TINYINT UNSIGNED NOT NULL,
  `name` VARCHAR(32) NOT NULL DEFAULT '',
  `nodes` TEXT NOT NULL,
  `sockets` TEXT NOT NULL,
  PRIMARY KEY (`guid`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Paragon board loadouts';
