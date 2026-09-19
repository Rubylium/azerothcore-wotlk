-- Custom playable classes. A row makes the server treat that class id as playable: it borrows the combat
-- tables (crit, dodge, regen, rating scaling, attack power) of TemplateClass, so no game table needs new rows.
-- The client learns the class from ChrClasses.dbc in the client patch (localTools/customClasses).
DROP TABLE IF EXISTS `custom_class`;
CREATE TABLE `custom_class` (
  `ClassId` tinyint unsigned NOT NULL COMMENT 'ChrClasses.dbc id, 10 or 12-15',
  `TemplateClass` tinyint unsigned NOT NULL COMMENT 'Existing class whose combat formulas are used',
  `InheritSpells` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '1: starts from the template class spells and trainers, 0: only its own',
  `Roles` tinyint unsigned NOT NULL DEFAULT 8 COMMENT 'Dungeon Finder roles it may queue as: 2 tank, 4 healer, 8 damage',
  `StartLevel` tinyint unsigned NOT NULL DEFAULT 0 COMMENT 'Level a new character starts at, 0: the realm start level',
  `Name` varchar(50) NOT NULL DEFAULT '' COMMENT 'For logs and commands; the client shows its own name',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`ClassId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Custom playable classes';
