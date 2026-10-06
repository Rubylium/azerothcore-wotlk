# Player rotation regression

Requires mod-playerbots, the tested class modules, custom talents/spell data and the combat bench on the local realm.
Uses fresh client-controlled GM characters with class AoE presets and normal resource costs/cooldowns.
Rogues use fixed gear; Frost DK uses a two-handed axe. Caster checks use baseline equipment.
God mode prevents deaths during observation; GM invisibility is disabled before combat.

Run with the usual `E2E_*` database and auth environment:

```powershell
go test -tags=e2e ./suites/modules/playerbotRotation -count=1 -v -timeout 12m
```

- `BossAndPack`: 60-second boss and 30-second five-enemy bench windows. Checks successful damage finishers,
  custom cooldowns, AoE builders/finishers, and absence of Sinister Strike fallback.
- `UtgardeKeep`: normal dungeon forge pack and Prince Keleseth, 20 seconds each in a fresh solo instance.
  Checks real dungeon threat detection and successful Subtlety attacks. This is not a full dungeon clear or DPS benchmark.
- `Mage_SingleTargetKit`: Arcane, Fire and Frost custom abilities on a boss; no untalented Scorch or legacy
  Combustion; capped Icicles spent through Ice Lance.
- `Rogue_AssassinationKit`: Rupture plus Thistle Tea, Marked for Death, Vendetta and Exsanguinate.
- `Rogue_CombatFinishers`: Slice and Dice, Rupture and Eviscerate remain reachable with the combo-point guards.
- `FrostDK_BreathPreservesFuel`: natural runic power starts Breath; competing runic spenders stay blocked during it.
- `Rotation_MultiDot`: Affliction and Shadow apply DoTs to at least three engaged pack enemies while retaining
  the player's selection. Seed and Shadow Crash are unlearned in the fixture to exclude automatic spreading.

The original detached player AI cast no AoE spells against five bench enemies. Registered bot-only tests miss that bug.
Bench sessions are scoped to their owner. Test characters, temporary bench summons and rotation state are cleaned up.
