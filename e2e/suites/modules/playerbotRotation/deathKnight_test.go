//go:build e2e

package playerbotRotation_test

import (
	"sync"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

func TestFrostDK_BreathPreservesFuel(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	bot := setupClass(t, 6, 1)
	bot.Learn(t, 92644)
	benchTarget(t, bot, "boss")
	var mutex sync.Mutex
	violations := 0
	cancel := bot.World.AddSpellCastResultHook(func(id uint32, success bool, reason uint8) {
		if success && (id == 55268 || id == 92647 || id == 49895 || id == 56815) && bot.World.SelfHasAura(92644) {
			mutex.Lock()
			violations++
			mutex.Unlock()
		}
	})
	casts := observe(t, bot, 45*time.Second)
	cancel()
	mutex.Lock()
	failures := violations
	mutex.Unlock()
	if casts[92644] == 0 {
		e2eharness.Assertf(t, "Breath never started from natural runic generation: %v", casts)
	}
	if failures != 0 {
		e2eharness.Assertf(t, "spent Breath fuel %d times", failures)
	}
}
