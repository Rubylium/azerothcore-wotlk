-- Nécromancien (class 13): the spells its characters were handed too early.
--
-- Its SkillLineAbility rows were written with AcquireMethod 2, which teaches a spell with its whole skill
-- line, and the class carries skill line 901 from creation: every character opened with the entire kit,
-- including the level 60 army and the three talent actives. The DBC now says 0 for a custom class's own
-- skill line (localTools/patchSinisterStrike.ps1), so nothing is over-granted from here on, but what was
-- already learned lives in `character_spell` and would stay forever.
--
-- Each spell goes back to whoever is not yet the level mod-necromancer learns it at
-- (NecromancerCommon.cpp, AbilityUnlocks); the module hands it back on the level-up or at the next login.
-- The three talent actives are taken from everyone: their talent rank is what teaches them.
DELETE `cs`
FROM `character_spell` `cs`
JOIN `characters` `c` ON `c`.`guid` = `cs`.`guid`
WHERE `c`.`class` = 13
  AND (
        (`cs`.`spell` = 90403 AND `c`.`level` < 2)
     OR (`cs`.`spell` = 90405 AND `c`.`level` < 4)
     OR (`cs`.`spell` = 90404 AND `c`.`level` < 6)
     OR (`cs`.`spell` = 90406 AND `c`.`level` < 8)
     OR (`cs`.`spell` = 90407 AND `c`.`level` < 10)
     OR (`cs`.`spell` = 90408 AND `c`.`level` < 14)
     OR (`cs`.`spell` = 90409 AND `c`.`level` < 18)
     OR (`cs`.`spell` = 90410 AND `c`.`level` < 22)
     OR (`cs`.`spell` = 90411 AND `c`.`level` < 28)
     OR (`cs`.`spell` = 90412 AND `c`.`level` < 34)
     OR (`cs`.`spell` = 90413 AND `c`.`level` < 42)
     OR (`cs`.`spell` = 90414 AND `c`.`level` < 52)
     OR (`cs`.`spell` = 90415 AND `c`.`level` < 60)
     OR `cs`.`spell` IN (90518, 90533, 90561)
  );
