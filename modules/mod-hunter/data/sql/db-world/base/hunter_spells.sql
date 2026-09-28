-- The Hunter's talent trees (modules/mod-hunter): attack power coefficients (ap_bonus, ap_dot_bonus) of its new spells.
-- Every Hunter family spell reads the ranged attack power (SpellInfo::IsRangedWeaponSpell) unless its damage class is
-- melee: the Survival bleeds read the melee one. Tuned against the WotLK kit they sit beside (Arcane Shot 0.15, Serpent
-- Sting 0.04 a tick, Aimed Shot and Steady Shot on the weapon) at ~6000 ranged attack power (raid gear with Aspect of
-- the Dragonhawk, before this server's paragon, which scales everything alike). The hits this module works out itself
-- (the pet's Kill Command, Beast Cleave, Stomp, the ricochets, the wild beasts...) get explicit zero rows, or the core
-- would add a default coefficient on top of an amount already final. README.md has the reasoning.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (93210, 93212, 93220, 93223, 93224, 93228, 93232, 93233, 93236, 93237,
    93251, 93254, 93257, 93262, 93264, 93265, 93280, 93281, 93282, 93290, 93292, 93298, 93300, 93301);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(93210, 0, 0, 0.3, 0, 'Hunter - Death Chakram (each of 7 hits)'),
(93212, 0, 0, 0.35, 0, 'Hunter - Stampede (each beast)'),
(93220, 0, 0, 0, 0.12, 'Hunter - Barbed Shot (each 2 s tick, 4 ticks)'),
(93223, 0, 0, 0.45, 0, 'Hunter - Cobra Shot'),
(93224, 0, 0, 0, 0, 'Hunter - Kill Command, the pet''s hit (amount set by mod-hunter)'),
(93228, 0, 0, 0, 0, 'Hunter - Beast Cleave (amount set by mod-hunter)'),
(93232, 0, 0, 0, 0, 'Hunter - Bloodshed bleed (amount set by mod-hunter)'),
(93233, 0, 0, 0, 0, 'Hunter - Stomp (amount set by mod-hunter)'),
(93236, 0, 0, 0, 0, 'Hunter - Brutal Companion (amount set by mod-hunter)'),
(93237, 0, 0, 0, 0, 'Hunter - Wild beast hit (unused, the beasts swing)'),
(93251, 0, 0, 0.12, 0, 'Hunter - Rapid Fire (each of 6 shots)'),
(93254, 0, 0, 0, 0, 'Hunter - Trick Shots ricochet (amount set by mod-hunter)'),
(93257, 0, 0, 0, 0.09, 'Hunter - Volley (each second, 6 s)'),
(93262, 0, 0, 1.0, 0, 'Hunter - Wailing Arrow'),
(93264, 0, 0, 0, 0, 'Hunter - Wailing Arrow splash (amount set by mod-hunter)'),
(93265, 0, 0, 0, 0, 'Hunter - Legacy of the Windrunners arrows (amount set by mod-hunter)'),
(93280, 0, 0, 0.45, 0, 'Hunter - Wildfire Bomb (the enemy hit)'),
(93281, 0, 0, 0, 0, 'Hunter - Wildfire Bomb burst (amount set by mod-hunter)'),
(93282, 0, 0, 0, 0.04, 'Hunter - Wildfire Bomb burn (each second, 6 s)'),
(93290, 0, 0, 0, 0, 'Hunter - Flanking Strike, the pet''s hit (amount set by mod-hunter)'),
(93292, 0, 0, 0, 0, 'Hunter - Spearhead bleed (amount set by mod-hunter)'),
(93298, 0, 0, 0, 0.05, 'Hunter - Bloodseeker bleed (each 2 s tick)'),
(93300, 0, 0, 0, 0, 'Hunter - Terms of Engagement (amount set by mod-hunter)'),
(93301, 0, 0, 0, 0.04, 'Hunter - Serpent Sting from melee (Viper Bite, Viper''s Venom), as Serpent Sting');

-- The wild beast of Bête sauvage and Appel de la nature sauvage: the Spirit Wolf of Feral Spirit as a template, a worg
-- to look at. mod-hunter calls it as a guardian that fights its Hunter's target for 8 s, and sets its swings from the
-- Hunter's ranged attack power.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 93240;
DELETE FROM `creature_template` WHERE `entry` = 93240;
DROP TEMPORARY TABLE IF EXISTS `hunter_wild_beast`;
CREATE TEMPORARY TABLE `hunter_wild_beast` ENGINE=InnoDB AS SELECT * FROM `creature_template` WHERE `entry` = 29264;
UPDATE `hunter_wild_beast` SET `entry` = 93240, `name` = 'Bête sauvage', `subname` = NULL, `minlevel` = 80,
    `maxlevel` = 80, `family` = 1, `type` = 1, `speed_run` = 1.71429, `BaseAttackTime` = 2000, `AIName` = '',
    `ScriptName` = '', `VerifiedBuild` = 0;
INSERT INTO `creature_template` SELECT * FROM `hunter_wild_beast`;
DROP TEMPORARY TABLE `hunter_wild_beast`;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(93240, 0, 11421, 1, 1, 0);
