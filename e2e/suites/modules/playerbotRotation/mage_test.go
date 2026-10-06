//go:build e2e

package playerbotRotation_test

import (
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

func TestMage_SingleTargetKit(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"playerbotRotation"}, Runtime: "med", Category: "modules/playerbotRotation"})
	for _, spec := range []struct {
		name     string
		index    int
		required []uint32
	}{
		{"arcane", 0, []uint32{92110, 92112}},
		{"fire", 1, []uint32{92120, 92121, 92122}},
		{"frost", 2, []uint32{92130, 42914}},
	} {
		t.Run(spec.name, func(t *testing.T) {
			bot := setupClass(t, 8, spec.index)
			for _, id := range spec.required {
				if !bot.World.KnowsSpell(id) {
					bot.Learn(t, id)
				}
			}
			if spec.index == 1 {
				for _, id := range []string{"11095", "12872", "12873"} {
					bot.GM(t, ".unlearn "+id)
				}
				bot.FlushWorld(t)
			}
			target := rangedBoss(t, bot)
			if spec.index == 2 {
				// Reproduce a capped custom Icicle resource before enabling rotation.
				_ = bot.World.SetTarget(bot.World.CharGUID())
				bot.GM(t, ".aura 92127")
				for i := 0; i < 4; i++ {
					bot.GM(t, ".aura 92127")
				}
				bot.FlushWorld(t)
				if stacks := bot.World.GetObject(bot.World.CharGUID()).AuraStacks(92127); stacks != 5 {
					e2eharness.Preconditionf(t, "expected 5 Icicles, got %d", stacks)
				}
				_ = bot.World.SetTarget(target)
				bot.FlushWorld(t)
			}
			casts := observe(t, bot, 20*time.Second)
			for _, id := range spec.required {
				if casts[id] == 0 {
					e2eharness.Assertf(t, "missing spell %d: %v", id, casts)
				}
			}
			if spec.index == 1 && (casts[42859] != 0 || casts[11129] != 0) {
				e2eharness.Assertf(t, "untalented Scorch or legacy Combustion cast: %v", casts)
			}
		})
	}
}
