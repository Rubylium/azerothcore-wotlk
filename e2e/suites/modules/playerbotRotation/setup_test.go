//go:build e2e

package playerbotRotation_test

import (
	"database/sql"
	"fmt"
	"os"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	_ "github.com/go-sql-driver/mysql"
)

func setupRogue(t *testing.T) *e2eharness.ScenarioBot {
	t.Helper()
	return setupClass(t, e2eharness.ClassRogue, 2)
}

func setupClass(t *testing.T, classId uint8, specIndex int) *e2eharness.ScenarioBot {
	t.Helper()
	bot := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "Rotest", Class: classId,
		Race: e2eharness.RaceHuman, Level: 80, LearnAllClass: true})
	hello := bot.Ident.CharName + "\x00PersonalLoot\tHELLO"
	_ = bot.World.SendChatMessage(7, 0xFFFFFFFF, hello)
	stop := make(chan struct{})
	t.Cleanup(func() { close(stop) })
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-stop:
				return
			case <-ticker.C:
				_ = bot.World.SendChatMessage(7, 0xFFFFFFFF, hello)
			}
		}
	}()
	worldDb, err := sql.Open("mysql", os.Getenv("E2E_WORLD_DSN"))
	if err != nil {
		e2eharness.Preconditionf(t, "fixture: %v", err)
	}
	t.Cleanup(func() { worldDb.Close() })
	var spec int
	var preset string
	err = worldDb.QueryRow("SELECT TreeId, AoeBuild FROM custom_talent_tree WHERE ClassId=? AND Kind=1 ORDER BY TreeId LIMIT 1 OFFSET ?", classId, specIndex).Scan(&spec, &preset)
	if err != nil {
		e2eharness.Preconditionf(t, "fixture: %v", err)
	}
	picks := map[int]byte{}
	for _, pick := range strings.Split(preset, ",") {
		parts := strings.Split(pick, ":")
		if len(parts) == 2 {
			nodeId, _ := strconv.Atoi(parts[0])
			picks[nodeId] = parts[1][0]
		}
	}
	rows, err := worldDb.Query("SELECT NodeId FROM custom_talent_node WHERE ClassId=? ORDER BY Position", classId)
	if err != nil {
		e2eharness.Preconditionf(t, "fixture: %v", err)
	}
	var build []byte
	for rows.Next() {
		var nodeId int
		if err := rows.Scan(&nodeId); err != nil {
			e2eharness.Preconditionf(t, "fixture: %v", err)
		}
		digit := byte('0')
		if value, ok := picks[nodeId]; ok {
			digit = value
		}
		build = append(build, digit)
	}
	rows.Close()
	_ = bot.World.SendChatMessage(7, 0xFFFFFFFF, bot.Ident.CharName+"\x00TalentTree\tLBUILD\t"+strconv.Itoa(spec)+"\t"+string(build))
	bot.FlushWorld(t)
	if classId == e2eharness.ClassRogue {
		bot.Learn(t, 674)
		bot.Learn(t, 1180)
		for _, entry := range []uint32{48225, 47043, 48227, 48223, 47112, 48226, 47077, 47581, 47945, 47934, 47730, 45609, 47734, 45461, 47953} {
			bot.EquipEntry(t, entry, 1)
			bot.WaitEquipped(t, entry, 5*time.Second)
		}
	}
	if classId == 6 {
		bot.Learn(t, 197)
		bot.EquipEntry(t, 42943, 1)
		bot.WaitEquipped(t, 42943, 5*time.Second)
	}
	bot.GM(t, ".maxskill")
	bot.GM(t, ".cheat god on")
	bot.FlushWorld(t)
	t.Logf("fixture %s, spec %d", bot.Ident.CharName, spec)
	t.Cleanup(func() {
		if bot.Alive() {
			bot.GM(t, ".cheat rotation off")
			bot.GM(t, ".bench clear")
			bot.FlushWorld(t)
		}
	})
	return bot
}

func benchTarget(t *testing.T, bot *e2eharness.ScenarioBot, layout string) uint64 {
	t.Helper()
	bot.GM(t, ".cheat rotation off")
	bot.GM(t, ".bench clear")
	bot.FlushWorld(t)
	seconds := 30
	if layout == "boss" {
		seconds = 60
	}
	previous := map[uint64]bool{}
	for _, unit := range bot.World.GetNearbyUnits(100) {
		previous[unit.GUID] = true
	}
	bot.GM(t, fmt.Sprintf(".bench run %s 10 %d", layout, seconds))
	bot.FlushWorld(t)
	entry := uint32(900201)
	if layout == "boss" {
		entry = 900202
	}
	var target uint64
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) && target == 0 {
		for _, unit := range bot.World.GetNearbyUnits(100) {
			if unit.Entry == entry && !previous[unit.GUID] {
				target = unit.GUID
				break
			}
		}
		if target == 0 {
			time.Sleep(50 * time.Millisecond)
		}
	}
	if target == 0 {
		e2eharness.Preconditionf(t, "bench did not spawn a new target")
	}
	object := bot.World.GetObject(target)
	_, _, _, mapId := bot.Pos()
	bot.Teleport(t, object.PosX+2, object.PosY, object.PosZ, mapId)
	bot.WaitUnit(t, entry, 10*time.Second)
	bot.CombatReady(t)
	bot.Face(t, target)
	_ = bot.World.SetTarget(target)
	bot.FlushWorld(t)
	return target
}
