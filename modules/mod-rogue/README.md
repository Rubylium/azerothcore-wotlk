# mod-rogue

The Rogue on the retail-style talent trees (localTools/rogue/talentTree.json, mod-custom-classes TalentTree.cpp): the
effects of its talents and abilities that spell data alone cannot carry. Spell data: localTools/rogue/Spells.ps1.

Every rogue also gets two changes:

- Backstab works from any side, and deals 20% more damage from behind (the "must be behind" requirement is dropped
  in data/sql/db-world/base/rogue_backstab.sql).
- Poisons put on a weapon never wear off: they are refreshed to a full hour every few minutes.

The Combat tree is the Crimson Duelist rework, scripted in mod-stat-growth (CombatRogue*.cpp). Its kit is only
taught while Combat is the rogue's specialization.
