-- The raid trinkets (modules/mod-stat-growth/src/RaidTrinkets.cpp): eight of the Hollow Voice (item level 477)
-- and eight of Gardien-chef Vorhan (485), a passive and an active for each role. Free Item.dbc rows made trinkets
-- (localTools/patchSinisterStrike.ps1), copied from Sharpened Twilight Scale (54590, heroic, item level 284); their
-- spells are localTools/raidTrinkets/Spells.ps1's.
DROP TEMPORARY TABLE IF EXISTS `raid_trinket`;
CREATE TEMPORARY TABLE `raid_trinket` SELECT * FROM `item_template` WHERE `entry` = 54590;
-- 17836 Éclat du Marteau béni
UPDATE `raid_trinket` SET
    `entry` = 17836, `name` = 'Éclat du Marteau béni', `description` = 'Un éclat de l''un des marteaux de l''Archevêque, encore tiède.', `ItemLevel` = 477,
    `displayid` = 53122, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 32, `stat_value1` = 238,
    `spellid_1` = 94600, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17836;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17837 Penne du Séraphin
UPDATE `raid_trinket` SET
    `entry` = 17837, `name` = 'Penne du Séraphin', `description` = 'Arrachée aux ailes du Séraphin au plus fort du sermon.', `ItemLevel` = 477,
    `displayid` = 62936, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 38, `stat_value1` = 532,
    `spellid_1` = 94602, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17837;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17838 Psautier du Néant
UPDATE `raid_trinket` SET
    `entry` = 17838, `name` = 'Psautier du Néant', `description` = 'Ses psaumes ont été écrits à rebours, par une voix creuse.', `ItemLevel` = 477,
    `displayid` = 52633, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 36, `stat_value1` = 238,
    `spellid_1` = 94603, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17838;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17839 Souffle du Néant
UPDATE `raid_trinket` SET
    `entry` = 17839, `name` = 'Souffle du Néant', `description` = 'Ce que Vel''thazar inspire ne revient jamais.', `ItemLevel` = 477,
    `displayid` = 52614, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 45, `stat_value1` = 361,
    `spellid_1` = 94605, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17839;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17840 Chapelet de l'Archevêque
UPDATE `raid_trinket` SET
    `entry` = 17840, `name` = 'Chapelet de l''Archevêque', `description` = 'Aldric en égrenait encore les perles quand le démon a parlé par sa bouche.', `ItemLevel` = 477,
    `displayid` = 45855, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 6, `stat_value1` = 361,
    `spellid_1` = 94606, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17840;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17841 Reliquaire d'Aldric
UPDATE `raid_trinket` SET
    `entry` = 17841, `name` = 'Reliquaire d''Aldric', `description` = 'La dernière prière d''Aldric y est scellée.', `ItemLevel` = 477,
    `displayid` = 59269, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 45, `stat_value1` = 361,
    `spellid_1` = 94608, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17841;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17842 Pierre du Bastion
UPDATE `raid_trinket` SET
    `entry` = 17842, `name` = 'Pierre du Bastion', `description` = 'Arrachée à la tour du Bastion, elle porte encore la marque des coups.', `ItemLevel` = 477,
    `displayid` = 31844, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 7, `stat_value1` = 433,
    `spellid_1` = 94609, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17842;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17843 Cierge de la Dernière lumière
UPDATE `raid_trinket` SET
    `entry` = 17843, `name` = 'Cierge de la Dernière lumière', `description` = 'Tant qu''il brûle, le Néant attend.', `ItemLevel` = 477,
    `displayid` = 6498, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 13, `stat_value1` = 228,
    `spellid_1` = 94611, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17843;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17844 Pierre à aiguiser du bourreau
UPDATE `raid_trinket` SET
    `entry` = 17844, `name` = 'Pierre à aiguiser du bourreau', `description` = 'Le bourreau l''use un peu plus à chaque sentence.', `ItemLevel` = 485,
    `displayid` = 38129, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 36, `stat_value1` = 240,
    `spellid_1` = 94612, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17844;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17845 Cadran du couvre-feu
