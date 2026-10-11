-- Bosses never play their wound animation when hit (CREATURE_TYPE_FLAG_DO_NOT_PLAY_WOUND_ANIM, 0x8): with the crit
-- chances of our paragon and gear nearly every hit is a critical one, and a boss without the flag flinched in a loop.
-- Every stock raid boss has it already; this gives it to every boss (rank 3) and dungeon boss (flags_extra 1, bound to
-- the instance) that lacks it.
UPDATE `creature_template` SET `type_flags` = `type_flags` | 8 WHERE (`rank` = 3 OR (`flags_extra` & 1) <> 0) AND (`type_flags` & 8) = 0;
