UPDATE `item_template` AS `item`
INNER JOIN `mod_stat_growth_item_backup` AS `backup` ON `backup`.`entry` = `item`.`entry`
SET `item`.`class` = `backup`.`class`,
    `item`.`subclass` = `backup`.`subclass`,
    `item`.`name` = `backup`.`name`,
    `item`.`displayid` = `backup`.`displayid`,
    `item`.`Quality` = `backup`.`Quality`,
    `item`.`Flags` = `backup`.`Flags`,
    `item`.`BuyCount` = `backup`.`BuyCount`,
    `item`.`InventoryType` = `backup`.`InventoryType`,
    `item`.`AllowableClass` = `backup`.`AllowableClass`,
    `item`.`AllowableRace` = `backup`.`AllowableRace`,
    `item`.`ItemLevel` = `backup`.`ItemLevel`,
    `item`.`RequiredLevel` = `backup`.`RequiredLevel`,
    `item`.`maxcount` = `backup`.`maxcount`,
    `item`.`stackable` = `backup`.`stackable`,
    `item`.`bonding` = `backup`.`bonding`,
    `item`.`description` = `backup`.`description`,
    `item`.`SoundOverrideSubclass` = `backup`.`SoundOverrideSubclass`,
    `item`.`spellid_1` = `backup`.`spellid_1`,
    `item`.`spelltrigger_1` = `backup`.`spelltrigger_1`,
    `item`.`spellcharges_1` = `backup`.`spellcharges_1`,
    `item`.`ScriptName` = `backup`.`ScriptName`,
    `item`.`VerifiedBuild` = `backup`.`VerifiedBuild`;

UPDATE `item_template_locale` AS `locale`
INNER JOIN `mod_stat_growth_item_locale_backup` AS `backup`
    ON `backup`.`ID` = `locale`.`ID` AND `backup`.`locale` = `locale`.`locale`
SET `locale`.`Name` = `backup`.`Name`, `locale`.`Description` = `backup`.`Description`;
