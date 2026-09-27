-- The Paladin's talent trees (modules/mod-paladin): spell power (direct_bonus, dot_bonus) and attack power (ap_bonus,
-- ap_dot_bonus) coefficients of its new spells. Tuned against the WotLK kit they sit beside (Holy Shock 0.81, Judgement
-- of Righteousness 0.32 SP + 0.2 AP, Exorcism 0.42 SP + 0.42 AP, Hammer of Wrath 0.15 SP + 0.15 AP) at ~5000 attack
-- power (Retribution, whose Sheath of Light turns 30% of it into spell power) and ~3000 spell power (Holy). The heals
-- this module hands out at amounts it works out itself (the beacons, Avenging Crusader, Word of Glory's splash) get
-- explicit zero rows, or the core would add a default coefficient on top of an amount already final.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (92901, 92905, 92906, 92911, 92915, 92916, 92919, 92921, 92922, 92930,
    92933, 92941, 92943, 92945, 92948, 92953);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(92901, 1.2, 0, 0, 0, 'Paladin - Word of Glory'),
(92905, 0.3, 0, 0.3, 0, 'Paladin - Divine Toll (each enemy)'),
(92906, 0.6, 0, 0, 0, 'Paladin - Divine Toll (Holy, each ally)'),
(92911, 0.55, 0, 0, 0, 'Paladin - Light of Dawn (each ally)'),
(92915, 0.25, 0, 0, 0, 'Paladin - Glimmer of Light (each ally, each Holy Shock)'),
(92916, 0.2, 0, 0.1, 0, 'Paladin - Glimmer of Light (each enemy, each Holy Shock)'),
(92919, 0, 0, 0, 0, 'Paladin - Beacon of Faith / Beacon of Virtue relay (amount set by mod-paladin)'),
(92921, 0, 0, 0, 0, 'Paladin - Avenging Crusader heal (amount set by mod-paladin)'),
(92922, 0, 0, 0, 0, 'Paladin - Light Bearer splash (amount set by mod-paladin)'),
(92930, 0.3, 0, 0.5, 0, 'Paladin - Shield of the Righteous'),
(92933, 0.4, 0, 0.2, 0, 'Paladin - Eye of Tyr'),
(92941, 0.3, 0, 0.7, 0, 'Paladin - Blade of Justice'),
(92943, 0.3, 0, 0.6, 0, 'Paladin - Wake of Ashes (each enemy)'),
(92945, 0.4, 0, 0.9, 0, 'Paladin - Final Reckoning (each enemy)'),
(92948, 0.5, 0, 1.2, 0, 'Paladin - Execution Sentence'),
(92953, 0, 0.03, 0, 0.05, 'Paladin - Expurgation (each tick)');

-- Divine Storm as a Holy Power finisher (92942) is the WotLK talent's spell cloned: it keeps the core script that heals
-- the group for a quarter of the damage and reaches 6 enemies in a dungeon
DELETE FROM `spell_script_names` WHERE `spell_id` = 92942 AND `ScriptName` = 'spell_pal_divine_storm';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(92942, 'spell_pal_divine_storm');
