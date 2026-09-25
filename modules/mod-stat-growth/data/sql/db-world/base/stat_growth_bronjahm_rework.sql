-- Bronjahm (the Forge of Souls) fights with abilities drawn in red before they land (BronjahmRework.cpp) instead of
-- the stock script's soul fragments and Soulstorm, which bots could not play.
UPDATE `creature_template` SET `ScriptName` = 'boss_bronjahm_evolutions' WHERE `entry` = 36497;
