# Necromancer

Implements class 13 and its first DPS specialization, **Legion**.

- Native spellbook/talent DBC rows: `localTools/necromancer/Spells.ps1`
- Soul resource, scaling and level unlocks: `NecromancerCommon.cpp`
- Capped temporary army and owner-target AI: `NecromancerMinions.cpp`
- Builders, spenders and mana sustain: `NecromancerSpells.cpp`
- World rows and script bindings: `data/sql/db-world/base/necromancer.sql`

The class has no trainer. Login and level changes teach every baseline ability unlocked by the character's level.
