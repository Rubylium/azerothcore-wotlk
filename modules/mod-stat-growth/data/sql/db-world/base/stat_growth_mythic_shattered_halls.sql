-- The Shattered Halls' bosses fight on the ground indicators (mythic/ShatteredHalls.cpp): module copies of their
-- stock scripts, with what was hidden drawn before it lands. Porung's gauntlet is run by its scout.
UPDATE `creature_template` SET `ScriptName` = 'boss_grand_warlock_nethekurse_evolutions' WHERE `entry` = 16807;
UPDATE `creature_template` SET `ScriptName` = 'npc_shattered_hand_scout_evolutions' WHERE `entry` = 17693;
UPDATE `creature_template` SET `ScriptName` = 'boss_warbringer_omrogg_evolutions' WHERE `entry` = 16809;
UPDATE `creature_template` SET `ScriptName` = 'boss_warchief_kargath_bladefist_evolutions' WHERE `entry` = 16808;
