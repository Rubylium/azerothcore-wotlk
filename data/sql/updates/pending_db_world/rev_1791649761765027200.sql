-- Le Traqueur d'évadés's trinkets (modules/mod-stat-growth/src/RaidTrinkets.cpp): two for every damage dealer at item
-- level 485, a passive and an active. Free Item.dbc rows made trinkets (localTools/patchSinisterStrike.ps1), copied from
-- Sharpened Twilight Scale (54590, heroic, item level 284) as the other raid trinkets; their spells are
-- localTools/raidTrinkets/Spells.ps1's.
DROP TEMPORARY TABLE IF EXISTS `raid_trinket`;
CREATE TEMPORARY TABLE `raid_trinket` SELECT * FROM `item_template` WHERE `entry` = 54590;
-- 17856 Croc du gangrechien
UPDATE `raid_trinket` SET
    `entry` = 17856, `name` = 'Croc du gangrechien', `description` = 'Arraché à la meute du Traqueur. Il mord encore.', `ItemLevel` = 485,
    `displayid` = 959, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 32, `stat_value1` = 240,
    `spellid_1` = 94624, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17856;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17857 Cor de l'hallali
UPDATE `raid_trinket` SET
    `entry` = 17857, `name` = 'Cor de l''hallali', `description` = 'Quand il sonne, la traque est finie.', `ItemLevel` = 485,
    `displayid` = 13081, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 36, `stat_value1` = 240,
    `spellid_1` = 94626, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17857;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
DROP TEMPORARY TABLE `raid_trinket`;
DELETE FROM `item_template_locale` WHERE `ID` IN (17856, 17857);
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(17856, 'frFR', 'Croc du gangrechien', 'Arraché à la meute du Traqueur. Il mord encore.', 0),
(17856, 'enUS', 'Felhound Fang', 'Torn from the Hunter''s pack. It still bites.', 0),
(17857, 'frFR', 'Cor de l''hallali', 'Quand il sonne, la traque est finie.', 0),
(17857, 'enUS', 'Horn of the Kill', 'When it sounds, the hunt is over.', 0);

-- Croc du gangrechien's passive: every blow and harmful spell, no rest (as Vorhan's whetstone)
DELETE FROM `spell_proc` WHERE `SpellId` = 94624;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(94624, 0, 0, 0, 0, 0, 0x10154, 1, 2, 0, 0, 0, 0, 100, 0, 0);
