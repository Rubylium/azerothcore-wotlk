CREATE TABLE IF NOT EXISTS `mod_combat_run` (
  `run_id` BIGINT UNSIGNED NOT NULL,
  `run_type` TINYINT UNSIGNED NOT NULL COMMENT '1=M+, 2=raid boss',
  `result` TINYINT UNSIGNED NOT NULL COMMENT '1=timed, 2=depleted, 3=abandoned, 4=raid kill',
  `map_id` SMALLINT UNSIGNED NOT NULL,
  `instance_id` INT UNSIGNED NOT NULL,
  `dungeon_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `difficulty` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `key_level` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `time_limit_seconds` INT UNSIGNED NOT NULL DEFAULT 0,
  `boss_entry` INT UNSIGNED NOT NULL DEFAULT 0,
  `boss_name` VARCHAR(120) NOT NULL DEFAULT '',
  `started_at_ms` BIGINT UNSIGNED NOT NULL,
  `ended_at_ms` BIGINT UNSIGNED NOT NULL,
  `duration_seconds` INT UNSIGNED NOT NULL,
  `timer_deaths` INT UNSIGNED NOT NULL DEFAULT 0,
  `human_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `bot_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `total_damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`run_id`),
  KEY `idx_type_time` (`run_type`, `ended_at_ms`),
  KEY `idx_mythic` (`dungeon_id`, `key_level`),
  KEY `idx_raid` (`map_id`, `boss_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_combat_participant` (
  `run_id` BIGINT UNSIGNED NOT NULL,
  `guid` INT UNSIGNED NOT NULL,
  `name` VARCHAR(32) NOT NULL,
  `is_bot` TINYINT UNSIGNED NOT NULL,
  `class_id` TINYINT UNSIGNED NOT NULL,
  `race_id` TINYINT UNSIGNED NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL,
  `role_mask` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `spec_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `item_level` DECIMAL(7,2) NOT NULL DEFAULT 0,
  `max_health` INT UNSIGNED NOT NULL DEFAULT 0,
  `strength` INT UNSIGNED NOT NULL DEFAULT 0,
  `agility` INT UNSIGNED NOT NULL DEFAULT 0,
  `stamina` INT UNSIGNED NOT NULL DEFAULT 0,
  `intellect` INT UNSIGNED NOT NULL DEFAULT 0,
  `spirit` INT UNSIGNED NOT NULL DEFAULT 0,
  `attack_power` INT UNSIGNED NOT NULL DEFAULT 0,
  `spell_power` INT NOT NULL DEFAULT 0,
  `melee_crit` FLOAT NOT NULL DEFAULT 0,
  `spell_crit` FLOAT NOT NULL DEFAULT 0,
  `melee_haste` FLOAT NOT NULL DEFAULT 0,
  `spell_haste` FLOAT NOT NULL DEFAULT 0,
  `damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `boss_damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `trash_damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `pet_damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `hits` INT UNSIGNED NOT NULL DEFAULT 0,
  `deaths` INT UNSIGNED NOT NULL DEFAULT 0,
  `active_ms` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`run_id`, `guid`),
  KEY `idx_class_spec` (`class_id`, `spec_id`),
  KEY `idx_bot` (`is_bot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_combat_ability` (
  `run_id` BIGINT UNSIGNED NOT NULL,
  `guid` INT UNSIGNED NOT NULL,
  `spell_id` INT UNSIGNED NOT NULL,
  `damage_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `boss_damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `trash_damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `hits` INT UNSIGNED NOT NULL DEFAULT 0,
  `pet_hits` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`run_id`, `guid`, `spell_id`),
  KEY `idx_spell` (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_combat_target` (
  `run_id` BIGINT UNSIGNED NOT NULL,
  `guid` INT UNSIGNED NOT NULL,
  `creature_entry` INT UNSIGNED NOT NULL,
  `is_boss` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `damage` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `hits` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`run_id`, `guid`, `creature_entry`),
  KEY `idx_target` (`creature_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_combat_route_event` (
  `run_id` BIGINT UNSIGNED NOT NULL,
  `sequence` INT UNSIGNED NOT NULL,
  `offset_ms` BIGINT UNSIGNED NOT NULL,
  `event_type` VARCHAR(32) NOT NULL,
  `actor_guid` INT UNSIGNED NOT NULL DEFAULT 0,
  `target_entry` INT UNSIGNED NOT NULL DEFAULT 0,
  `position_x` FLOAT NOT NULL DEFAULT 0,
  `position_y` FLOAT NOT NULL DEFAULT 0,
  `position_z` FLOAT NOT NULL DEFAULT 0,
  `details` VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`run_id`, `sequence`),
  KEY `idx_event_type` (`event_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
