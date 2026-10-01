-- The Shaman's talent trees (modules/mod-shaman): spell power (direct_bonus, dot_bonus) and attack power (ap_bonus)
-- coefficients of its new spells. Sized against the WotLK kit they sit beside (Lightning Bolt 0.71, Lava Burst 0.57,
-- Earth Shock 0.39, Healing Wave 1.61, Chain Heal 1.34, Riptide 0.40 + 0.19 a tick) at ~2500 spell power and ~5000
-- attack power (the bench's gear); Enhancement's strikes scale with attack power, its spells with the spell power
-- Mental Quickness gives it. The damage and heals this module hands out at amounts it works out itself (Crash
-- Lightning's relays, Ancestral Guidance, Cloudburst, Ascension's copies) get explicit zero rows, or the core would add
-- a default coefficient on top of an amount already final. README.md has the reasoning.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (95424, 95427, 95428, 95431, 95433, 95435, 95441, 95442, 95443, 95445,
    95455, 95471, 95479, 95480, 95481, 95483, 95484);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(95424, 0, 0.1, 0, 0, 'Shaman - Lightning Lasso (each tick)'),
(95427, 0, 0, 0, 0, 'Shaman - Ancestral Guidance heal (amount set by mod-shaman)'),
(95428, 0.5371, 0, 0, 0, 'Shaman - Earth Shield (Elemental Orbit, the Shaman''s own)'),
(95431, 1.8, 0, 0, 0, 'Shaman - Elemental Blast'),
(95433, 0.9, 0, 0, 0, 'Shaman - Icefury'),
(95435, 0.6, 0, 0, 0, 'Shaman - Earthquake (each enemy, each shake)'),
(95441, 0, 0, 1.5, 0, 'Shaman - Crash Lightning (each enemy)'),
(95442, 0, 0, 0.9, 0, 'Shaman - Ice Strike'),
(95443, 0, 0, 1.4, 0, 'Shaman - Sundering (each enemy)'),
(95445, 0, 0, 0, 0, 'Shaman - Crash Lightning relay (amount set by mod-shaman)'),
(95455, 0, 0, 1.2, 0, 'Shaman - Ascendance winds (each enemy)'),
(95471, 0.6, 0, 0, 0, 'Shaman - Unleash Life'),
(95479, 0, 0, 0, 0, 'Shaman - Ascendance heal (amount set by mod-shaman)'),
(95480, 0.18, 0, 0, 0, 'Shaman - Healing Rain (each ally, each 2 s)'),
(95481, 0, 0, 0, 0, 'Shaman - Cloudburst heal (amount set by mod-shaman)'),
(95483, 0.3, 0, 0, 0, 'Shaman - Healing Tide Totem (each ally, each 2 s)'),
(95484, 0.7, 0, 0, 0, 'Shaman - Wellspring (each ally)');

-- Elemental Orbit's own Earth Shield (95428) heals as the stock one does
DELETE FROM `spell_script_names` WHERE `spell_id` = 95428 AND `ScriptName` = 'spell_sha_earth_shield';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(95428, 'spell_sha_earth_shield');
