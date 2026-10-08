-- mod-legendary copies made before their rows and rolls were fitted as raid items (Legendary.cpp FitSetPieces,
-- PersonalLootSystem.cpp ItemLevelOf). Run with worldserver stopped: gear bonuses are cached in memory.
-- Not idempotent (it grows values): never edit it once applied.
--
-- 1. A copy's gear bonuses were rolled at its base row's item level (227; 289 for the Marque de l'Inquisiteur,
--    legendary 1), not the copy's own (485 for Gardien-chef Vorhan's sets): about half what a raid item of the copy's
--    item level rolls. Every bonus grows linearly with the item level it is rolled at, so it is grown to the copy's;
--    Fortune (affix 21) stays within 1-2 (EssenceTuning MaxItemFortune). Affix 0 is none. Both tables are
--    mod-legendary's and mod-stat-growth's own: skipped on a database without them.
SET @legendary_tables := (
    SELECT COUNT(*) FROM information_schema.TABLES
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME IN ('mod_personal_loot_roll', 'character_legendary')
);
SET @grown := 'ROUND(`r`.`affix#_value` * `l`.`item_level` / IF(`l`.`legendary` = 1, 289, 227))';
SET @affix := REPLACE(CONCAT('`r`.`affix#_value` = IF(`r`.`affix#_type` = 0, `r`.`affix#_value`, ',
    'IF(`r`.`affix#_type` = 21, LEAST(2, GREATEST(1, @G)), GREATEST(1, @G)))'), '@G', @grown);
SET @legendary_rolls_sql := IF(@legendary_tables = 2, CONCAT(
    'UPDATE `mod_personal_loot_roll` `r` JOIN `character_legendary` `l` ON `l`.`item_guid` = `r`.`item_guid` SET ',
    REPLACE(@affix, '#', '1'), ', ', REPLACE(@affix, '#', '2'), ', ',
    REPLACE(@affix, '#', '3'), ', ', REPLACE(@affix, '#', '4')), 'SELECT 1');
PREPARE legendary_rolls_stmt FROM @legendary_rolls_sql;
EXECUTE legendary_rolls_stmt;
DEALLOCATE PREPARE legendary_rolls_stmt;

-- 2. Vorhan's set pieces had no durability: their copies were made with none, and would load broken now that the row
--    has some. Set above any maximum, the load brings it down to the row's (Item::LoadFromDB) and saves it.
UPDATE `item_instance` SET `durability` = 65535 WHERE `durability` = 0 AND `itemEntry` IN (
    13672, 13673, 13674, 13675, 13676, 13677, 13678, 13679, 13680, 13681, 13682, 13683, 13684, 13685, 13686,
    13687, 13688, 13689, 13690, 13691, 13692, 13693, 13694, 13695, 13696, 13697, 13710, 13711, 13712, 13713,
    13714, 13715, 13716, 13717, 12187);
