-- Gardien-chef Vorhan, his prisoners and the abyssal (stat_growth_warden_vorhan.sql): no flinch when hit
-- (CREATURE_TYPE_FLAG_DO_NOT_PLAY_WOUND_ANIM, 0x8). Under a raid's damage the warden twitched without end and his
-- abilities' animations were lost in it. The client reads it with the creature's query: a client that cached him
-- before needs its cache cleared.
UPDATE `creature_template` SET `type_flags` = `type_flags` | 8 WHERE `entry` BETWEEN 930200 AND 930202;
