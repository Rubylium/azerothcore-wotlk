-- Halls of Lightning: the bosses fight on copies of their stock scripts with their mechanics drawn in red before they
-- land (mythic/HallsOfLightning.cpp): Bjarngrim's Mortal Strike, Cleave and Whirlwind, Volkhan's shattering golems,
-- Ionar's Ball Lightning, Static Overload and sparks, Loken's Pulsing Shockwave. Heroic templates use these scripts.
UPDATE `creature_template` SET `ScriptName` = 'boss_bjarngrim_evolutions' WHERE `entry` = 28586;
UPDATE `creature_template` SET `ScriptName` = 'boss_volkhan_evolutions' WHERE `entry` = 28587;
UPDATE `creature_template` SET `ScriptName` = 'npc_molten_golem_evolutions' WHERE `entry` = 28695;
UPDATE `creature_template` SET `ScriptName` = 'boss_ionar_evolutions' WHERE `entry` = 28546;
UPDATE `creature_template` SET `ScriptName` = 'boss_loken_evolutions' WHERE `entry` = 28923;