UPDATE `raid_trinket` SET
    `entry` = 17845, `name` = 'Cadran du couvre-feu', `description` = 'Quand l''aiguille tombe, plus personne ne sort.', `ItemLevel` = 485,
    `displayid` = 6540, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 32, `stat_value1` = 240,
    `spellid_1` = 94614, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17845;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17846 Registre d'écrou
UPDATE `raid_trinket` SET
    `entry` = 17846, `name` = 'Registre d''écrou', `description` = 'Chaque nom y est inscrit. Chaque nom y est rayé.', `ItemLevel` = 485,
    `displayid` = 1317, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 45, `stat_value1` = 367,
    `spellid_1` = 94615, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17846;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17847 Œil du Gardien-chef
UPDATE `raid_trinket` SET
    `entry` = 17847, `name` = 'Œil du Gardien-chef', `description` = 'Il ne se ferme jamais tout à fait.', `ItemLevel` = 485,
    `displayid` = 59524, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 32, `stat_value1` = 240,
    `spellid_1` = 94617, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17847;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17848 Lettre de grâce
UPDATE `raid_trinket` SET
    `entry` = 17848, `name` = 'Lettre de grâce', `description` = 'Signée, mais jamais remise.', `ItemLevel` = 485,
    `displayid` = 31847, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 6, `stat_value1` = 367,
    `spellid_1` = 94618, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17848;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17851 Tampon de libération
UPDATE `raid_trinket` SET
    `entry` = 17851, `name` = 'Tampon de libération', `description` = 'Personne ne l''a jamais vu servir.', `ItemLevel` = 485,
    `displayid` = 35649, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 45, `stat_value1` = 367,
    `spellid_1` = 94620, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17851;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17852 Maillon des fers
UPDATE `raid_trinket` SET
    `entry` = 17852, `name` = 'Maillon des fers', `description` = 'Un maillon des fers qui ont tenu plus longtemps que leurs prisonniers.', `ItemLevel` = 485,
    `displayid` = 32335, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 7, `stat_value1` = 441,
    `spellid_1` = 94621, `spelltrigger_1` = 1, `spellcooldown_1` = -1,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17852;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
-- 17853 Verrou du cachot
UPDATE `raid_trinket` SET
    `entry` = 17853, `name` = 'Verrou du cachot', `description` = 'Une fois tiré, il ne s''ouvre plus que de l''extérieur.', `ItemLevel` = 485,
    `displayid` = 64264, `Quality` = 4, `RequiredLevel` = 80, `Flags` = 524296, `maxcount` = 0,
    `stat_type1` = 13, `stat_value1` = 230,
    `spellid_1` = 94623, `spelltrigger_1` = 0, `spellcooldown_1` = 120000,
    `spellcategory_1` = 0, `spellcategorycooldown_1` = -1;
