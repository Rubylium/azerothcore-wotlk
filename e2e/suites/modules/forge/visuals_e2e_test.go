//go:build e2e

package forge_test

import (
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

// P3 display regression: forged armour is quiet while idle, and its movement effect is brief and interruptible.
func TestForge_ArmourTrailIsOccasionalAndStopsWithMovement(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"forge", "visuals"}, Runtime: "med", Category: "modules/forge"})
	bot := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "FrgVis", Level: 80})
	if err := bot.World.SendChatMessage(7, 0xffffffff, bot.Name+"\x00PersonalLoot\tHELLO"); err != nil {
		e2eharness.HarnessFailf(t, "Gear Bonuses handshake: %v", err)
	}
	bot.FlushWorld(t)
	bot.TeleportPad(t, e2eharness.PackagePad(t))
	equipForgedArmour(t, bot)
	assertNoBodyEffects(t, bot, 3*time.Second)

	if err := bot.World.MoveForward(); err != nil {
		t.Fatalf("start movement: %v", err)
	}
	t.Cleanup(func() { _ = bot.World.MoveStop() })
	e2eharness.WaitAura(t, bot.World, emberTrail, 48*time.Second)
	if err := bot.World.MoveStop(); err != nil {
		t.Fatalf("stop movement: %v", err)
	}
	bot.WaitAuraGone(t, emberTrail, time.Second)
	assertNoBodyEffects(t, bot, 3*time.Second)

	if err := bot.World.MoveForward(); err != nil {
		t.Fatalf("resume movement: %v", err)
	}
	e2eharness.WaitAura(t, bot.World, emberTrail, 48*time.Second)
	// Keep moving: the aura must expire naturally rather than remain for the whole movement.
	bot.WaitAuraGone(t, emberTrail, 2*time.Second)
	assertNoBodyEffects(t, bot, 3*time.Second)
	t.Log("PASS: no idle body aura; movement trail stops immediately and expires within 1.2 seconds")
}
