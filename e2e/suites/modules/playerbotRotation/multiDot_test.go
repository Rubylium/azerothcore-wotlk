//go:build e2e

package playerbotRotation_test

import (
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

func TestRotation_MultiDot(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	for _, spec := range []struct {
		name  string
		class uint8
		index int
		dot   uint32
	}{
		{"affliction", 9, 0, 47813}, {"shadow", 5, 2, 48125},
	} {
		t.Run(spec.name, func(t *testing.T) {
			bot := setupClass(t, spec.class, spec.index)
			// Disable automatic spreading: the oracle must exercise off-target casts.
			if spec.name == "shadow" {
				bot.GM(t, ".unlearn 93569")
			} else {
				for _, id := range []string{"27243", "47835", "47836"} {
					bot.GM(t, ".unlearn "+id)
				}
			}
			bot.FlushWorld(t)
			previous := map[uint64]bool{}
			for _, unit := range bot.World.GetNearbyUnits(100) {
				previous[unit.GUID] = true
			}
			target := benchTarget(t, bot, "pack5")
			object := bot.World.GetObject(target)
			_, _, _, mapId := bot.Pos()
			bot.Teleport(t, object.PosX+20, object.PosY, object.PosZ, mapId)
			bot.WaitUnit(t, 900201, 10*time.Second)
			bot.Face(t, target)
			_ = bot.World.SetTarget(target)
			bot.FlushWorld(t)
			bot.GM(t, ".cheat rotation on")
			deadline := time.Now().Add(25 * time.Second)
			seen := map[uint64]bool{}
			for time.Now().Before(deadline) {
				for _, unit := range bot.World.GetNearbyUnits(50) {
					if unit.Entry == 900201 && !previous[unit.GUID] && unit.HasAura(spec.dot) {
						seen[unit.GUID] = true
					}
				}
				if len(seen) >= 3 {
					break
				}
				time.Sleep(100 * time.Millisecond)
			}
			bot.GM(t, ".cheat rotation off")
			bot.FlushWorld(t)
			t.Logf("DoT %d reached targets: %v", spec.dot, seen)
			if len(seen) < 3 {
				e2eharness.Assertf(t, "DoT %d reached only %d pack enemies", spec.dot, len(seen))
			}
			if bot.World.TargetGUID() != target {
				e2eharness.Assertf(t, "rotation changed player selection")
			}
		})
	}
}
