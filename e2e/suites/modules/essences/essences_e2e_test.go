//go:build e2e

package essences_test

import (
	"encoding/binary"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/client"
	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

// Requires mod-stat-growth and its default per-essence amounts and tier multipliers.
func TestEssences_CappedFamiliesKeepTierAndPreserveSavedTotals(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"short", "essences"}, Runtime: "short", Category: "modules/essences"})
	bot := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "EssCap", Level: 80})
	seedCappedEssences(t, bot)
	for _, reward := range []struct {
		entry  uint32
		points uint64
	}{{2461, 1}, {3441, 3}, {2050, 1}, {1950, 3}} {
		before := essencePoints(t, bot)
		consumeEssence(t, bot, reward.entry)
		after := essencePoints(t, bot)
		for _, source := range []string{"mod_stat_growth_resource", "mod_stat_growth_fortune"} {
			if after[source] != 10000 {
				t.Fatalf("saved %s changed: %d", source, after[source])
			}
		}
		changed := 0
		for source, perPoint := range map[string]uint64{
			"mod_stat_growth": 1, "mod_stat_growth_xp": 10, "mod_stat_growth_vitality": 1,
		} {
			if delta := after[source] - before[source]; delta != 0 {
				changed++
				if delta != reward.points*perPoint {
					t.Fatalf("entry %d lost tier: %s gained %d", reward.entry, source, delta)
				}
			}
		}
		if changed != 1 {
			t.Fatalf("entry %d changed %d uncapped families, want one", reward.entry, changed)
		}
	}
	t.Log("PASS: capped essence families redirect at the same tier; stored over-cap totals remain intact")
}

func TestEssences_FortuneCapsEarnedGoldAndDoesNotMultiplyTrades(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"short", "essences", "trade", "multi_bot"},
		Runtime: "short", Category: "modules/essences"})
	bots := e2eharness.NewScenario(t, e2eharness.ScenarioOpts{Prefix: "EssGold", Count: 2, Level: 80})
	a, b := bots[0], bots[1]
	for _, bot := range bots {
		verifyAddon(t, bot)
	}
	for _, bot := range bots {
		seedCappedEssences(t, bot)
		bot.SetMoney(t, 100000)
		bot.TeleportPad(t, e2eharness.PackagePad(t))
	}
	a.CombatReady(t)
	worldDB, err := e2eharness.OpenWorldDB()
	if err != nil {
		e2eharness.Preconditionf(t, "world database: %v", err)
	}
	defer worldDB.Close()
	var creature uint32
	if err := worldDB.QueryRow("SELECT entry FROM creature_template WHERE mingold>0 AND maxlevel<=20 " +
		"AND npcflag=0 AND ScriptName='' AND lootid<>0 ORDER BY entry LIMIT 1").Scan(&creature); err != nil {
		e2eharness.Preconditionf(t, "no low-level gold-bearing creature: %v", err)
	}
	guid := a.SpawnKillLootable(t, creature, 45*time.Second)
	goldPackets := make(chan uint32, 1)
	cancel := a.World.AddPacketHook(func(opcode uint16, data []byte) {
		if opcode == client.SmsgLootResponse && len(data) >= 13 && binary.LittleEndian.Uint64(data[:8]) == guid {
			select {
			case goldPackets <- binary.LittleEndian.Uint32(data[9:13]):
			default:
			}
		}
	})
	defer cancel()
	a.OpenLoot(t, guid, 10*time.Second)
	var baseGold uint32
	select {
	case baseGold = <-goldPackets:
	case <-time.After(5 * time.Second):
		e2eharness.Preconditionf(t, "no loot gold packet")
	}
	if baseGold == 0 {
		e2eharness.Preconditionf(t, "loot fixture has no gold on this database")
	}
	before := a.MoneyAfterSave(t)
	if err := a.World.LootMoney(); err != nil {
		e2eharness.HarnessFailf(t, "loot money: %v", err)
	}
	a.FlushWorld(t)
	a.AssertMoneyEqual(t, before+2*baseGold)
	a.LootRelease(t, guid)
	a.CombatStop(t)
	b.CombatStop(t)
	pad := e2eharness.PackagePad(t)
	a.Teleport(t, pad.X, pad.Y, pad.Z, pad.Map)
	b.Teleport(t, pad.X+1, pad.Y, pad.Z, pad.Map)
	aBefore, bBefore := a.MoneyAfterSave(t), b.MoneyAfterSave(t)
	e2eharness.OpenTrade(t, a, b)
	a.SetTradeGold(t, 1000)
	e2eharness.CompleteTrade(t, a, b)
	a.AssertMoneyEqual(t, aBefore-1000)
	b.AssertMoneyEqual(t, bBefore+1000)
	t.Log("PASS: over-cap Fortune pays exactly double earned gold and no bonus on transferred gold")
}
