-- The Mage's talent trees (modules/mod-mage): Fire's spell power coefficients. Meteor's impact (92124) and Phoenix
-- Flames (92121) had none of their own, so the core gave them an instant spell's 0.43 and the two cooldowns hit like a
-- Fire Blast. Living Bomb keeps WotLK's coefficients while Incantation vive cut Fireball's damage per hit, so on a
-- pack it outweighed everything else: its explosion goes from 0.4 to 0.32 (its damage over time is unchanged).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (92121, 92124, 44461, 55361, 55362);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(92121, 1, 0, 0, 0, 'Mage - Phoenix Flames'),
(92124, 2.2, 0, 0, 0, 'Mage - Meteor (impact)'),
(44461, 0.32, 0, 0, 0, 'Mage - Living Bomb DD'),
(55361, 0.32, 0, 0, 0, 'Mage - Living Bomb DD'),
(55362, 0.32, 0, 0, 0, 'Mage - Living Bomb DD');
