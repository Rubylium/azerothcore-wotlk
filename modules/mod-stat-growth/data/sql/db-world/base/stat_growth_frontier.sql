-- Le Front du Nord (src/frontier/Frontier.cpp): open-world content at level 80, in the tier phase (0x4000).
-- Plan: .agents/plans/northrend-frontier/northrend-frontier.PLAN.md
--
-- The roaming elites (940000-940015): copies of stock Northrend creatures under their own entries, two a tier zone,
-- each keeping its model, weapons and movement; its own French name. Frontier.cpp sets their level, health and
-- damage to the zone's tier when they spawn, and their loot. No stock loot, no SmartAI: the script drives them.

DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN 940000 AND 940015;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 940000 AND 940015;
DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 940000 AND 940015;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 940000 AND 940015;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 940000 AND 940015;

DROP TEMPORARY TABLE IF EXISTS `tmp_frontier_copy`;
CREATE TEMPORARY TABLE `tmp_frontier_copy` (
    `entry` INT UNSIGNED NOT NULL,
    `source` INT UNSIGNED NOT NULL,
    `name` VARCHAR(100) NOT NULL,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB;
INSERT INTO `tmp_frontier_copy` (`entry`, `source`, `name`) VALUES
    -- Palier I: Borean Tundra, Howling Fjord
    (940000, 24954, 'Magnataure ancien'),
    (940001, 36874, 'Revenant glaciaire'),
    (940002, 23961, 'Heaume-de-fer Écorche-dragon'),
    (940003, 23960, 'Lanceur de runes Écorche-dragon'),
    -- Palier II: Dragonblight, Grizzly Hills
    (940004, 27605, 'Abomination colossale'),
    (940005, 34920, 'Jormungar des profondeurs'),
    (940006, 26482, 'Grizzly arctique ancien'),
    (940007, 27020, 'Worgen de la Lune-de-sang'),
    -- Palier III: Zul'Drak, Sholazar Basin
    (940008, 29836, 'Chevaucheur de guerre drakkari'),
    (940009, 28418, 'Trappeur drakkari'),
    (940010, 28078, 'Ravageur Frénécœur'),
    (940011, 28086, 'Guêpe-reine saphir'),
    -- Palier IV: Storm Peaks, Icecrown
    (940012, 30320, 'Givreloup balafré'),
    (940013, 29978, 'Assaillant nain de fer'),
    (940014, 30689, 'Abomination enchaînée'),
    (940015, 32255, 'Héros converti');

DROP TEMPORARY TABLE IF EXISTS `tmp_frontier_template`;
CREATE TEMPORARY TABLE `tmp_frontier_template` LIKE `creature_template`;
INSERT INTO `tmp_frontier_template` SELECT t.* FROM `creature_template` t JOIN `tmp_frontier_copy` c ON c.`source` = t.`entry`;
UPDATE `tmp_frontier_template` t JOIN `tmp_frontier_copy` c ON c.`source` = t.`entry` SET t.`entry` = c.`entry`;
UPDATE `tmp_frontier_template` t JOIN `tmp_frontier_copy` c ON c.`entry` = t.`entry` SET
    t.`name` = c.`name`, t.`subname` = 'Élite du Front du Nord', t.`IconName` = '',
    t.`difficulty_entry_1` = 0, t.`difficulty_entry_2` = 0, t.`difficulty_entry_3` = 0,
    t.`KillCredit1` = 0, t.`KillCredit2` = 0, t.`gossip_menu_id` = 0, t.`npcflag` = 0, t.`faction` = 14,
    t.`rank` = 1, t.`minlevel` = 81, t.`maxlevel` = 81, t.`exp` = 2,
    t.`unit_flags` = 0, t.`unit_flags2` = 0, t.`dynamicflags` = 0, t.`VehicleId` = 0,
    t.`lootid` = 0, t.`pickpocketloot` = 0, t.`skinloot` = 0, t.`mingold` = 0, t.`maxgold` = 0,
    t.`AIName` = '', t.`MovementType` = 0, t.`HealthModifier` = 1, t.`ManaModifier` = 1,
    t.`ArmorModifier` = 1, t.`DamageModifier` = 1, t.`ExperienceModifier` = 0, t.`RegenHealth` = 1,
    t.`CreatureImmunitiesId` = 0, t.`flags_extra` = 0, t.`ScriptName` = 'npc_frontier_elite',
    t.`detection_range` = 18, t.`VerifiedBuild` = NULL;
INSERT INTO `creature_template` SELECT * FROM `tmp_frontier_template`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT c.`entry`, m.`Idx`, m.`CreatureDisplayID`, m.`DisplayScale` * 1.25, m.`Probability`, NULL
FROM `tmp_frontier_copy` c JOIN `creature_template_model` m ON m.`CreatureID` = c.`source`;

INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
SELECT c.`entry`, e.`ID`, e.`ItemID1`, e.`ItemID2`, e.`ItemID3`, NULL
FROM `tmp_frontier_copy` c JOIN `creature_equip_template` e ON e.`CreatureID` = c.`source`;

INSERT INTO `creature_template_movement`
    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
SELECT c.`entry`, v.`Ground`, v.`Swim`, v.`Flight`, 0, v.`Chase`, 0, v.`InteractionPauseTimer`
FROM `tmp_frontier_copy` c JOIN `creature_template_movement` v ON v.`CreatureId` = c.`source`;

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`)
SELECT c.`entry`, 'frFR', c.`name`, 'Élite du Front du Nord', NULL FROM `tmp_frontier_copy` c;

DROP TEMPORARY TABLE `tmp_frontier_template`;
DROP TEMPORARY TABLE `tmp_frontier_copy`;

-- Éclat de givre, the Front du Nord's currency: item 37711, a row the client's Item.dbc already has (a currency
-- token, class 10) with no template on the server. Its icon is that row's own until the patcher gives it the shard's.
DELETE FROM `item_template` WHERE `entry` = 37711;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`,
    `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`,
    `description`) VALUES
(37711, 10, 0, 'Éclat de givre', 32278, 3, 0, 1, 0, 0, 0, 80, 0, 0, 1000, 1,
    'Arraché aux menaces du Front du Nord. Le quartier-maître de Dalaran l''échange contre de l''équipement.');
DELETE FROM `item_template_locale` WHERE `ID` = 37711;
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(37711, 'frFR', 'Éclat de givre',
    'Arraché aux menaces du Front du Nord. Le quartier-maître de Dalaran l''échange contre de l''équipement.', NULL);
