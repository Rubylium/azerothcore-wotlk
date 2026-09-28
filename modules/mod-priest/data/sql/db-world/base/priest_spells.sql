-- The Priest's talent trees (modules/mod-priest): spell power coefficients (direct_bonus, dot_bonus) of its new
-- spells. Tuned against the WotLK kit they sit beside (Mind Blast 0.43, Shadow Word: Pain 0.18 a tick, Flash Heal
-- 0.81, Greater Heal 1.61, Prayer of Healing 0.53) at ~3000 spell power (raid gear before this server's paragon, which
-- scales everything alike), the heals against the reworked Holy Paladin's (Word of Glory 3500 + 1.2). The damage and
-- heals this module hands out at amounts it works out itself (Atonement, Echo of Light, the apparitions, Psychic Link,
-- Void Eruption's splash...) get explicit zero rows, or the core would add a default coefficient on top of an amount
-- already final. README.md has the reasoning.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (93502, 93510, 93515, 93516, 93517, 93518, 93519, 93520, 93530, 93531,
    93532, 93534, 93541, 93550, 93551, 93552, 93553, 93556, 93557, 93558, 93560, 93561, 93563, 93564, 93568, 93569,
    93571, 93572);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(93502, 0, 0, 0, 0, 'Priest - Atonement heal (amount set by mod-priest)'),
(93510, 2.5, 0, 0, 0, 'Priest - Power Word: Life'),
(93515, 0.45, 0, 0, 0, 'Priest - Divine Star damage (each enemy, each pass)'),
(93516, 0.45, 0, 0, 0, 'Priest - Divine Star heal (each ally, each pass)'),
(93517, 0.9, 0, 0, 0, 'Priest - Halo damage (each enemy)'),
(93518, 0.9, 0, 0, 0, 'Priest - Halo heal (each ally)'),
(93519, 1.6, 0, 0, 0, 'Priest - Mindgames'),
(93520, 0, 0, 0, 0, 'Priest - Mindgames heal (amount set by mod-priest)'),
(93530, 0.6, 0, 0, 0, 'Priest - Power Word: Radiance (its target)'),
(93531, 0.6, 0, 0, 0, 'Priest - Power Word: Radiance (each other ally)'),
(93532, 1.0, 0, 0, 0, 'Priest - Schism'),
(93534, 0.25, 0.13, 0, 0, 'Priest - Purge the Wicked (hit, each 2 s tick)'),
(93541, 0, 0, 0, 0, 'Priest - Surging Light heal (amount set by mod-priest)'),
(93550, 2.2, 0, 0, 0, 'Priest - Holy Word: Serenity'),
(93551, 0.9, 0, 0, 0, 'Priest - Holy Word: Sanctify (its target)'),
(93552, 1.2, 0, 0, 0, 'Priest - Holy Word: Chastise'),
(93553, 0, 0, 0, 0, 'Priest - Echo of Light (amount set by mod-priest)'),
(93556, 0.9, 0, 0, 0, 'Priest - Holy Word: Sanctify (each other ally)'),
(93557, 0.35, 0, 0, 0, 'Priest - Cosmic Ripple (each ally)'),
(93558, 0, 0, 0, 0, 'Priest - Trail of Light (amount set by mod-priest)'),
(93560, 0.9, 0, 0, 0, 'Priest - Devouring Plague instant hit'),
(93561, 1.3, 0, 0, 0, 'Priest - Void Bolt'),
(93563, 1.4, 0, 0, 0, 'Priest - Void Eruption (its target)'),
(93564, 0, 0, 0, 0, 'Priest - Void Eruption splash (amount set by mod-priest)'),
(93568, 0, 0, 0, 0, 'Priest - Shadowy Apparition (amount set by mod-priest)'),
(93569, 0.6, 0, 0, 0, 'Priest - Shadow Crash (each enemy)'),
(93571, 0.45, 0, 0, 0, 'Priest - Void Torrent (each of 3 ticks)'),
(93572, 0, 0, 0, 0, 'Priest - Psychic Link (amount set by mod-priest)');
