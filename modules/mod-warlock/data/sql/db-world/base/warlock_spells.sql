-- The Warlock's talent trees (modules/mod-warlock): spell power coefficients (direct_bonus, dot_bonus) of its new
-- spells. Sized against the WotLK kit they sit beside (Shadow Bolt 0.857, Incinerate 0.714, Chaos Bolt 0.714,
-- Corruption 0.2 a tick, Curse of Agony 0.1 a tick) at ~2300 spell power (the bench's gear). The damage this module
-- hands out at amounts it works out itself (the summoned demons' hits, Havoc's and Internal Combustion's relays) and
-- the shields it sizes get explicit zero rows, or the core would add a default coefficient on top of an amount already
-- final. README.md has the reasoning.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (95704, 95714, 95724, 95729, 95731, 95735, 95736, 95737, 95738, 95739,
    95741, 95750, 95751, 95752, 95753, 95754, 95755, 95756, 95757, 95758, 95760, 95766, 95767, 95768, 95769, 95770,
    95771, 95772);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(95704, 0, 0, 0, 0, 'Warlock - Soul Leech shield (amount set by mod-warlock)'),
(95714, 0, 0, 0, 0, 'Warlock - Dark Pact shield (amount set by mod-warlock)'),
(95724, 0.3, 0, 0, 0, 'Warlock - Grimoire of Sacrifice hit'),
(95729, 0.25, 0, 0, 0, 'Warlock - Harvest hit'),
(95731, 0, 0.2, 0, 0, 'Warlock - Soul Rot (each tick)'),
(95735, 0.24, 0, 0, 0, 'Warlock - Malefic Rapture (each damage over time effect on the target)'),
(95736, 0.16, 0, 0, 0, 'Warlock - Phantom Singularity (each enemy, each 2 s)'),
(95737, 0, 0.25, 0, 0, 'Warlock - Vile Taint (each tick)'),
(95738, 0, 0, 0, 0, 'Warlock - Darkglare Eye Beam (amount set by mod-warlock)'),
(95739, 0.25, 0, 0, 0, 'Warlock - Doom Blossom (each enemy)'),
(95741, 0.6, 0, 0, 0, 'Warlock - Demonbolt'),
(95750, 0.45, 0, 0, 0, 'Warlock - Hand of Gul''dan (each enemy, each shard)'),
(95751, 0, 0, 0, 0, 'Warlock - Wild Imp Fel Firebolt (amount set by mod-warlock)'),
(95752, 0, 0, 0, 0, 'Warlock - Dreadstalker Dreadbite (amount set by mod-warlock)'),
(95753, 0.7, 0, 0, 0, 'Warlock - Implosion (each imp, each enemy)'),
(95754, 0.13, 0, 0, 0, 'Warlock - Bilescourge Bombers (each enemy, each second)'),
(95755, 0, 0, 0, 0, 'Warlock - Felstorm (amount set by mod-warlock)'),
(95756, 0, 0, 0, 0, 'Warlock - Demonic Tyrant Demonfire (amount set by mod-warlock)'),
(95757, 0, 0, 0, 0, 'Warlock - Grimoire Felguard Legion Strike (amount set by mod-warlock)'),
(95758, 1.2, 0, 0, 0, 'Warlock - Doom (each enemy)'),
(95760, 0.6, 0, 0, 0, 'Warlock - Conflagration'),
(95766, 0.65, 0, 0, 0, 'Warlock - Rain of Fire (each enemy, each second)'),
(95767, 0.1, 0, 0, 0, 'Warlock - Channel Demonfire bolt'),
(95768, 0, 0, 0, 0, 'Warlock - Havoc relay (amount set by mod-warlock)'),
(95769, 1.6, 0, 0, 0, 'Warlock - Cataclysm (each enemy)'),
(95770, 0, 0, 0, 0, 'Warlock - Infernal Immolation (amount set by mod-warlock)'),
(95771, 0.5, 0, 0, 0, 'Warlock - Infernal impact (each enemy)'),
(95772, 0, 0, 0, 0, 'Warlock - Internal Combustion (amount set by mod-warlock)');

-- The demons mod-warlock summons: allied guardians of the Warlock's for a few seconds, copied from the stock demons
-- (their stats as a template) and dressed as the retail ones: the Wild Imp as a fel imp, the Dreadstalker as a
-- felhound, the Demonic Tyrant as a doomguard, the Darkglare as a beholder, the Grimoire's Felguard and the infernal as
-- themselves. The casters (imps, tyrant, eye) do nothing on their own: mod-warlock has them follow the Warlock and cast;
-- the others fight the target mod-warlock sends them at, their swings set from the Warlock's spell power.
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 95600 AND 95605;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 95600 AND 95605;
DROP TEMPORARY TABLE IF EXISTS `warlock_demon`;
CREATE TEMPORARY TABLE `warlock_demon` ENGINE=InnoDB AS SELECT * FROM `creature_template` WHERE `entry` = 416;
UPDATE `warlock_demon` SET `entry` = 95600, `name` = 'Diablotin sauvage', `subname` = NULL, `minlevel` = 80,
    `maxlevel` = 80, `family` = 0, `type` = 3, `MovementType` = 0, `AIName` = 'NullCreatureAI', `ScriptName` = '',
    `VerifiedBuild` = 0;
INSERT INTO `creature_template` SELECT * FROM `warlock_demon`;
UPDATE `warlock_demon` SET `entry` = 95603, `name` = 'Oeil noir', `speed_run` = 1.14286;
INSERT INTO `creature_template` SELECT * FROM `warlock_demon`;
DROP TEMPORARY TABLE `warlock_demon`;
CREATE TEMPORARY TABLE `warlock_demon` ENGINE=InnoDB AS SELECT * FROM `creature_template` WHERE `entry` = 417;
UPDATE `warlock_demon` SET `entry` = 95601, `name` = 'Traqueffroi', `subname` = NULL, `minlevel` = 80, `maxlevel` = 80,
    `family` = 0, `type` = 3, `speed_run` = 1.71429, `BaseAttackTime` = 2000, `MovementType` = 0, `AIName` = '',
    `ScriptName` = '', `VerifiedBuild` = 0;
INSERT INTO `creature_template` SELECT * FROM `warlock_demon`;
DROP TEMPORARY TABLE `warlock_demon`;
CREATE TEMPORARY TABLE `warlock_demon` ENGINE=InnoDB AS SELECT * FROM `creature_template` WHERE `entry` = 11859;
UPDATE `warlock_demon` SET `entry` = 95602, `name` = 'Tyran démoniaque', `subname` = NULL, `minlevel` = 80,
    `maxlevel` = 80, `family` = 0, `type` = 3, `MovementType` = 0, `AIName` = 'NullCreatureAI', `ScriptName` = '',
    `VerifiedBuild` = 0;
INSERT INTO `creature_template` SELECT * FROM `warlock_demon`;
DROP TEMPORARY TABLE `warlock_demon`;
CREATE TEMPORARY TABLE `warlock_demon` ENGINE=InnoDB AS SELECT * FROM `creature_template` WHERE `entry` = 17252;
UPDATE `warlock_demon` SET `entry` = 95604, `name` = 'Gangregarde', `subname` = NULL, `minlevel` = 80, `maxlevel` = 80,
    `family` = 0, `type` = 3, `BaseAttackTime` = 2000, `MovementType` = 0, `AIName` = '', `ScriptName` = '',
    `VerifiedBuild` = 0;
INSERT INTO `creature_template` SELECT * FROM `warlock_demon`;
DROP TEMPORARY TABLE `warlock_demon`;
CREATE TEMPORARY TABLE `warlock_demon` ENGINE=InnoDB AS SELECT * FROM `creature_template` WHERE `entry` = 89;
UPDATE `warlock_demon` SET `entry` = 95605, `name` = 'Infernal', `subname` = NULL, `minlevel` = 80, `maxlevel` = 80,
    `family` = 0, `type` = 3, `BaseAttackTime` = 2000, `MovementType` = 0, `AIName` = '', `ScriptName` = '',
    `VerifiedBuild` = 0;
INSERT INTO `creature_template` SELECT * FROM `warlock_demon`;
DROP TEMPORARY TABLE `warlock_demon`;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(95600, 0, 16890, 1, 1, 0),
(95601, 0, 1913, 1, 1, 0),
(95602, 0, 18169, 1, 1, 0),
(95603, 0, 20526, 0.6, 1, 0),
(95604, 0, 14255, 1, 1, 0),
(95605, 0, 169, 1, 1, 0);
