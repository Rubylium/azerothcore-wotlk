-- The Death Knight's talent trees (modules/mod-death-knight): attack power coefficients of its new damage spells. Without
-- a row the core gives a Death Knight spell no attack power scaling at all. Tuned against the WotLK kit they sit beside:
-- Death Coil 0.15, Howling Blast 0.2 on every target, Death and Decay 0.048 a second.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (92602, 92612, 92615, 92643, 92645, 92646, 92647, 92662, 92666);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(92602, 0, 0, 1, 0, 'Death Knight - Soul Reaper (the scythe, under 35% health)'),
(92612, 0, 0, 0.15, 0, 'Death Knight - Death''s Caress'),
(92615, 0, 0, 0.1, 0, 'Death Knight - Bonestorm (each second, every enemy)'),
(92643, 0, 0, 0.06, 0, 'Death Knight - Remorseless Winter (each second, every enemy)'),
(92645, 0, 0, 0.25, 0, 'Death Knight - Breath of Sindragosa (each second, every enemy)'),
(92646, 0, 0, 1.5, 0, 'Death Knight - Frostwyrm''s Fury'),
(92647, 0, 0, 0.2, 0, 'Death Knight - Glacial Advance'),
(92662, 0, 0, 0.22, 0, 'Death Knight - Festering Wound (a burst)'),
(92666, 0, 0, 0.12, 0, 'Death Knight - Epidemic (each diseased enemy)');
