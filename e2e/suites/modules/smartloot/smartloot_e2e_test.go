//go:build e2e

package smartloot_test

import (
	"bytes"
	"encoding/binary"
	"strings"
	"sync"
	"testing"
	"time"

	_ "github.com/go-sql-driver/mysql"

	"github.com/azerothcore/AzerothGhost/client"
	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

// Rubyrogue's gear (2026-10-02): generated variants from 297 to 461 everywhere but the feet, which wear the stock
// Frostbitten Fur Boots (277). Those boots are the only leather agility feet the generator has a base for, and owning
// any copy of an item used to rule out all its variants: the feet never got loot again. Entries are generated
// variants (GeneratedItemBase * (variant + 1) + base), built at startup.
// By equipment slot (EQUIPMENT_SLOT_*)
var rubyrogueGear = map[uint8]uint32{
	0:  9094681, // head, 460
	1:  2807093, // neck
	2:  2017334, // shoulders
	4:  2999776, // chest
	5:  312851,  // waist, 297: the second weakest, its only base (Astrylian's Sutured Cinch) owned too
	6:  2409993, // legs
	7:  50607,   // feet: Frostbitten Fur Boots, stock 277
	8:  9094638, // wrists
	9:  1689651, // hands
	10: 2606508, // finger
	11: 2020656, // finger
	12: 2151740, // trinket
	13: 2151742, // trinket
	14: 2213341, // back
	15: 2147888, // main hand
	16: 2016756, // off hand
	17: 8964776, // ranged, 310
}

const (
	slotFeet  uint8 = 7
	chatSystem      = 0x00
	// Seeded items' guids: far above any the server hands out, so none of its own can meet them
	seededItemGuids = 0xE0000000
	// MAX_ENCHANTMENT_SLOT x 3 zeroes, as the core saves an item without enchantments
	noEnchantments = "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
)

// The character's equipment, written while it is offline: everything worn and in the backpack replaced by gear.
// Equipping through the client could not do it: the starter items it takes off fill the backpack, and the last
// pieces never got in.
func seedEquipment(t *testing.T, bot *e2eharness.ScenarioBot, gear map[uint8]uint32) {
	t.Helper()
	if err := bot.World.SendLogout(); err != nil {
		e2eharness.HarnessFailf(t, "logout: %v", err)
	}
	if err := bot.World.WaitForLogout(30 * time.Second); err != nil {
		e2eharness.HarnessFailf(t, "wait logout: %v", err)
	}
	bot.Close()
	owner := bot.GUID & 0xffffffff
	deadline := time.Now().Add(5 * time.Second)
	for {
		var online uint8
		if err := bot.CharDB.QueryRow("SELECT online FROM characters WHERE guid=?", owner).Scan(&online); err != nil {
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

	for _, query := range []string{
		"DELETE ii FROM item_instance ii JOIN character_inventory ci ON ci.item = ii.guid WHERE ci.guid = ? AND ci.bag = 0",
		"DELETE FROM character_inventory WHERE guid = ? AND bag = 0",
	} {
		if _, err := bot.CharDB.Exec(query, owner); err != nil {
			e2eharness.Preconditionf(t, "clear equipment: %v", err)
		}
	}
	for slot, entry := range gear {
		item := uint64(seededItemGuids) + uint64(owner&0xffff)*32 + uint64(slot)
		if _, err := bot.CharDB.Exec("REPLACE INTO item_instance (guid, itemEntry, owner_guid, count, flags, "+
			"enchantments, durability) VALUES (?, ?, ?, 1, 1, ?, 100)", item, entry, owner, noEnchantments); err != nil {
			e2eharness.Preconditionf(t, "seed item %d: %v", entry, err)
		}
		if _, err := bot.CharDB.Exec("INSERT INTO character_inventory (guid, bag, slot, item) VALUES (?, 0, ?, ?)",
			owner, slot, item); err != nil {
			e2eharness.Preconditionf(t, "seed slot %d: %v", slot, err)
		}
	}

	sessions := e2eharness.LoginBots(t, []e2eharness.BotIdent{{
		Account: bot.Ident.Account, CharName: bot.Name, Race: bot.Ident.Race, Class: bot.Ident.Class,
	}})
	bot.Session = sessions[0]
	bot.WaitInWorld(t, 30*time.Second)
	verifyAddon(t, bot)
}

func verifyAddon(t *testing.T, bot *e2eharness.ScenarioBot) {
	t.Helper()
	if err := bot.World.SendChatMessage(7, 0xffffffff, bot.Name+"\x00PersonalLoot\tHELLO"); err != nil {
		e2eharness.HarnessFailf(t, "Gear Bonuses handshake: %v", err)
	}
	bot.FlushWorld(t)
}

// systemMessages collects the system chat lines the server sends the bot (the GM commands' output)
func systemMessages(bot *e2eharness.ScenarioBot) (lines func() []string, cancel func()) {
	var lock sync.Mutex
	var collected []string
	cancel = bot.World.AddPacketHook(func(opcode uint16, data []byte) {
		if opcode != client.SmsgMessageChat || len(data) < 1 || data[0] != chatSystem {
			return
		}
		r := bytes.NewReader(data[1:])
		var lang uint32
		var sender, target uint64
		var unknown, length uint32
		for _, field := range []any{&lang, &sender, &unknown, &target, &length} {
			if binary.Read(r, binary.LittleEndian, field) != nil {
				return
			}
		}
		if length == 0 || length > 4096 {
			return
		}
		text := make([]byte, length)
		if _, err := r.Read(text); err != nil {
			return
		}
		lock.Lock()
		collected = append(collected, strings.TrimRight(string(text), "\x00"))
		lock.Unlock()
	})
	lines = func() []string {
		lock.Lock()
		defer lock.Unlock()
		return append([]string(nil), collected...)
	}
	return lines, cancel
}

// A slot whose only item for the class is owned in a lower copy still gets that item's better variants: a Mythic+
// reward at 370 goes to the feet, the slot furthest behind
func TestSmartLoot_OwnedLowerCopyStillUpgrades(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"short", "smartloot"}, Runtime: "short", Category: "modules/smartloot"})
	bot := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "Sloot", Class: e2eharness.ClassRogue, Level: 80})
	verifyAddon(t, bot)
	seedEquipment(t, bot, rubyrogueGear)
	bot.WaitEquippedSlot(t, 50607, slotFeet, 10*time.Second)

	lines, cancel := systemMessages(bot)
	defer cancel()
	e2eharness.MustGM(t, bot.World, ".lootdebug 370")
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		for _, line := range lines() {
			if !strings.Contains(line, "Pick:") {
				continue
			}
			for _, each := range lines() {
				t.Logf("lootdebug: %s", each)
			}
			if !strings.Contains(line, " for Feet,") {
				e2eharness.Assertf(t, "a 370 reward should go to the feet (277, the weakest slot), got: %s", line)
			}
			t.Logf("PASS %s", line)
			return
		}
		time.Sleep(50 * time.Millisecond)
	}
	e2eharness.HarnessFailf(t, ".lootdebug printed no pick; lines: %q", lines())
}
