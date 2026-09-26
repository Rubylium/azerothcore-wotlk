-- The Mage's talent trees (modules/mod-mage): Arcane Explosion builds Arcane Blast charges (36032) instead of spending
-- them - mod-mage adds one when it hits an enemy - so only Arcane Barrage (family flags word 1 0x8000) spends them now.
-- Word 0's 0x1000, Arcane Explosion, is dropped from the charge's proc.
UPDATE `spell_proc` SET `SpellFamilyMask0` = 0 WHERE `SpellId` = 36032;
