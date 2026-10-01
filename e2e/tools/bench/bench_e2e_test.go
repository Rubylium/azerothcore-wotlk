//go:build e2e

// Combat bench driver (not a regression test): logs a GM in, brings bench bots and runs `.bench` layouts on GM
// Island, printing each report. Driven by localTools/combatBench/runBench.ps1 (one run) and bench.ps1 (a session that
// stays logged in for a tuning loop); skipped unless BENCH_BOTS or BENCH_SESSION_DIR is set.
// See .agents/docs/systems/combat-bench.md.
//
//	BENCH_BOTS     ";"-separated `.bench bot` arguments, e.g. "mage fire aoe;rogue sub aoe"
//	BENCH_LAYOUTS  ","-separated layouts (default "single,pack5,pack8,pack12")
//	BENCH_KEY      scaling (default "10": M+ +10)
//	BENCH_SECS     seconds per test (default "45")
//	BENCH_PULSE    a group damage pulse for healer tests (% of the key's reference health)
package bench_test

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
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

	bot, log := startBench(t)
	report := func(format string, args ...any) { t.Logf(format, args...) }
	spawnBots(t, bot, log, os.Getenv("BENCH_BOTS"), os.Getenv("BENCH_PULSE"), report)
	runLayouts(t, bot, log, envOr("BENCH_LAYOUTS", "single,pack5,pack8,pack12"), envOr("BENCH_KEY", "10"),
		envOr("BENCH_SECS", "45"), report)
	bot.GM(t, ".bench clear")
	time.Sleep(2 * time.Second)
}

// A GM on the bench: logged in, its addon handshake kept up, at GM Island. The chat it receives goes to the log.
func startBench(t *testing.T) (*e2eharness.ScenarioBot, *chatLog) {
	bot := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "Bench", Class: e2eharness.ClassWarrior, Level: 80})
	log := &chatLog{}
	cancel := bot.World.AddPacketHook(func(op uint16, data []byte) {
		if op == client.SmsgMessageChat {
			log.add(data)
		}
	})
	t.Cleanup(cancel)

	// The mandatory Gear Bonuses addon handshake (mod-stat-growth PersonalLootSystem.cpp), else the session is
	// kicked after its grace time: an addon whisper to self, resent in case a teleport resets it
	hello := bot.Ident.CharName + "\x00PersonalLoot\tHELLO"
	_ = bot.World.SendChatMessage(7, 0xFFFFFFFF, hello)
	stop := make(chan struct{})
	t.Cleanup(func() { close(stop) })
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
	return bot, log
}

// Brings the bench bots (";"-separated `.bench bot` arguments) and an optional group damage pulse (`.bench pulse`),
// then reports `.bench list`
func spawnBots(t *testing.T, bot *e2eharness.ScenarioBot, log *chatLog, bots, pulse string,
	report func(string, ...any)) {
	setup := log.count()
	for _, args := range strings.Split(bots, ";") {
		if args = strings.TrimSpace(args); args != "" {
			bot.GM(t, ".bench bot "+args)
			time.Sleep(4 * time.Second)
		}
	}
	time.Sleep(10 * time.Second)
	if pulse != "" {
		bot.GM(t, ".bench pulse "+pulse)
		time.Sleep(time.Second)
	}
	bot.GM(t, ".bench list")
	time.Sleep(2 * time.Second)
	for _, line := range log.since(setup) {
		report("[setup] %s", line)
	}
}

// Each layout in turn (","-separated): its report, then RESULT <layout> <run id> for the per-spell tables
func runLayouts(t *testing.T, bot *e2eharness.ScenarioBot, log *chatLog, layouts, key, seconds string,
	report func(string, ...any)) {
	for _, layout := range strings.Split(layouts, ",") {
		if layout = strings.TrimSpace(layout); layout == "" {
			continue
		}
		start := log.count()
		bot.GM(t, ".bench run "+layout+" "+key+" "+seconds)
		line, ok := waitFor(log, start, runRe, 3*time.Minute)
		for _, l := range log.since(start) {
			report("[%s] %s", layout, l)
		}
		if ok {
			report("RESULT %s %s", layout, runRe.FindStringSubmatch(line)[1])
		} else {
			report("RESULT %s none", layout)
		}
		bot.GM(t, ".bench reset")
		time.Sleep(3 * time.Second)
	}
}

