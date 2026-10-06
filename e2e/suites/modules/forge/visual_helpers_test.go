//go:build e2e

package forge_test

import (
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	_ "github.com/go-sql-driver/mysql"
)

const emberTrail = uint32(92400)

func assertNoBodyEffects(t *testing.T, bot *e2eharness.ScenarioBot, window time.Duration) {
	t.Helper()
	ticker := time.NewTicker(50 * time.Millisecond)
	defer ticker.Stop()
	deadline := time.NewTimer(window)
	defer deadline.Stop()
	for {
		for _, spell := range []uint32{emberTrail, 92401, 92402, 92404, 92405} {
			if bot.HasAura(spell) {
				t.Fatalf("unexpected persistent Forge body aura %d", spell)
			}
		}
		select {
		case <-deadline.C:
			return
		case <-ticker.C:
		}
	}
}

func equipForgedArmour(t *testing.T, bot *e2eharness.ScenarioBot) {
	t.Helper()
	db, err := e2eharness.OpenWorldDB()
	if err != nil {
		e2eharness.Preconditionf(t, "world database: %v", err)
	}
	defer db.Close()
	// Three ordinary armour slots at rank 8 meet the first cosmetic threshold (24).
	for _, inventoryType := range []uint32{1, 3, 10} {
		var base uint32
		err := db.QueryRow("SELECT entry FROM item_template WHERE class=4 AND subclass=1 "+
			"AND Quality=4 AND ItemLevel>=200 AND RequiredLevel<=80 AND (AllowableClass & 1)<>0 "+
			"AND (AllowableRace & 1)<>0 AND RequiredSkill=0 AND RandomProperty=0 AND RandomSuffix=0 "+
			"AND map=0 AND area=0 AND (Flags & 16)=0 AND InventoryType=? ORDER BY entry LIMIT 1",
			inventoryType).Scan(&base)
		if err != nil {
			e2eharness.Preconditionf(t, "forgeable armour slot %d: %v", inventoryType, err)
		}
		entry := uint32(136*65536) + base
		bot.EquipEntry(t, entry, 1)
		bot.WaitEquipped(t, entry, 5*time.Second)
	}
}
