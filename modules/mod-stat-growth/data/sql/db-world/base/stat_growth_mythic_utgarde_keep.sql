-- Utgarde Keep in Mythic+ (src/mythic/UtgardeKeep.cpp): Prince Keleseth marks his Frost Tomb and Shadow Nova in red
-- before they land; Skarvald the Constructor (and his ghost) marks a player and charges the spot instead of wiping
-- his threat onto a random player.
UPDATE `creature_template` SET `ScriptName` = 'boss_keleseth_evolutions' WHERE `entry` = 23953;
UPDATE `creature_template` SET `ScriptName` = 'boss_skarvald_evolutions' WHERE `entry` IN (24200, 27390);
