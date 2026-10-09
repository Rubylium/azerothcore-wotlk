-- The challenge board's satchel (mod-playerbots ChallengeBoard.cpp SatchelEntry 4573) came back: the test-item purge
-- (rev_1789757199356343300) lists 4573, a test item's entry in the stock client, and was once run again by hand after
-- the board had made the entry its own. With no template, every reward claim read as full bags. As made by
-- mod-playerbots 2026_09_23_00_challenge_board.sql.
DELETE FROM `item_loot_template` WHERE `Entry` = 4573;
DELETE FROM `item_template_locale` WHERE `ID` = 4573;
DELETE FROM `item_template` WHERE `entry` = 4573;

DROP TEMPORARY TABLE IF EXISTS `tmp_challenge_satchel`;
CREATE TEMPORARY TABLE `tmp_challenge_satchel` LIKE `item_template`;
-- The Dungeon Finder's Satchel of Helpful Goods: a container opened like loot, bound on pickup
INSERT INTO `tmp_challenge_satchel`
SELECT * FROM `item_template` WHERE `entry` = 52005;
UPDATE `tmp_challenge_satchel` SET
    `entry` = 4573,
    `name` = 'Challenge Satchel',
    `displayid` = 1244,
    `description` = 'A challenge won. It may hold one more of the boss\'s spoils.',
    `Quality` = 4,
    `ItemLevel` = 1,
    `RequiredLevel` = 0,
    `BuyPrice` = 0,
    `SellPrice` = 0,
    `minMoneyLoot` = 0,
    `maxMoneyLoot` = 0,
    `VerifiedBuild` = NULL;
INSERT INTO `item_template` SELECT * FROM `tmp_challenge_satchel`;
DROP TEMPORARY TABLE `tmp_challenge_satchel`;

INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(4573, 'frFR', 'Sacoche du défi', 'Un défi relevé. Elle peut contenir un butin de plus du boss vaincu.', NULL);

-- Runic Healing Potion
INSERT INTO `item_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`,
    `MinCount`, `MaxCount`, `Comment`) VALUES
(4573, 33447, 0, 100, 0, 1, 0, 1, 2, 'Challenge Satchel - Runic Healing Potion');