// A bench session (localTools/combatBench/bench.ps1): logged in once, it takes requests until told to quit, so a
// tuning loop costs only its runs - no build of this driver, no login, no bots brought again. BENCH_SESSION_DIR is
// its folder: a request is a file req-<n>.txt of lines, its answer res-<n>.txt (written once done), and the file
// ready says the session is up. A line is one of
//
//	.<command>                        a chat command (.tune set ..., .bench pulse 10) and its replies
//	bots <a;b;...> [pulse=<percent>]  the bench bots replaced by these (`.bench bot` arguments)
//	run <layouts> [key] [seconds]     the layouts in turn, their reports and RESULT lines
//	wait <seconds>
//	quit                              ends the session
func TestTool_CombatBenchSession(t *testing.T) {
	meta.Begin(t, meta.TestMeta{Tags: []string{"tool", "long"}, Runtime: "long", Category: "tool"})
	dir := os.Getenv("BENCH_SESSION_DIR")
	if dir == "" {
		t.Skip("combat bench session: set BENCH_SESSION_DIR (localTools/combatBench/bench.ps1)")
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("session folder: %v", err)
	}

	bot, log := startBench(t)
	ready := filepath.Join(dir, "ready")
	if err := os.WriteFile(ready, []byte(bot.Ident.CharName), 0o644); err != nil {
		t.Fatalf("ready: %v", err)
	}
	defer os.Remove(ready)

	for {
		requests, _ := filepath.Glob(filepath.Join(dir, "req-*.txt"))
		sort.Strings(requests)
		if len(requests) == 0 {
			time.Sleep(300 * time.Millisecond)
			continue
		}
		request := requests[0]
		content, err := os.ReadFile(request)
		if err != nil {
			time.Sleep(300 * time.Millisecond)
			continue
		}
		_ = os.Remove(request)

		var out strings.Builder
		report := func(format string, args ...any) {
			line := fmt.Sprintf(format, args...)
			out.WriteString(line + "\n")
			t.Log(line)
		}
		quit := handleRequest(t, bot, log, string(content), report)

		name := strings.TrimSuffix(strings.TrimPrefix(filepath.Base(request), "req-"), ".txt")
		answer := filepath.Join(dir, "res-"+name+".txt")
		_ = os.WriteFile(answer+".tmp", []byte(out.String()), 0o644)
		_ = os.Rename(answer+".tmp", answer)
		if quit {
			bot.GM(t, ".bench clear")
			time.Sleep(2 * time.Second)
			return
		}
	}
}

// One session request, line by line. Returns whether it asked to quit.
func handleRequest(t *testing.T, bot *e2eharness.ScenarioBot, log *chatLog, content string,
	report func(string, ...any)) bool {
	quit := false
	// A request written by Windows PowerShell 5 may open with a byte order mark
	content = strings.TrimPrefix(strings.ReplaceAll(content, "\r", ""), "\ufeff")
	for _, line := range strings.Split(content, "\n") {
		if line = strings.TrimSpace(line); line == "" {
			continue
		}
		fields := strings.Fields(line)
		switch {
		case strings.HasPrefix(line, "."):
			start := log.count()
			bot.GM(t, line)
			time.Sleep(1500 * time.Millisecond)
			for _, reply := range log.since(start) {
				report("[%s] %s", fields[0], reply)
			}
		case fields[0] == "bots":
			bot.GM(t, ".bench dismiss all")
			time.Sleep(3 * time.Second)
			arguments := strings.TrimSpace(strings.TrimPrefix(line, "bots"))
			pulse := ""
			if at := strings.LastIndex(arguments, " pulse="); at >= 0 {
				pulse = strings.TrimSpace(arguments[at+len(" pulse="):])
				arguments = arguments[:at]
			}
			spawnBots(t, bot, log, arguments, pulse, report)
		case fields[0] == "run" && len(fields) >= 2:
			key, seconds := "10", "45"
			if len(fields) >= 3 {
				key = fields[2]
			}
			if len(fields) >= 4 {
				seconds = fields[3]
			}
			runLayouts(t, bot, log, fields[1], key, seconds, report)
		case fields[0] == "wait" && len(fields) == 2:
			if wait, err := time.ParseDuration(fields[1] + "s"); err == nil {
				time.Sleep(wait)
			}
		case fields[0] == "quit":
			quit = true
		default:
			report("[session] unknown line: %s", line)
		}
	}
	return quit
}
