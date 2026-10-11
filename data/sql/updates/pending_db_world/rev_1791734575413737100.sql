-- The raid trinkets (modules/mod-stat-growth/src/RaidTrinkets.cpp) were copied from Sharpened Twilight Scale (54590)
-- with its item limit category (82: one equipped): all eighteen counted as one, so a second could never be worn
-- ("You have reached the limit of this gem socketed in equipped items"). Each stays unique-equipped (Flags).
UPDATE `item_template` SET `ItemLimitCategory` = 0 WHERE `entry` IN
    (17836, 17837, 17838, 17839, 17840, 17841, 17842, 17843, 17844, 17845, 17846, 17847, 17848, 17851, 17852, 17853,
     17856, 17857);