DELETE FROM `item_template` WHERE `entry` = 17853;
INSERT INTO `item_template` SELECT * FROM `raid_trinket`;
DROP TEMPORARY TABLE `raid_trinket`;
DELETE FROM `item_template_locale` WHERE `ID` IN (17836, 17837, 17838, 17839, 17840, 17841, 17842, 17843, 17844, 17845, 17846, 17847, 17848, 17851, 17852, 17853);
INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(17836, 'frFR', 'Éclat du Marteau béni', 'Un éclat de l''un des marteaux de l''Archevêque, encore tiède.', 0),
(17836, 'enUS', 'Shard of the Blessed Hammer', 'A shard of one of the Archbishop''s hammers, still warm.', 0),
(17837, 'frFR', 'Penne du Séraphin', 'Arrachée aux ailes du Séraphin au plus fort du sermon.', 0),
(17837, 'enUS', 'Seraphim''s Pinion', 'Torn from the Seraphim''s wings at the height of the sermon.', 0),
(17838, 'frFR', 'Psautier du Néant', 'Ses psaumes ont été écrits à rebours, par une voix creuse.', 0),
(17838, 'enUS', 'Psalter of the Void', 'Its psalms were written backwards, by a hollow voice.', 0),
(17839, 'frFR', 'Souffle du Néant', 'Ce que Vel''thazar inspire ne revient jamais.', 0),
(17839, 'enUS', 'Breath of the Void', 'What Vel''thazar breathes in never comes back.', 0),
(17840, 'frFR', 'Chapelet de l''Archevêque', 'Aldric en égrenait encore les perles quand le démon a parlé par sa bouche.', 0),
(17840, 'enUS', 'The Archbishop''s Rosary', 'Aldric was still telling its beads when the demon spoke through his mouth.', 0),
(17841, 'frFR', 'Reliquaire d''Aldric', 'La dernière prière d''Aldric y est scellée.', 0),
(17841, 'enUS', 'Aldric''s Reliquary', 'Aldric''s last prayer is sealed inside.', 0),
(17842, 'frFR', 'Pierre du Bastion', 'Arrachée à la tour du Bastion, elle porte encore la marque des coups.', 0),
(17842, 'enUS', 'Bastion Stone', 'Torn from the Bastion tower, it still bears the marks of the blows.', 0),
(17843, 'frFR', 'Cierge de la Dernière lumière', 'Tant qu''il brûle, le Néant attend.', 0),
(17843, 'enUS', 'Last Light Candle', 'As long as it burns, the Void waits.', 0),
(17844, 'frFR', 'Pierre à aiguiser du bourreau', 'Le bourreau l''use un peu plus à chaque sentence.', 0),
(17844, 'enUS', 'Headsman''s Whetstone', 'The headsman wears it down a little more with every sentence.', 0),
(17845, 'frFR', 'Cadran du couvre-feu', 'Quand l''aiguille tombe, plus personne ne sort.', 0),
(17845, 'enUS', 'Curfew Dial', 'When the hand falls, no one leaves.', 0),
(17846, 'frFR', 'Registre d''écrou', 'Chaque nom y est inscrit. Chaque nom y est rayé.', 0),
(17846, 'enUS', 'Prison Ledger', 'Every name is written in it. Every name is struck out.', 0),
(17847, 'frFR', 'Œil du Gardien-chef', 'Il ne se ferme jamais tout à fait.', 0),
(17847, 'enUS', 'Eye of the Warden-Chief', 'It never quite closes.', 0),
(17848, 'frFR', 'Lettre de grâce', 'Signée, mais jamais remise.', 0),
(17848, 'enUS', 'Letter of Pardon', 'Signed, but never delivered.', 0),
(17851, 'frFR', 'Tampon de libération', 'Personne ne l''a jamais vu servir.', 0),
(17851, 'enUS', 'Release Stamp', 'No one has ever seen it used.', 0),
(17852, 'frFR', 'Maillon des fers', 'Un maillon des fers qui ont tenu plus longtemps que leurs prisonniers.', 0),
(17852, 'enUS', 'Shackle Link', 'A link of the irons that outlasted their prisoners.', 0),
(17853, 'frFR', 'Verrou du cachot', 'Une fois tiré, il ne s''ouvre plus que de l''extérieur.', 0),
(17853, 'enUS', 'Dungeon Bolt', 'Once drawn, it only opens from the outside.', 0);

-- Their passives' procs: what sets them off, how often, and how long they rest
DELETE FROM `spell_proc` WHERE `SpellId` IN (94600, 94603, 94606, 94609, 94612, 94615, 94618, 94621);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(94600, 0, 0, 0, 0, 0, 0x154, 1, 2, 0, 0, 0, 0, 35, 45000, 0),
(94603, 0, 0, 0, 0, 0, 0x10000, 1, 2, 0, 0, 0, 0, 10, 45000, 0),
(94606, 0, 0, 0, 0, 0, 0x4000, 2, 2, 0, 0, 0, 0, 10, 45000, 0),
(94609, 0, 0, 0, 0, 0, 0x28, 1, 2, 0, 0, 0, 0, 15, 30000, 0),
(94612, 0, 0, 0, 0, 0, 0x154, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(94615, 0, 0, 0, 0, 0, 0x10000, 1, 2, 0, 0, 0, 0, 10, 45000, 0),
(94618, 0, 0, 0, 0, 0, 0x4000, 2, 2, 0, 0, 0, 0, 100, 0, 0),
(94621, 0, 0, 0, 0, 0, 0x28, 1, 2, 0, 0, 0, 0, 100, 0, 0);
