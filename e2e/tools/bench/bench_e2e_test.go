//go:build e2e

// Combat bench driver (not a regression test): logs a GM in, brings bench bots and runs `.bench` layouts on GM
// Island, printing each report. Driven by localTools/combatBench/runBench.ps1; skipped unless BENCH_BOTS is set.
// See .agents/docs/systems/combat-bench.md.
//
//	BENCH_BOTS     ";"-separated `.bench bot` arguments, e.g. "mage fire aoe;rogue sub aoe"
//	BENCH_LAYOUTS  ","-separated layouts (default "single,pack5,pack8,pack12")
//	BENCH_KEY      scaling (default "10": M+ +10)
//	BENCH_SECS     seconds per test (default "45")
package bench_test

import (
	"os"
	"regexp"
	"strings"
	"sync"
	"testing"
	"time"

	_ "github.com/go-sql-driver/mysql"

	"github.com/azerothcore/AzerothGhost/client"
	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	"github.com/azerothcore/azerothcore-wotlk/e2e/internal/meta"
)

type chatLog struct {
	mu    sync.Mutex
	lines []string
}

var printable = regexp.MustCompile(`[\x20-\x7e\x80-\xff]{4,}`)

func (c *chatLog) add(data []byte) {
	c.mu.Lock()
	defer c.mu.Unlock()
	for _, m := range printable.FindAll(data, -1) {
		c.lines = append(c.lines, string(m))
	}
}

func (c *chatLog) since(n int) []string {
	c.mu.Lock()
	defer c.mu.Unlock()
	if n > len(c.lines) {
		return nil
	}
	return append([]string(nil), c.lines[n:]...)
}

func (c *chatLog) count() int {
	c.mu.Lock()
	defer c.mu.Unlock()
	return len(c.lines)
}

func waitFor(log *chatLog, start int, want *regexp.Regexp, timeout time.Duration) (string, bool) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		for _, line := range log.since(start) {
			if want.MatchString(line) {
				return line, true
			}
		}
		time.Sleep(200 * time.Millisecond)
	}
	return "", false
}

func envOr(name, fallback string) string {
	if value := os.Getenv(name); value != "" {
		return value
	}
	return fallback
}

var runRe = regexp.MustCompile(`run (\d{6,})`)

func TestTool_CombatBench(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"tool", "long"}, Runtime: "long", Category: "tool"})
	if os.Getenv("BENCH_BOTS") == "" {
		t.Skip("combat bench driver: set BENCH_BOTS (localTools/combatBench/runBench.ps1)")
	}

	bot := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "Bench", Class: e2eharness.ClassWarrior, Level: 80})
	log := &chatLog{}
	cancel := bot.World.AddPacketHook(func(op uint16, data []byte) {
		if op == client.SmsgMessageChat {
			log.add(data)
		}
	})
	defer cancel()

	// The mandatory Gear Bonuses addon handshake (mod-stat-growth PersonalLootSystem.cpp), else the session is
	// kicked after its grace time: an addon whisper to self, resent in case a teleport resets it
	hello := bot.Ident.CharName + "\x00PersonalLoot\tHELLO"
	_ = bot.World.SendChatMessage(7, 0xFFFFFFFF, hello)
	stop := make(chan struct{})
	defer close(stop)
	go func() {
		for {
			select {
			case <-stop:
				return
			case <-time.After(5 * time.Second):
				_ = bot.World.SendChatMessage(7, 0xFFFFFFFF, hello)
			}
		}
	}()

	before := bot.World.TeleportSeq()
	bot.GM(t, ".bench tele")
	_ = bot.World.WaitForTeleportAfter(before, 20*time.Second)
	bot.WaitInWorld(t, 20*time.Second)
	bot.FlushWorld(t)

	setup := log.count()
	for _, args := range strings.Split(os.Getenv("BENCH_BOTS"), ";") {
		if args = strings.TrimSpace(args); args != "" {
			bot.GM(t, ".bench bot "+args)
			time.Sleep(4 * time.Second)
		}
	}
	time.Sleep(10 * time.Second)
	// A group damage pulse for healer tests (`.bench pulse`): BENCH_PULSE, a share of the key's reference health
	if pulse := os.Getenv("BENCH_PULSE"); pulse != "" {
		bot.GM(t, ".bench pulse "+pulse)
		time.Sleep(time.Second)
	}
	bot.GM(t, ".bench list")
	time.Sleep(2 * time.Second)
	for _, line := range log.since(setup) {
		t.Logf("[setup] %s", line)
	}

	key := envOr("BENCH_KEY", "10")
	seconds := envOr("BENCH_SECS", "45")
	for _, layout := range strings.Split(envOr("BENCH_LAYOUTS", "single,pack5,pack8,pack12"), ",") {
		start := log.count()
		bot.GM(t, ".bench run "+layout+" "+key+" "+seconds)
		line, ok := waitFor(log, start, runRe, 3*time.Minute)
		for _, l := range log.since(start) {
			t.Logf("[%s] %s", layout, l)
		}
		if ok {
			t.Logf("RESULT %s %s", layout, runRe.FindStringSubmatch(line)[1])
		} else {
			t.Logf("RESULT %s none", layout)
		}
		bot.GM(t, ".bench reset")
		time.Sleep(3 * time.Second)
	}
	bot.GM(t, ".bench clear")
	time.Sleep(2 * time.Second)
}
