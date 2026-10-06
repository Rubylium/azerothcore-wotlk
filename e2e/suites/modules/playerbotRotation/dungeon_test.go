//go:build e2e

package playerbotRotation_test

import (
	"sync"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

// Fresh solo instance; exercise real dungeon threat lists rather than bench-seeded threat.
func TestSubtletyRotation_UtgardeKeep(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	bot := setupRogue(t)
	for _, encounter := range []struct {
		name    string
		x, y, z float32
		entry   uint32
		pack    bool
	}{
		{"forge pack", 264.44, -59.66, 24.77, 24078, true},
		{"Prince Keleseth", 193.12, 199.49, 40.82, 23953, false},
	} {
		bot.GM(t, ".cheat rotation off")
		bot.GM(t, ".combatstop")
		bot.Teleport(t, encounter.x, encounter.y, encounter.z, 574)
		target := bot.WaitUnit(t, encounter.entry, 10*time.Second)
		bot.CombatReady(t)
		bot.Face(t, target)
		_ = bot.World.SetTarget(target)
		var mutex sync.Mutex
		casts := map[uint32]int{}
		cancel := bot.World.AddSpellCastResultHook(func(spellId uint32, success bool, reason uint8) {
			if success {
				mutex.Lock()
				casts[spellId]++
				mutex.Unlock()
			}
		})
		bot.GM(t, ".cheat rotation on")
		// Observe actual dungeon casts for a bounded combat window.
		<-time.After(20 * time.Second)
		bot.GM(t, ".cheat rotation off")
		bot.FlushWorld(t)
		cancel()
		mutex.Lock()
		snapshot := make(map[uint32]int, len(casts))
		for id, count := range casts {
			snapshot[id] = count
		}
		mutex.Unlock()
		t.Logf("%s successful casts: %v", encounter.name, snapshot)
		if encounter.pack && snapshot[92321] == 0 {
			e2eharness.Assertf(t, "no Shuriken Storm against dungeon pack: %v", snapshot)
		}
		if !encounter.pack && snapshot[48657]+snapshot[48691] == 0 {
			e2eharness.Assertf(t, "no Subtlety builder against dungeon boss: %v", snapshot)
		}
	}
}
