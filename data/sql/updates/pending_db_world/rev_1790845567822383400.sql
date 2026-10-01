--
-- Live tuning (src/server/game/Tuning/LiveTuning.h, .tune): the overrides set while the server runs, kept until they
-- are reset or baked into the code (localTools/tuning/bakeTuning.py)
CREATE TABLE IF NOT EXISTS `live_tuning` (
    `Key` VARCHAR(100) NOT NULL COMMENT 'a knob of the code''s (LiveTuning::Knob), module.name',
    `Value` DOUBLE NOT NULL,
    PRIMARY KEY (`Key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Live tuning: knob overrides';

CREATE TABLE IF NOT EXISTS `live_tuning_spell` (
    `SpellId` INT UNSIGNED NOT NULL,
    `Multiplier` FLOAT NOT NULL COMMENT 'on the spell''s damage and healing',
    PRIMARY KEY (`SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Live tuning: spell damage and healing multipliers';

DELETE FROM `command` WHERE `name` IN ('tune', 'tune list', 'tune get', 'tune set', 'tune reset', 'tune spell', 'tune reload');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('tune', 2, 'Syntax: .tune $subcommand\nLive tuning: the code''s knobs and spell multipliers, changed while the server runs.'),
('tune list', 2, 'Syntax: .tune list [$filter]\nThe knobs whose key holds $filter (all without one), their value and the code''s, then the spell multipliers.'),
('tune get', 2, 'Syntax: .tune get $key\nA knob''s value and the code''s.'),
('tune set', 2, 'Syntax: .tune set $key $value\nOverrides a knob at once, saved until reset or baked into the code. Its default value drops the override.'),
('tune reset', 2, 'Syntax: .tune reset $key|all\nA knob back to the code''s value; all: every knob and spell multiplier.'),
('tune spell', 2, 'Syntax: .tune spell $spellId [$multiplier]\nA spell''s damage and healing multiplier (1 drops it); without one, shows it.'),
('tune reload', 2, 'Syntax: .tune reload\nThe overrides again from the world database.');
