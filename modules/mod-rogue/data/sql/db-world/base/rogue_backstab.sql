-- The Rogue's talent trees (modules/mod-rogue): Backstab no longer has to come from behind the target. mod-rogue
-- adds 20% damage when it does. SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET is 0x20000.
UPDATE `spell_custom_attr` SET `attributes` = `attributes` & ~0x20000
WHERE `spell_id` IN (53, 2589, 2590, 2591, 8721, 11279, 11280, 11281, 25300, 26863, 48656, 48657);
