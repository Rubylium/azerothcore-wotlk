-- Paragon: what each character has earned and where it has been spent (ParagonSystem.cpp).

-- One row per allocated node. Spent points are COUNT(*) of this table, so the two can never disagree.
CREATE TABLE IF NOT EXISTS `character_paragon` (
    `guid` INT UNSIGNED NOT NULL,
    `node` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `node`),
    KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Total points ever earned, including any past the cap.
--
-- Points keep dropping at the cap and bank here rather than being thrown away: the cap is meant to be raised
-- later, and a drop that silently does nothing is the worst way to reward a boss kill. Available to spend is
-- min(earned, cap) - spent, so banked points appear the moment the cap moves.
CREATE TABLE IF NOT EXISTS `character_paragon_points` (
    `guid` INT UNSIGNED NOT NULL,
    `earned` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
