-- Model row for the custom adaptive dummy template (cloned from Grandmaster's Training Dummy).
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900100;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT 900100, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, NULL
FROM `creature_template_model`
WHERE `CreatureID` = 31144;
