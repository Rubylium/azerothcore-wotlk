#include "GroundIndicators.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "StringFormat.h"
#include "TemporarySummon.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <list>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

// The FX lab (.agents/docs/systems/fx-lab.md): a plain gray room, 333 yards a side with 40-yard walls, built on the
// unused test map development (451) by localTools/fxLab/buildFxLab.py, to look at spell visuals with nothing else in the
// way. Its commands, for game masters:
//   .fxlab                              to its middle (rings every 5 yards round it, lines every 33.3)
//   .fxlab back                         back where .fxlab was typed from
//   .fxlab dummy [big]                  a target dummy 10 yards ahead (takes no damage, matches the level); big: x3
//   .fxlab clear                        the dummies placed here taken away
//   .fxlab shape <key|spell> [radius] [seconds]
//                                       a ground indicator of localTools/groundIndicators/shapes.json on the ground
//   .fxlab shapes [filter]              their keys
//   .fxlab kit <SpellVisualKit id>      that kit played on the target (or the nearest dummy, or oneself)
namespace
{
using namespace Acore::ChatCommands;

constexpr uint32 MAP_FX_LAB = 451;
constexpr float LabX = 2933.333f;
constexpr float LabY = 800.0f;
constexpr float LabZ = 0.0f;
// The Adaptive AoE Training Dummy (stat_growth_adaptive_training_dummies.sql): takes no damage, matches the level of
// the player nearest to it
constexpr uint32 NPC_FX_LAB_DUMMY = 900100;
constexpr float DummyDistance = 10.0f;
constexpr float BigDummyScale = 3.0f;
constexpr uint32 DummyLifetimeMs = 2 * HOUR * IN_MILLISECONDS;
constexpr float ClearRange = 400.0f;
constexpr float DefaultShapeRadius = 5.0f;
constexpr uint32 DefaultShapeSeconds = 6;
constexpr uint32 MaxShapeSeconds = 120;

struct Shape
{
    std::string key;
    std::string kind;
    uint32 spell = 0;
    bool carried = false;       // an aura on its carrier, at its own size
    bool builtToSize = false;   // its model is built at its size (a line's piece, a curtain, a billboard): unscaled
    bool directional = false;   // a line or a cone: from its origin, forward
};

// Where .fxlab was typed from, per game master, for .fxlab back
std::unordered_map<ObjectGuid, WorldLocation> ReturnPoints;

std::string Lower(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

// shapes.json is read when asked, so a shape just added is there without a restart. The server runs from the
// repository's server/ folder; the source file's own path finds it from anywhere on the machine that built it.
std::vector<Shape> LoadShapes()
{
    std::filesystem::path const relative = std::filesystem::path("localTools") / "groundIndicators" / "shapes.json";
    std::error_code error;
    std::vector<std::filesystem::path> const candidates = {
        std::filesystem::path(__FILE__).parent_path() / ".." / ".." / ".." / relative,
        std::filesystem::current_path(error) / ".." / relative,
        std::filesystem::current_path(error) / relative,
    };

    std::vector<Shape> shapes;
    for (std::filesystem::path const& candidate : candidates)
    {
        std::ifstream file(candidate);
        if (!file)
            continue;

        // One shape a line, as the file is written
        static std::regex const keyPattern(R"re("key"\s*:\s*"([^"]+)")re");
        static std::regex const kindPattern(R"re("kind"\s*:\s*"([^"]+)")re");
        static std::regex const spellPattern(R"re("spell"\s*:\s*(\d+))re");
        static std::regex const carriedPattern(R"re("carried"\s*:\s*true)re");
        static std::regex const scalePattern(R"re("scale"\s*:)re");
        static std::regex const ratioPattern(R"re("ratio"\s*:)re");
        std::string line;
        while (std::getline(file, line))
        {
            std::smatch key, kind, spell;
            if (!std::regex_search(line, key, keyPattern) || !std::regex_search(line, kind, kindPattern) ||
                !std::regex_search(line, spell, spellPattern))
                continue;

            Shape shape;
            shape.key = key[1];
            shape.kind = kind[1];
            shape.spell = uint32(std::stoul(spell[1]));
            shape.carried = std::regex_search(line, carriedPattern);
            shape.builtToSize = std::regex_search(line, scalePattern) || shape.kind == "curtain" ||
                shape.kind == "billboard";
            shape.directional = shape.kind == "rect" || shape.kind == "cone" || shape.kind == "curtain" ||
                (shape.kind == "texture" && std::regex_search(line, ratioPattern));
            shapes.push_back(shape);
        }
        if (!shapes.empty())
            break;
    }
    return shapes;
}

Shape const* FindShape(std::vector<Shape> const& shapes, std::string const& wanted)
{
    std::string const lowered = Lower(wanted);
    bool const numeric = !wanted.empty() && wanted.size() <= 9 &&
        std::ranges::all_of(wanted, [](unsigned char c) { return std::isdigit(c) != 0; });
    uint32 const spell = numeric ? uint32(std::stoul(wanted)) : 0;
    for (Shape const& shape : shapes)
        if ((numeric && shape.spell == spell) || Lower(shape.key) == lowered)
            return &shape;
    return nullptr;
}

// A point distance yards ahead of unit, on the floor
Position Ahead(Unit* unit, float distance)
{
    float const facing = unit->GetOrientation();
    float const x = unit->GetPositionX() + distance * std::cos(facing);
    float const y = unit->GetPositionY() + distance * std::sin(facing);
    float z = unit->GetMapHeight(x, y, unit->GetPositionZ() + 2.0f);
    if (z <= INVALID_HEIGHT)
        z = unit->GetPositionZ();
    return Position(x, y, z, facing);
}

// The dummies this game master placed, around them
std::list<Creature*> OwnDummies(Player* player)
{
    std::list<Creature*> dummies;
    player->GetCreatureListWithEntryInGrid(dummies, NPC_FX_LAB_DUMMY, ClearRange);
    dummies.remove_if([player](Creature* dummy)
    {
        TempSummon* summon = dummy->ToTempSummon();
        return !summon || summon->GetSummonerGUID() != player->GetGUID();
    });
    return dummies;
}

class FxLabCommandScript final : public CommandScript
{
public:
    FxLabCommandScript() : CommandScript("FxLabCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable fxlabTable = {
            { "", HandleGo, SEC_GAMEMASTER, Console::No },
            { "back", HandleBack, SEC_GAMEMASTER, Console::No },
            { "dummy", HandleDummy, SEC_GAMEMASTER, Console::No },
            { "clear", HandleClear, SEC_GAMEMASTER, Console::No },
            { "shape", HandleShape, SEC_GAMEMASTER, Console::No },
            { "shapes", HandleShapes, SEC_GAMEMASTER, Console::No },
            { "kit", HandleKit, SEC_GAMEMASTER, Console::No },
            { "cast", HandleCast, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "fxlab", fxlabTable },
        };
        return commandTable;
    }

    static bool HandleGo(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (player->GetMapId() != MAP_FX_LAB)
            ReturnPoints[player->GetGUID()] = player->GetWorldLocation();
        if (!player->TeleportTo(MAP_FX_LAB, LabX, LabY, LabZ + 0.5f, 0.0f))
        {
            handler->SendErrorMessage("The FX lab (map 451) could not be reached.");
            return false;
        }
        CleanUp(player);
        handler->SendSysMessage("FX lab: rings every 5 yards round the middle, lines every 33.3 yards. "
            ".fxlab dummy [big], .fxlab shape <key> [radius] [seconds], .fxlab kit <id>, .fxlab back.");
        return true;
    }

    // A clean character to look at: whatever buff or debuff the last test left on it (a boss's debuff, a carried
    // look - Gardien-chef Vorhan's seat number over the head) goes; its passives stay
    static void CleanUp(Player* player)
    {
        player->RemoveAppliedAuras([](AuraApplication const* application)
        {
            return !application->GetBase()->IsPassive();
        });
    }

    static bool HandleBack(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        auto const found = ReturnPoints.find(player->GetGUID());
        if (found == ReturnPoints.end())
        {
            handler->SendErrorMessage("No place to go back to: .fxlab was not used since the server started.");
            return false;
        }
        WorldLocation const back = found->second;
        ReturnPoints.erase(found);
        player->TeleportTo(back);
        return true;
    }

    static bool HandleDummy(ChatHandler* handler, Optional<std::string> size)
    {
        Player* player = handler->GetPlayer();
        bool const big = size && Lower(*size) == "big";
        Position spot = Ahead(player, DummyDistance * (big ? 1.5f : 1.0f));
        // Facing the game master
        spot.SetOrientation(Position::NormalizeOrientation(player->GetOrientation() + float(M_PI)));
        TempSummon* dummy = player->SummonCreature(NPC_FX_LAB_DUMMY, spot, TEMPSUMMON_TIMED_DESPAWN, DummyLifetimeMs);
        if (!dummy)
        {
            handler->SendErrorMessage("The dummy could not be placed (creature 900100 missing?).");
            return false;
        }
        if (big)
            dummy->SetObjectScale(BigDummyScale);
        handler->PSendSysMessage("Dummy placed{} (gone in 2 hours, or with .fxlab clear).", big ? ", boss-sized" : "");
        return true;
    }

    static bool HandleClear(ChatHandler* handler)
    {
        std::list<Creature*> dummies = OwnDummies(handler->GetPlayer());
        for (Creature* dummy : dummies)
            dummy->DespawnOrUnsummon();
        CleanUp(handler->GetPlayer());
        handler->PSendSysMessage("{} dummies taken away, and your buffs and debuffs.", dummies.size());
        return true;
    }

    static bool HandleShapes(ChatHandler* handler, Optional<std::string> filter)
    {
        std::vector<Shape> const shapes = LoadShapes();
        if (shapes.empty())
        {
            handler->SendErrorMessage("localTools/groundIndicators/shapes.json was not found.");
            return false;
        }
        std::string const wanted = filter ? Lower(*filter) : std::string();
        std::string line;
        uint32 count = 0;
        for (Shape const& shape : shapes)
        {
            if (!wanted.empty() && Lower(shape.key).find(wanted) == std::string::npos)
                continue;
            line += Acore::StringFormat("{}{} ({}, {})", line.empty() ? "" : ", ", shape.key, shape.kind, shape.spell);
            ++count;
            if (line.size() > 200)
            {
                handler->SendSysMessage(line);
                line.clear();
            }
        }
        if (!line.empty())
            handler->SendSysMessage(line);
        handler->PSendSysMessage("{} shapes.", count);
        return true;
    }

    static bool HandleShape(ChatHandler* handler, std::string what, Optional<float> radius, Optional<uint32> seconds)
    {
        Player* player = handler->GetPlayer();
        std::vector<Shape> const shapes = LoadShapes();
        Shape const* shape = FindShape(shapes, what);
        if (!shape)
        {
            handler->SendErrorMessage(shapes.empty() ? "localTools/groundIndicators/shapes.json was not found." :
                "No such shape: .fxlab shapes [filter] lists them (a key, or its spell id).");
            return false;
        }

        float const size = std::max(0.5f, radius.value_or(DefaultShapeRadius));
        uint32 const durationMs = std::clamp<uint32>(seconds.value_or(DefaultShapeSeconds), 1, MaxShapeSeconds) *
            IN_MILLISECONDS;
        Unit* target = handler->getSelectedUnit();
        if (target == player)
            target = nullptr;

        if (shape->carried)
        {
            Unit* carrier = target ? target : player;
            if (!GroundIndicators::ShowCarriedLook(carrier, shape->spell, durationMs))
            {
                handler->SendErrorMessage("That look could not be put on.");
                return false;
            }
        }
        else if (shape->kind == "billboard")
            GroundIndicators::ShowBillboard(player, target ? target->GetPosition() : Ahead(player, DummyDistance),
                                            shape->spell, durationMs, target);
        else if (shape->directional)
        {
            // From the game master's feet, towards the target or ahead
            Position origin = player->GetPosition();
            float const facing = target ? player->GetAngle(target) : player->GetOrientation();
            GroundIndicators::ShowDecal(player, origin, facing, shape->builtToSize ? 1.0f : size, durationMs,
                                        shape->spell);
        }
        else
        {
            // On the target, or just past its own edge ahead
            Position const center = target ? target->GetPosition() : Ahead(player, size + 3.0f);
            GroundIndicators::ShowDecal(player, center, player->GetOrientation(),
                                        shape->builtToSize ? 1.0f : size, durationMs, shape->spell);
        }
        handler->PSendSysMessage("{} ({}, spell {}){} for {} s.", shape->key, shape->kind, shape->spell,
            shape->builtToSize || shape->carried ? " at its own size" : Acore::StringFormat(", {} yards", size),
            durationMs / IN_MILLISECONDS);
        return true;
    }

    // The unit a look is shown on: the selection, else the nearest of the player's dummies, else the player
    static Unit* LookTarget(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Unit* on = handler->getSelectedUnit();
        if (!on || on == player)
        {
            std::list<Creature*> dummies = OwnDummies(player);
            dummies.sort([player](Creature* a, Creature* b)
            {
                return player->GetDistance(a) < player->GetDistance(b);
            });
            on = dummies.empty() ? static_cast<Unit*>(player) : dummies.front();
        }
        return on;
    }

    // .fxlab cast <spell>: the player casts it (triggered: no cost, no class, no combo point needed) on its selection,
    // else its nearest dummy - a whole spell's look with no target to click (the screenshot runs cannot select one)
    static bool HandleCast(ChatHandler* handler, SpellInfo const* spellInfo)
    {
        if (!spellInfo)
        {
            handler->SendErrorMessage("Usage: .fxlab cast <spell id or link>");
            return false;
        }
        Player* player = handler->GetPlayer();
        Unit* on = LookTarget(handler);
        player->SetFacingToObject(on);
        player->CastSpell(on, spellInfo->Id, TRIGGERED_FULL_MASK);
        handler->PSendSysMessage("Spell {} cast on {}.", spellInfo->Id, on->GetName());
        return true;
    }

    static bool HandleKit(ChatHandler* handler, uint32 kit)
    {
        Unit* on = LookTarget(handler);
        on->SendPlaySpellVisual(kit);
        handler->PSendSysMessage("SpellVisualKit {} played on {}.", kit, on->GetName());
        return true;
    }
};
}

void AddFxLabScripts()
{
    new FxLabCommandScript();
}
