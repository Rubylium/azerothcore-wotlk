//go:build e2e

package playerbotRotation_test

import (
	"sync"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

// Real client-controlled rogue, not a registered bot: reproduces the .cheat rotation AoE failure.
func TestSubtletyRotation_BossAndPack(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	bot := setupRogue(t)
	for _, layout := range []string{"boss", "pack5"} {
		benchTarget(t, bot, layout)
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
		// Fixed observation window measures the cast distribution, not a setup/acknowledgement wait.
		duration := 30 * time.Second
		if layout == "boss" {
			duration = 60 * time.Second
		}
		<-time.After(duration)
		bot.GM(t, ".cheat rotation off")
		bot.FlushWorld(t)
		cancel()
		mutex.Lock()
		snapshot := make(map[uint32]int, len(casts))
		for id, count := range casts {
			snapshot[id] = count
		}
		mutex.Unlock()
		t.Logf("%s casts: %v", layout, snapshot)
		if layout == "pack5" && (snapshot[92321] < 3 || snapshot[92340]+snapshot[92341] < 2) {
			e2eharness.Assertf(t, "pack rotation did not alternate area builders and finishers: %v", snapshot)
		}
		if layout == "boss" && (snapshot[48668] == 0 || snapshot[48668]+snapshot[92341] < 3) {
			e2eharness.Assertf(t, "boss rotation did not alternate builders and damage finishers: %v", snapshot)
		}
		if snapshot[48638] != 0 {
			e2eharness.Assertf(t, "Subtlety fell back to Sinister Strike: %v", snapshot)
		}
		if layout == "boss" && (snapshot[92320] == 0 || snapshot[92322] == 0) {
			e2eharness.Assertf(t, "missing Subtlety cooldowns: %v", snapshot)
		}
	}
}
