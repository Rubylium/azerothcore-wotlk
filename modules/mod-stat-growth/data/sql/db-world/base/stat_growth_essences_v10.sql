UPDATE `item_template`
SET `name` = 'Ascendant Essence of Growth',
    `Quality` = 4,
    `description` = 'A living shard of boundless potential. Consume it to permanently gain +1 class-compatible stat. Its power has no limit.'
WHERE `entry` = 42590;

UPDATE `item_template`
SET `name` = 'Ascendant Essence of Wisdom',
    `Quality` = 4,
    `description` = 'Knowledge gathered across countless lifetimes. Consume it to permanently increase experience gained by 10%. Its power has no limit.'
WHERE `entry` = 39163;

UPDATE `item_template`
SET `name` = 'Ascendant Essence of Flow',
    `Quality` = 4,
    `description` = 'Primal energy pulses within the crystal. Consume it to permanently increase primary-resource regeneration by 1%. Its power has no limit.'
WHERE `entry` = 21238;

UPDATE `item_template`
SET `name` = 'Ascendant Essence of Vitality',
    `Quality` = 4,
    `description` = 'The heartbeat of an undying force echoes within. Consume it to permanently increase maximum health by 1%. Its power has no limit.'
WHERE `entry` = 41606;

UPDATE `item_template`
SET `name` = 'Ascendant Essence of Fortune',
    `Quality` = 4,
    `description` = 'Fate bends around this gilded fragment. Consume it to permanently increase gold and normal-loot gains by 1%. Its power has no limit.'
WHERE `entry` = 23656;

DELETE FROM `item_template_locale` WHERE `ID` IN (21238, 23656, 39163, 41606, 42590);
