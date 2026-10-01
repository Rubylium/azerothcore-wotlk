-- The Barbarian (modules/mod-barbarian): its aura scripts, and the coefficients of its spells that do not strike with
-- the weapon. Carnage's tick is set by the script from attack power, so it carries an explicit zero row (the core would
-- add a default coefficient on top).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_barbarian_carnage', 'spell_barbarian_storm_of_steel');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(97114, 'spell_barbarian_carnage'),
(97127, 'spell_barbarian_storm_of_steel');

-- The Ascendance's frost and the Chasseur de têtes' Étripeur scale with attack power (the weapon strikes and throws
-- with the weapon, like Mortal Strike).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (97114, 97116, 97166, 97202, 97203, 97209, 97210, 97213);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(97114, 0, 0, 0, 0, 'Barbarian - Carnage (tick set by mod-barbarian from attack power)'),
(97116, 0, 0, 0, 0, 'Barbarian - Born in Blood heal (amount set by mod-barbarian)'),
(97166, 0, 0, 0, 0.12, 'Barbarian - Gutspiller (each 3 s tick)'),
(97202, 0, 0, 0.45, 0, 'Barbarian - Keg Smash frost (each enemy, before the Tankard)'),
(97203, 0, 0, 0.5, 0, 'Barbarian - Breath of the North (each enemy)'),
(97209, 0, 0, 0.8, 0, 'Barbarian - Hodir''s Wrath (each enemy)'),
(97210, 0, 0, 0.3, 0, 'Barbarian - Frozen Tankard'),
(97213, 0, 0, 0.25, 0, 'Barbarian - Ancestral Combat frost blow');
