-- Pestiféré (class 12, mask 2048): the Dungeon Finder's Satchel of Helpful Goods picks its item by class. The two
-- low-level satchels (51999, 52000) have no death knight group, as death knights start at 55, so a Pestiféré, which
-- counts as one, got an empty satchel. It takes the warrior and paladin item, the gear it wears at those levels.
-- The higher satchels already give it the plate item through the death knight bit.
UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 2048
WHERE `SourceTypeOrReferenceId` = 10 AND `SourceGroup` IN (10038, 10041) AND `ConditionTypeOrReference` = 15;
