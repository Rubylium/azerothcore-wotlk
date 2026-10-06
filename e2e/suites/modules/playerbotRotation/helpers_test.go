//go:build e2e

package playerbotRotation_test

import (
	"sync"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
)

func observe(t *testing.T, bot *e2eharness.ScenarioBot, duration time.Duration) map[uint32]int {
	t.Helper()
	var mutex sync.Mutex
	casts := map[uint32]int{}
	cancel := bot.World.AddSpellCastResultHook(func(id uint32, success bool, reason uint8) {
		if success {
			mutex.Lock()
			casts[id]++
			mutex.Unlock()
		}
	})
	bot.GM(t, ".cheat rotation on")
	<-time.After(duration) // Bounded measurement window, not a setup wait.
	bot.GM(t, ".cheat rotation off")
	bot.FlushWorld(t)
	cancel()
	mutex.Lock()
	defer mutex.Unlock()
	snapshot := map[uint32]int{}
	for id, count := range casts {
		snapshot[id] = count
	}
	t.Logf("successful casts: %v", snapshot)
	return snapshot
}

func rangedBoss(t *testing.T, bot *e2eharness.ScenarioBot) uint64 {
	t.Helper()
	target := benchTarget(t, bot, "boss")
	object := bot.World.GetObject(target)
	_, _, _, mapId := bot.Pos()
	bot.Teleport(t, object.PosX+20, object.PosY, object.PosZ, mapId)
	bot.WaitUnit(t, 900202, 10*time.Second)
	bot.Face(t, target)
	_ = bot.World.SetTarget(target)
	bot.FlushWorld(t)
	return target
}
