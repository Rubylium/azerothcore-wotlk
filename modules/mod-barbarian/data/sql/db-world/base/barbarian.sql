-- The Barbarian (modules/mod-barbarian): its aura scripts, and the coefficients of its spells that do not strike with
-- the weapon. Carnage's tick is set by the script from attack power, so it carries an explicit zero row (the core would
-- add a default coefficient on top).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_barbarian_carnage', 'spell_barbarian_storm_of_steel');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(97114, 'spell_barbarian_carnage'),
(97127, 'spell_barbarian_storm_of_steel');

DELETE FROM `spell_bonus_data` WHERE `entry` IN (97114, 97116);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(97114, 0, 0, 0, 0, 'Barbarian - Carnage (tick set by mod-barbarian from attack power)'),
(97116, 0, 0, 0, 0, 'Barbarian - Born in Blood heal (amount set by mod-barbarian)');
