-- L'Infini's health at Défi I from a raid group of its profile (300 / 100) measured on the simulation bench
-- (Power::RaidBossHealth: 290 s x 0.85 it can be hit, the clear share 0.85, the raid factor 1.048), up from the
-- model's hard check: 19.4 million to 20.4 million. The tiers above grow by the measured raid power (ChallengeTiers.h).
UPDATE `creature_template` SET `HealthModifier` = 1461 WHERE `entry` = 930000;
