-- The Mage's talent trees (modules/mod-mage): Distorsion temporelle is the Mage's Bloodlust, and shares its script -
-- whoever it reaches is Sated, and nobody Sated or Exhausted is reached.
DELETE FROM `spell_script_names` WHERE `spell_id` = 92100;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (92100, 'spell_sha_bloodlust');
