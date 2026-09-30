//go:build e2e

package essences_test

import (
	"encoding/binary"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	_ "github.com/go-sql-driver/mysql"
)

func verifyAddon(t *testing.T, bot *e2eharness.ScenarioBot) {
	t.Helper()
	if err := bot.World.SendChatMessage(7, 0xffffffff, bot.Name+"\x00PersonalLoot\tHELLO"); err != nil {
		e2eharness.HarnessFailf(t, "Gear Bonuses handshake: %v", err)
	}
	bot.FlushWorld(t)
}

// Only fresh harness characters are seeded, while offline. This exercises old over-cap saved totals without
// changing production accounts or relying on test-only server commands.
func seedCappedEssences(t *testing.T, bot *e2eharness.ScenarioBot) {
	t.Helper()
	verifyAddon(t, bot)
	if err := bot.World.SendLogout(); err != nil {
		e2eharness.HarnessFailf(t, "logout: %v", err)
	}
	if err := bot.World.WaitForLogout(30 * time.Second); err != nil {
		e2eharness.HarnessFailf(t, "wait logout: %v", err)
	}
	bot.Close()
	deadline := time.Now().Add(5 * time.Second)
	for {
		var online uint8
		if err := bot.CharDB.QueryRow("SELECT online FROM characters WHERE guid=?", bot.GUID&0xffffffff).
			Scan(&online); err != nil {
			e2eharness.HarnessFailf(t, "wait for offline save: %v", err)
		}
		if online == 0 {
			break
		}
		if time.Now().After(deadline) {
			e2eharness.Preconditionf(t, "character remained online after logout")
		}
		time.Sleep(50 * time.Millisecond)
	}
	for _, source := range []string{"mod_stat_growth_resource", "mod_stat_growth_fortune"} {
		if _, err := bot.CharDB.Exec("REPLACE INTO character_settings (guid, source, data) VALUES (?, ?, ?)",
			bot.GUID&0xffffffff, source, "10000 "); err != nil {
			e2eharness.Preconditionf(t, "seed %s: %v", source, err)
		}
	}
	sessions := e2eharness.LoginBots(t, []e2eharness.BotIdent{{
		Account: bot.Ident.Account, CharName: bot.Name, Race: bot.Ident.Race, Class: bot.Ident.Class,
	}})
	bot.Session = sessions[0]
	bot.WaitInWorld(t, 30*time.Second)
	verifyAddon(t, bot)
}

func essencePoints(t *testing.T, bot *e2eharness.ScenarioBot) map[string]uint64 {
	t.Helper()
	bot.Save(t)
	bot.FlushWorld(t)
	rows, err := bot.CharDB.Query("SELECT source, data FROM character_settings WHERE guid=? AND source IN "+
		"('mod_stat_growth', 'mod_stat_growth_xp', 'mod_stat_growth_vitality', "+
		"'mod_stat_growth_resource', 'mod_stat_growth_fortune')", bot.GUID&0xffffffff)
	if err != nil {
		e2eharness.HarnessFailf(t, "read essence totals: %v", err)
	}
	defer rows.Close()
	result := make(map[string]uint64)
	for rows.Next() {
		var source, data string
		if err := rows.Scan(&source, &data); err != nil {
			e2eharness.HarnessFailf(t, "scan essence totals: %v", err)
		}
		for _, field := range strings.Fields(data) {
			value, err := strconv.ParseUint(field, 10, 32)
			if err != nil {
				e2eharness.HarnessFailf(t, "parse essence total: %v", err)
			}
			result[source] += value
		}
	}
	if err := rows.Err(); err != nil {
		e2eharness.HarnessFailf(t, "read essence rows: %v", err)
	}
	return result
}

func consumeEssence(t *testing.T, bot *e2eharness.ScenarioBot, entry uint32) {
	t.Helper()
	worldDB, err := e2eharness.OpenWorldDB()
	if err != nil {
		e2eharness.Preconditionf(t, "world database: %v", err)
	}
	defer worldDB.Close()
	var spell uint32
	var script string
	if err := worldDB.QueryRow("SELECT spellid_1, ScriptName FROM item_template WHERE entry=?", entry).
		Scan(&spell, &script); err != nil || spell == 0 || !strings.Contains(script, "essence") {
		e2eharness.Preconditionf(t, "essence %d needs module item data: spell=%d script=%q err=%v",
			entry, spell, script, err)
	}
	bag, slot := bot.AddItemWait(t, entry, 1)
	bot.Save(t)
	bot.FlushWorld(t)
	var itemGUID uint64
	deadline := time.Now().Add(5 * time.Second)
	for {
		err := bot.CharDB.QueryRow("SELECT ii.guid FROM character_inventory ci JOIN item_instance ii ON ii.guid=ci.item "+
			"WHERE ci.guid=? AND ii.itemEntry=? LIMIT 1", bot.GUID&0xffffffff, entry).Scan(&itemGUID)
		if err == nil {
			break
		}
		if time.Now().After(deadline) {
			e2eharness.Preconditionf(t, "essence inventory: %v", err)
		}
		time.Sleep(50 * time.Millisecond)
	}
	// SpellHandler.cpp HandleUseItemOpcode: bag, slot, cast count, spell, full item GUID, glyph, flags, target mask.
	packet := make([]byte, 24)
	packet[0], packet[1], packet[2] = bag, slot, 1
	binary.LittleEndian.PutUint32(packet[3:7], spell)
	binary.LittleEndian.PutUint64(packet[7:15], itemGUID|0x4000000000000000)
	if err := bot.World.SendPacketRaw(0x00ab, packet); err != nil {
		e2eharness.HarnessFailf(t, "use essence: %v", err)
	}
	bot.FlushWorld(t)
	deadline = time.Now().Add(5 * time.Second)
	for {
		got := bot.InventoryCount(t, entry)
		if got == 0 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("essence %d was not consumed: count=%d", entry, got)
		}
		time.Sleep(50 * time.Millisecond)
	}
}
