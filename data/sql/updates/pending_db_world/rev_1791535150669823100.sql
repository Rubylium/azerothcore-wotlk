-- Ambush (every rank) no longer needs the rogue behind its target, as on retail: the Outlaw's Audace makes it usable
-- out of stealth in the middle of a fight, where a rogue is rarely behind (1 Ambush a minute on a boss for six bots)
UPDATE `spell_custom_attr` SET `attributes` = `attributes` & ~0x20000 WHERE `spell_id` IN (8676, 8724, 8725, 11267, 11268, 11269, 27441, 48689, 48690, 48691);
DELETE FROM `spell_custom_attr` WHERE `spell_id` IN (8676, 8724, 8725, 11267, 11268, 11269, 27441, 48689, 48690, 48691) AND `attributes` = 0;
