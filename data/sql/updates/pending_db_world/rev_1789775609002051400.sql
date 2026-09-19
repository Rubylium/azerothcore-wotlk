-- Icecrown Citadel: the Gunship Battle can be skipped. The gunship captain (Muradin Bronzebeard for the Alliance,
-- High Overlord Saurfang for the Horde) gets a second option; boss_icecrown_gunship_battle.cpp marks the battle won
-- and sends everyone in the instance to Deathbringer's Rise.
DELETE FROM `gossip_menu_option` WHERE `MenuID` IN (10875, 10954) AND `OptionID` = 1;
INSERT INTO `gossip_menu_option` (`MenuID`, `OptionID`, `OptionIcon`, `OptionText`, `OptionBroadcastTextID`, `OptionType`, `OptionNpcFlag`, `ActionMenuID`, `ActionPoiID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `VerifiedBuild`) VALUES
(10875, 1, 0, 'Skip the battle and take us straight to Deathbringer Saurfang.', 0, 1, 1, 0, 0, 0, 0, '', 0, 0),
(10954, 1, 0, 'Skip the battle and take us straight to Deathbringer Saurfang.', 0, 1, 1, 0, 0, 0, 0, '', 0, 0);

DELETE FROM `gossip_menu_option_locale` WHERE `MenuID` IN (10875, 10954) AND `OptionID` = 1 AND `Locale` = 'frFR';
INSERT INTO `gossip_menu_option_locale` (`MenuID`, `OptionID`, `Locale`, `OptionText`, `BoxText`) VALUES
(10875, 1, 'frFR', 'Passons la bataille : emmenez-nous directement au Porte-mort Saurcroc.', NULL),
(10954, 1, 'frFR', 'Passons la bataille : emmenez-nous directement au Porte-mort Saurcroc.', NULL);
