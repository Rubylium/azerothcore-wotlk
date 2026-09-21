-- Necromancer spell bindings and temporary army templates.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('NecromancerAbilitySpellScript', 'NecromancerPeriodicAuraScript');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90400, 'NecromancerAbilitySpellScript'), (90402, 'NecromancerAbilitySpellScript'),
(90403, 'NecromancerAbilitySpellScript'), (90403, 'NecromancerPeriodicAuraScript'),
(90404, 'NecromancerAbilitySpellScript'), (90405, 'NecromancerAbilitySpellScript'),
(90406, 'NecromancerAbilitySpellScript'), (90407, 'NecromancerPeriodicAuraScript'),
(90408, 'NecromancerAbilitySpellScript'), (90409, 'NecromancerAbilitySpellScript'),
(90410, 'NecromancerAbilitySpellScript'), (90411, 'NecromancerAbilitySpellScript'),
(90412, 'NecromancerAbilitySpellScript'), (90413, 'NecromancerAbilitySpellScript'),
(90414, 'NecromancerAbilitySpellScript'), (90415, 'NecromancerAbilitySpellScript'),
(90518, 'NecromancerAbilitySpellScript'), (90533, 'NecromancerAbilitySpellScript'),
(90561, 'NecromancerAbilitySpellScript'), (90424, 'NecromancerPeriodicAuraScript');

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 910100 AND 910103;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 910100 AND 910103;

INSERT INTO `creature_template` (
`entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,`KillCredit1`,`KillCredit2`,`name`,`subname`,`IconName`,`gossip_menu_id`,
`minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,`detection_range`,`rank`,`dmgschool`,
`DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,`family`,`type`,
`type_flags`,`lootid`,`pickpocketloot`,`skinloot`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,`AIName`,`MovementType`,`HoverHeight`,`HealthModifier`,
`ManaModifier`,`ArmorModifier`,`ExperienceModifier`,`RacialLeader`,`movementId`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`VerifiedBuild`)
SELECT 910100,0,0,0,0,0,'Guerrier squelette','Serviteur du Nécromancien','',0,1,80,2,35,0,1,1.14286,1,1,40,0,0,1,2000,2000,1,1,1,0,0,0,0,6,0,0,0,0,0,0,0,0,'',0,1,1,1,1,1,0,0,1,0,0,'NecromancerSkeletonAI',NULL
UNION ALL SELECT 910101,0,0,0,0,0,'Archer maudit','Serviteur du Nécromancien','',0,1,80,2,35,0,1,1.14286,1,1,40,0,0,1,2000,1900,1,1,1,0,0,0,0,6,0,0,0,0,0,0,0,0,'',0,1,1,1,1,1,0,0,1,0,0,'NecromancerArcherAI',NULL
UNION ALL SELECT 910102,0,0,0,0,0,'Mage de peste','Serviteur du Nécromancien','',0,1,80,2,35,0,1,1.14286,1,1,40,0,5,1,2000,2500,1,1,8,0,0,0,0,6,0,0,0,0,0,0,0,0,'',0,1,1,1,1,1,0,0,1,0,0,'NecromancerMageAI',NULL
UNION ALL SELECT 910103,0,0,0,0,0,'Abomination','Serviteur du Nécromancien','',0,1,80,2,35,0,1,1.14286,1,1,40,1,0,1,2000,2000,1,1,1,0,0,0,0,6,0,0,0,0,0,0,0,0,'',0,1,1,1,1,1,0,0,1,0,0,'NecromancerAbominationAI',NULL;

-- One stable visual per role. Scaling and combat behavior are owned by the module, not these source templates.
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 910100, 0, `CreatureDisplayID`, `DisplayScale`, 1, `VerifiedBuild` FROM `creature_template_model` WHERE `CreatureID`=26126 ORDER BY `Probability` DESC LIMIT 1;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 910101, 0, `CreatureDisplayID`, `DisplayScale`, 1, `VerifiedBuild` FROM `creature_template_model` WHERE `CreatureID`=14489 ORDER BY `Probability` DESC LIMIT 1;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 910102, 0, `CreatureDisplayID`, `DisplayScale`, 1, `VerifiedBuild` FROM `creature_template_model` WHERE `CreatureID`=17903 ORDER BY `Probability` DESC LIMIT 1;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 910103, 0, `CreatureDisplayID`, `DisplayScale`, 1, `VerifiedBuild` FROM `creature_template_model` WHERE `CreatureID`=26518 ORDER BY `Probability` DESC LIMIT 1;
