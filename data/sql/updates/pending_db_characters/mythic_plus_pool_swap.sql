-- Gundrak (217) and The Nexus (226) left the Mythic+ pool, replaced by The Shattered Halls (189) and
-- The Forge of Souls (252).
--
-- A character's Mythic+ score is the sum of its best run per dungeon (mod-playerbots, GetScore), so records for
-- dungeons nobody can run any more would keep paying score for ever. They go with the dungeons.
DELETE FROM `mythic_plus_best` WHERE `dungeon` IN (217, 226);
