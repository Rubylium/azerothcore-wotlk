//go:build e2e

package playerbotRotation_test

import (
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

func TestRogue_AssassinationKit(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	bot := setupClass(t, 4, 0)
	for _, id := range []uint32{92310, 92311, 92312, 92313} {
		bot.Learn(t, id)
	}
	benchTarget(t, bot, "boss")
	casts := observe(t, bot, 60*time.Second)
	for _, id := range []uint32{6774, 48672, 92310, 92311, 92312, 92313} {
		if casts[id] == 0 {
			e2eharness.Assertf(t, "missing Assassination spell %d: %v", id, casts)
		}
	}
}

func TestRogue_CombatFinishers(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	bot := setupClass(t, 4, 1)
	benchTarget(t, bot, "boss")
	casts := observe(t, bot, 60*time.Second)
	for _, id := range []uint32{6774, 48672, 48668} {
		if casts[id] == 0 {
			e2eharness.Assertf(t, "missing Combat finisher %d: %v", id, casts)
		}
	}
}
