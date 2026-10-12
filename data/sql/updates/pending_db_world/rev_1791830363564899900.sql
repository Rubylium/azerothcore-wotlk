-- Valérian's trinkets (modules/mod-stat-growth/src/RaidTrinkets.cpp): four at item level 485, one for each role. Free
-- Item.dbc rows made trinkets (localTools/patchSinisterStrike.ps1), copied from Sharpened Twilight Scale (54590, heroic,
-- item level 284) as the other raid trinkets, without its item limit category; their spells are
-- localTools/raidTrinkets/Spells.ps1's, what those cannot say modules/mod-stat-growth/src/RaidTrinketSpells.cpp's.
DROP TEMPORARY TABLE IF EXISTS `raid_trinket`;
CREATE TEMPORARY TABLE `raid_trinket` SELECT * FROM `item_template` WHERE `entry` = 54590;
-- 17859 Lance de la Litanie
UPDATE `raid_trinket` SET
    `entry` = 17859, `name` = 'Lance de la Litanie', `description` = 'Le Zélote la lançait au dernier verset.', `ItemLevel` = 485,
    `displayid` = 42993, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0, `ItemLimitCategory` = 0,
    `stat_type1` = 32, `stat_value1` = 240,
    `spellid_1` = 94550, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17859;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17860 Psaume du Zélote
UPDATE `raid_trinket` SET
    `entry` = 17860, `name` = 'Psaume du Zélote', `description` = 'Chaque verset plus fort que le précédent.', `ItemLevel` = 485,
    `displayid` = 34961, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0, `ItemLimitCategory` = 0,
    `stat_type1` = 45, `stat_value1` = 367,
    `spellid_1` = 94552, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17860;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17861 Sceau du Chœur
UPDATE `raid_trinket` SET
    `entry` = 17861, `name` = 'Sceau du Chœur', `description` = 'Une seule voix chante, et toutes répondent.', `ItemLevel` = 485,
    `displayid` = 40108, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0, `ItemLimitCategory` = 0,
    `stat_type1` = 45, `stat_value1` = 367,
    `spellid_1` = 94555, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17861;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17862 Chaîne brisée
UPDATE `raid_trinket` SET
    `entry` = 17862, `name` = 'Chaîne brisée', `description` = 'Elle a retenu le Zélote. Plus maintenant.', `ItemLevel` = 485,
    `displayid` = 9635, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0, `ItemLimitCategory` = 0,
    `stat_type1` = 7, `stat_value1` = 441,
    `spellid_1` = 94557, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17862;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
DROP TEMPORARY TABLE `raid_trinket`;
DELETE FROM `item_template_locale` WHERE `ID` IN (17859, 17860, 17861, 17862);
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(17859, 'frFR', 'Lance de la Litanie', 'Le Zélote la lançait au dernier verset.', 0),
(17859, 'enUS', 'Spear of the Litany', 'The Zealot hurled it at the last verse.', 0),
(17860, 'frFR', 'Psaume du Zélote', 'Chaque verset plus fort que le précédent.', 0),
(17860, 'enUS', 'Zealot''s Psalm', 'Each verse louder than the last.', 0),
(17861, 'frFR', 'Sceau du Chœur', 'Une seule voix chante, et toutes répondent.', 0),
(17861, 'enUS', 'Seal of the Choir', 'A single voice sings, and all answer.', 0),
(17862, 'frFR', 'Chaîne brisée', 'Elle a retenu le Zélote. Plus maintenant.', 0),
(17862, 'enUS', 'Broken Chain', 'It held the Zealot. Not anymore.', 0);

-- Their procs: Lance de la Litanie's blows (15%, 3 s rest), Psaume du Zélote's harmful spells (each cast once, its
-- phase), Sceau du Chœur's direct heals (each heal's hit)
DELETE FROM `spell_proc` WHERE `SpellId` IN (94550, 94552, 94555);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(94550, 0, 0, 0, 0, 0, 0x154, 1, 2, 0, 0, 0, 0, 15, 3000, 0),
(94552, 0, 0, 0, 0, 0, 0x10000, 7, 1, 0, 0, 0, 0, 100, 0, 0),
(94555, 0, 0, 0, 0, 0, 0x4000, 2, 2, 0, 0, 0, 0, 100, 0, 0);

-- What their spell data cannot say (RaidTrinketSpells.cpp)
DELETE FROM `spell_script_names` WHERE `spell_id` IN (94551, 94552, 94555, 94556, 94557);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(94551, 'RaidTrinketLitanySpearScript'),
(94552, 'RaidTrinketZealotPsalmScript'),
(94555, 'RaidTrinketChoirSealScript'),
(94556, 'RaidTrinketChoirEchoScript'),
(94557, 'RaidTrinketBrokenChainScript');
