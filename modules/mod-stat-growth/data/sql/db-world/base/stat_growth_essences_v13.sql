-- Essences are consumed automatically when awarded. Keeping them Bind on Pickup only
-- causes the redundant confirmation dialog when selecting Need in group loot.
UPDATE `item_template`
SET `bonding` = 0
WHERE `entry` IN (
    1267, 1533, 1612,
    1704, 1950, 2050,
    2461, 3338, 3441,
    3507, 42590, 39163,
    21238, 41606, 23656
);
