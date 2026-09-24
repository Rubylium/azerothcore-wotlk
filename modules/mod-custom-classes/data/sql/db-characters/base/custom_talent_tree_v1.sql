-- Retail-style talent trees: what each character has taken, per dual-specialization slot (TalentTree.cpp).

-- One row per node taken: `value` is the rank, or for a choice node the option (1 or 2). A node the trees no
-- longer have is ignored on login, which gives its points back.
CREATE TABLE IF NOT EXISTS `character_talent_tree` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `node` SMALLINT UNSIGNED NOT NULL,
    `value` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `spec`, `node`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
