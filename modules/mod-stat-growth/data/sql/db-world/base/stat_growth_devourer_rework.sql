-- The Devourer of Souls (the Forge of Souls) fights with abilities drawn in red before they land
-- (DevourerRework.cpp) instead of the stock script's Mirrored Soul, Well of Souls, Unleashed Souls and Wailing Souls,
-- which bots could not play.
UPDATE `creature_template` SET `ScriptName` = 'boss_devourer_of_souls_evolutions' WHERE `entry` = 36502;
