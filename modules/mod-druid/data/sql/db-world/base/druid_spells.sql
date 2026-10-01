-- The Druid's talent trees (modules/mod-druid): spell power (direct_bonus, dot_bonus) and attack power (ap_bonus,
-- ap_dot_bonus) coefficients of its new spells. Sized against the WotLK kit they sit beside (Wrath 0.571, Starfire 1.0,
-- Moonfire 0.15 + 0.13 a tick, Rejuvenation 0.376 a tick, Swipe (Bear) 0.063 attack power) at ~2500 spell power and
-- ~9000 attack power in the forms (the bench's gear). The damage this module hands out at amounts it works out itself
-- (Starfire's splash, Primal Wrath's strikes and bleeds, Feral Frenzy's bleed) gets explicit zero rows, or the core
-- would add a default coefficient on top of an amount already final. README.md has the reasoning.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (96040, 96041, 96042, 96047, 96048, 96050, 96051, 96052, 96060, 96061,
    96071, 96072, 96073, 96084, 96086, 96087);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(96040, 1.5, 0, 0, 0, 'Druid - Starsurge'),
(96041, 0.15, 0.13, 0, 0, 'Druid - Sunfire'),
(96042, 0.2, 0.11, 0, 0, 'Druid - Stellar Flare'),
(96047, 0, 0, 0, 0, 'Druid - Starfire splash (amount set by mod-druid)'),
(96048, 0.4, 0, 0, 0, 'Druid - Shooting Star'),
(96050, 0.15, 0, 0, 0, 'Druid - Starfall (each enemy, each second)'),
(96051, 0.3, 0, 0, 0, 'Druid - Fury of Elune (each enemy, each second)'),
(96052, 2, 0, 0, 0, 'Druid - Orbit Breaker Full Moon'),
(96060, 0, 0, 0.15, 0.04, 'Druid - Thrash (cat)'),
(96061, 0, 0, 0.1, 0.03, 'Druid - Thrash (bear)'),
(96071, 0, 0, 0, 0, 'Druid - Primal Wrath strike (amount set by mod-druid)'),
(96072, 0, 0, 0, 0, 'Druid - Primal Wrath bleed (amount set by mod-druid)'),
(96073, 0, 0, 0, 0, 'Druid - Feral Frenzy bleed (amount set by mod-druid)'),
(96084, 0, 0.376, 0, 0, 'Druid - Germination'),
(96086, 0.18, 0, 0, 0, 'Druid - Efflorescence (each ally, each 2 s)'),
(96087, 0, 0.3, 0, 0, 'Druid - Cenarion Ward (each tick)');

-- Shred works from any side, as retail: its behind-the-target requirement (SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET)
-- taken off every rank
UPDATE `spell_custom_attr` SET `attributes` = `attributes` & ~0x20000
WHERE `spell_id` IN (5221, 6800, 8992, 9829, 9830, 27001, 27002, 48571, 48572);
