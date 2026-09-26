-- The Mechanar's bosses fight on the ground indicators (mythic/Mechanar.cpp): module copies of their stock scripts,
-- with what was hidden drawn before it lands. A ScriptName takes precedence over the AIName, so the Nether Charge's
-- SmartAI rows stay in place, unused.
UPDATE `creature_template` SET `ScriptName` = 'boss_gatewatcher_gyrokill_evolutions' WHERE `entry` = 19218;
UPDATE `creature_template` SET `ScriptName` = 'boss_gatewatcher_iron_hand_evolutions' WHERE `entry` = 19710;
UPDATE `creature_template` SET `ScriptName` = 'boss_mechano_lord_capacitus_evolutions' WHERE `entry` = 19219;
UPDATE `creature_template` SET `ScriptName` = 'npc_nether_charge_evolutions' WHERE `entry` = 20405;
UPDATE `creature_template` SET `ScriptName` = 'boss_nethermancer_sepethrea_evolutions' WHERE `entry` = 19221;
UPDATE `creature_template` SET `ScriptName` = 'npc_raging_flames_evolutions' WHERE `entry` = 20481;
UPDATE `creature_template` SET `ScriptName` = 'boss_pathaleon_the_calculator_evolutions' WHERE `entry` = 19220;
