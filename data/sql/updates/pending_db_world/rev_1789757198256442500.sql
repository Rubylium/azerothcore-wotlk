-- Pestiféré (class 12): it starts where a human warrior does, so it gets the human warrior's Northshire class
-- quest too (Simple Letter, from Marshal McBride to the warrior trainer). The other Northshire quests allow
-- every class already.
UPDATE `quest_template_addon` SET `AllowableClasses` = `AllowableClasses` | 2048 WHERE `ID` = 3100;
