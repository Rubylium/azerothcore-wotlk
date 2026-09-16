UPDATE `item_template`
SET `subclass` = 5,
    `SoundOverrideSubclass` = 0,
    `spellid_1` = 46168
WHERE (`entry` = 42590);

UPDATE `item_template`
SET `SoundOverrideSubclass` = 0,
    `spellid_1` = 46168
WHERE (`entry` = 39163);
