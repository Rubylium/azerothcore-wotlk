# mod-rogue

The Rogue on the retail-style talent trees (localTools/rogue/talentTree.json, mod-custom-classes TalentTree.cpp): the
effects of its talents and abilities that spell data alone cannot carry. Spell data: localTools/rogue/Spells.ps1.

Every rogue also gets two changes:

- Backstab works from any side, and deals 20% more damage from behind (the "must be behind" requirement is dropped
  in data/sql/db-world/base/rogue_backstab.sql).
- Poisons put on a weapon never wear off: they are refreshed to a full hour every few minutes.

The Combat tree is the Crimson Duelist rework, scripted in mod-stat-growth (CombatRogue*.cpp). Its kit is only
taught while Combat is the rogue's specialization.

## Finesse's looks

Finesse wears retail's looks, put together from the effects and sounds the Ascension client carries
(localTools/rogue/ascensionVisuals.json, imported by localTools/ascensionImport/importVisuals.py into
client-assets/imported): Backstab, Ambush (Shadowstrike), Eviscerate, Hemorrhage and Shadow Dance through
localTools/rogue/StockSpells.ps1, the spec's own abilities through Spells.ps1. What spell data cannot show is played
here, by kit id:

- Poudre noire's hit on every enemy it reaches (kit 77900), Technique secrète's on every enemy at each strike (77901).
- Technique secrète's two shadows: a creature (910300, data/sql/db-world/base/rogue_shadow_clone.sql) summoned on each
  side of the target for a moment, wearing the rogue's look and weapons (Mirror Image's Clone Me!) under a dark
  see-through skin (spell 92327), stabbing with its strike (kit 77902).
