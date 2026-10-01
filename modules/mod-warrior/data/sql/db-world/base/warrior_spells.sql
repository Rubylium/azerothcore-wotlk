-- The Warrior's talent trees (modules/mod-warrior): attack power coefficients (ap_bonus, ap_dot_bonus) of its new spells
-- that do not strike with the weapon (the weapon strikes - Colossus Smash, Raging Blow, Rampage... - scale with the
-- weapon and attack power like Mortal Strike). Sized against the WotLK kit they sit beside (Thunder Clap 0.12 of attack
-- power, Shockwave 0.75, Mortal Strike about 1.1 weapon hit) at the bench's ~5000 attack power. The damage this module
-- hands out at amounts it works out itself (Meat Cleaver's relays) gets an explicit zero row, or the core would add a
-- default coefficient on top of an amount already final. README.md has the reasoning.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (95103, 95105, 95106, 95123, 95124, 95130);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(95103, 0, 0, 0.6, 0, 'Warrior - Storm Bolt'),
(95105, 0, 0, 1.1, 0.3, 'Warrior - Thunderous Roar (each enemy, each 2 s bleed tick)'),
(95106, 0, 0, 1.0, 0, 'Warrior - Champion''s Spear (each enemy)'),
(95123, 0, 0, 1.1, 0.3, 'Warrior - Odyn''s Fury (each enemy, each 2 s burn tick)'),
(95124, 0, 0, 0.6, 0, 'Warrior - Ravager (each enemy, each blow)'),
(95130, 0, 0, 0, 0, 'Warrior - Meat Cleaver relay (amount set by mod-warrior)');

-- Ignore Pain's absorb (95144) takes half of each hit until spent (mod-warrior's spell_warr_ignore_pain_absorb)
DELETE FROM `spell_script_names` WHERE `spell_id` = 95144 AND `ScriptName` = 'spell_warr_ignore_pain_absorb';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(95144, 'spell_warr_ignore_pain_absorb');
