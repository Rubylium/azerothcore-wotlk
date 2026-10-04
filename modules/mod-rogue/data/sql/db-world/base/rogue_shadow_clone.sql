-- Technique secrète's shadows (modules/mod-rogue, RogueTalents.cpp): a creature summoned beside the rogue's target for
-- a moment, wearing the rogue's look (Clone Me!) and weapons. It cannot be selected, attacked or fight; Mirror Image's
-- model until the clone takes over.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 910300;
DELETE FROM `creature_template` WHERE `entry` = 910300;

INSERT INTO `creature_template` (
`entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,`KillCredit1`,`KillCredit2`,`name`,`subname`,`IconName`,`gossip_menu_id`,
`minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,`detection_range`,`rank`,`dmgschool`,
`DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,`family`,`type`,
`type_flags`,`lootid`,`pickpocketloot`,`skinloot`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,`AIName`,`MovementType`,`HoverHeight`,`HealthModifier`,
`ManaModifier`,`ArmorModifier`,`ExperienceModifier`,`RacialLeader`,`movementId`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`VerifiedBuild`) VALUES
(910300,0,0,0,0,0,'Ombre','','',0,80,80,2,35,0,1,1.14286,1,1,0,0,0,1,2000,2000,1,1,1,33555202,0,0,0,7,0,0,0,0,0,0,0,0,'',0,1,1,1,1,1,0,0,1,0,0,'',NULL);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(910300, 0, 11686, 1, 1, NULL);
