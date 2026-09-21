#include "ParagonSystem.h"

#include "Chat.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "CharacterDatabase.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StatGrowthConfig.h"
#include "StatGrowthSystem.h"
#include "StringFormat.h"
#include "WorldSession.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr std::string_view Prefix = "Paragon";

// The board, loaded once at startup. The client ships the same board as a Lua table generated from the same
// script (FrameXML/ParagonBoard.lua); the server keeps its own copy because it is the one that decides whether
// an allocation is legal. Both carry the same signature so a mismatch is reported instead of silently
// mis-drawing the tree.
struct ParagonNode
{
    uint32 id = 0;
    uint8 type = 0;
    uint8 stat = 0;
    uint32 value = 0;
    bool free = false;
};

std::unordered_map<uint32, ParagonNode> Board;
std::unordered_map<uint32, std::vector<uint32>> Adjacency;
uint32 BoardSignature = 0;

// A character's own board. Kept on the player so it dies with the session.
struct ParagonState : public DataMap::Base
{
    uint32 earned = 0;                      // everything ever awarded, including past the cap
    std::unordered_set<uint32> allocated;   // paid-for nodes only; free ones are never stored
    bool applied = false;
};

constexpr char const* StateKey = "ParagonState";

ParagonState* GetState(Player* player)
{
    return player ? player->CustomData.Get<ParagonState>(StateKey) : nullptr;
}

uint32 PointCap()
{
    return statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointCap);
}

uint32 SpentPoints(ParagonState const* state)
{
    return state ? static_cast<uint32>(state->allocated.size()) : 0;
}

// Banked points past the cap do not count until the cap moves, which is the whole point of banking them.
uint32 AvailablePoints(ParagonState const* state)
{
    if (!state)
        return 0;

    uint32 const usable = std::min(state->earned, PointCap());
    uint32 const spent = SpentPoints(state);
    return usable > spent ? usable - spent : 0;
}

void Send(Player* player, std::string const& body)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;

    WorldPacket packet;
    std::string const payload = std::string(Prefix) + "\t" + body;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&packet);
}

bool IsFrench(Player* player)
{
    return player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
}

// Every free node is allocated from the start, so the hub never has to be bought and the first real node is
// always adjacent to something.
bool IsAllocated(ParagonState const* state, uint32 nodeId)
{
    auto const node = Board.find(nodeId);
    if (node == Board.end())
        return false;
    if (node->second.free)
        return true;
    return state && state->allocated.count(nodeId) > 0;
}

// A node may only be taken next to one already held: that adjacency is what makes the tree a tree rather than
// a shopping list, and it is enforced here because the client cannot be trusted with it.
bool IsReachable(ParagonState const* state, uint32 nodeId)
{
    auto const links = Adjacency.find(nodeId);
    if (links == Adjacency.end())
        return false;

    for (uint32 neighbour : links->second)
        if (IsAllocated(state, neighbour))
            return true;
    return false;
}

void ApplyNode(Player* player, ParagonNode const& node, bool apply)
{
    if (!node.value || node.stat >= static_cast<uint8>(PermanentStat::Count))
        return;

    ApplyPermanentStat(player, static_cast<PermanentStat>(node.stat), node.value, apply);
}

void SaveEarned(Player* player, uint32 earned)
{
    CharacterDatabase.Execute("REPLACE INTO character_paragon_points (guid, earned) VALUES ({}, {})",
        player->GetGUID().GetCounter(), earned);
}

// The whole board a character holds, as the frame needs it. Chunked because an addon whisper is capped well
// below what a full allocation would be once the point cap is raised.
void SendState(Player* player, bool open)
{
    ParagonState* state = GetState(player);
    if (!state)
        return;

    Send(player, Acore::StringFormat("{}\t{}\t{}\t{}\t{}\t{}", open ? "OPEN" : "STATE", BoardSignature,
        PointCap(), AvailablePoints(state), state->earned, SpentPoints(state)));

    std::string chunk;
    for (uint32 nodeId : state->allocated)
    {
        if (chunk.size() > 180)
        {
            Send(player, "NODES\t" + chunk);
            chunk.clear();
        }
        if (!chunk.empty())
            chunk += ",";
        chunk += std::to_string(nodeId);
    }
    if (!chunk.empty())
        Send(player, "NODES\t" + chunk);

    Send(player, "DONE");
}

// A bot is handed the stat a real board of this size would be worth rather than a board of its own: it has no
// UI to spend points in, nobody would ever see its tree, and 200 bots each carrying an allocation table would
// be a lot of rows for something invisible. What matters is that a bot party keeps pace with the player.
uint32 AverageNodeValue()
{
    if (Board.empty())
        return 0;

    uint64 total = 0;
    uint32 counted = 0;
    for (auto const& [id, node] : Board)
        if (!node.free && node.value)
        {
            total += node.value;
            ++counted;
        }
    return counted ? static_cast<uint32>(total / counted) : 0;
}

PermanentStat BotStat(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
            return PermanentStat::Intellect;
        case CLASS_ROGUE:
        case CLASS_HUNTER:
            return PermanentStat::Agility;
        case CLASS_DRUID:
        case CLASS_SHAMAN:
        case CLASS_PALADIN:
            return bot->HasSpell(5176) || bot->HasSpell(585) ? PermanentStat::Intellect : PermanentStat::Strength;
        default:
            return PermanentStat::Strength;
    }
}

// What a boss is worth, by what it was killed on. Rolled per player rather than per kill, so nobody is
// competing with their own group for it.
//
// Only a real boss counts - trash would turn a long dungeon into a better farm than a hard one, which is the
// opposite of the point.
float GetParagonDropChance(Player* /*player*/, Creature* killed)
{
    if (!killed->IsDungeonBoss() && !killed->isWorldBoss())
        return 0.0f;

    Map* map = killed->GetMap();
    if (!map || !map->IsDungeon())
        return 0.0f;

    auto const value = [](StatGrowthConfigKey key) { return statGrowthConfig.GetConfigValue<float>(key); };

    if (map->IsRaid())
        return map->IsHeroic() ? value(StatGrowthConfigKey::ParagonRaidHeroicChance)
                               : value(StatGrowthConfigKey::ParagonRaidChance);

    // IsMythic is >= 0: a key is above zero, Mythique 0 is exactly zero, and anything else is -1.
    if (map->IsMythic())
    {
        int32 const level = map->GetMythicLevel();
        if (level > 0)
            return value(StatGrowthConfigKey::ParagonMythicChance)
                + level * value(StatGrowthConfigKey::ParagonMythicChancePerLevel);
        return value(StatGrowthConfigKey::ParagonMythicZeroChance);
    }

    return map->IsHeroic() ? value(StatGrowthConfigKey::ParagonHeroicChance)
                           : value(StatGrowthConfigKey::ParagonNormalChance);
}

struct BotParagon : public DataMap::Base
{
    uint32 applied = 0;                     // stat currently handed out, so it can be taken back
    PermanentStat stat = PermanentStat::Strength;
};

constexpr char const* BotKey = "ParagonBot";
}

void LoadParagonBoard()
{
    Board.clear();
    Adjacency.clear();
    BoardSignature = 0;

    QueryResult nodes = WorldDatabase.Query("SELECT id, type, stat, value, free FROM paragon_node");
    if (!nodes)
    {
        LOG_INFO("server.loading", ">> Paragon board is empty (run localTools/paragon/buildParagonTree.py)");
        return;
    }

    do
    {
        Field* field = nodes->Fetch();
        ParagonNode node;
        node.id = field[0].Get<uint32>();
        node.type = field[1].Get<uint8>();
        node.stat = field[2].Get<uint8>();
        node.value = field[3].Get<uint32>();
        node.free = field[4].Get<uint8>() != 0;
        Board[node.id] = node;

        // Cheap order-independent signature, matched against the client's copy of the board
        BoardSignature += node.id * 31 + node.value * 7 + node.stat;
    } while (nodes->NextRow());

    uint32 links = 0;
    if (QueryResult result = WorldDatabase.Query("SELECT node_a, node_b FROM paragon_node_link"))
        do
        {
            Field* field = result->Fetch();
            uint32 const a = field[0].Get<uint32>();
            uint32 const b = field[1].Get<uint32>();
            if (!Board.count(a) || !Board.count(b))
            {
                LOG_ERROR("sql.sql", "paragon_node_link names a node that does not exist: {} - {}", a, b);
                continue;
            }
            Adjacency[a].push_back(b);
            Adjacency[b].push_back(a);
            BoardSignature += a * 13 + b * 17;
            ++links;
        } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} paragon nodes and {} links (signature {})",
        Board.size(), links, BoardSignature);
}

void LoadParagonForPlayer(Player* player)
{
    if (!player || player->GetSession()->IsBot())
        return;

    ParagonState* state = player->CustomData.GetDefault<ParagonState>(StateKey);
    state->earned = 0;
    state->allocated.clear();
    state->applied = false;

    uint32 const guid = player->GetGUID().GetCounter();
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT earned FROM character_paragon_points WHERE guid = {}", guid))
        state->earned = result->Fetch()[0].Get<uint32>();

    if (QueryResult result = CharacterDatabase.Query("SELECT node FROM character_paragon WHERE guid = {}", guid))
        do
        {
            uint32 const nodeId = result->Fetch()[0].Get<uint32>();
            // A node the board no longer has (the tree was reshaped) is dropped rather than applied: the point
            // stays earned, so it can simply be spent again.
            if (Board.count(nodeId))
                state->allocated.insert(nodeId);
        } while (result->NextRow());
}

void ForgetParagonForPlayer(Player* player)
{
    if (player)
        player->CustomData.Erase(StateKey);
}

void ApplyStoredParagon(Player* player)
{
    ParagonState* state = GetState(player);
    if (!state || state->applied)
        return;

    for (uint32 nodeId : state->allocated)
        if (auto const node = Board.find(nodeId); node != Board.end())
            ApplyNode(player, node->second, true);

    state->applied = true;
}

void ApplyBotParagon(Player* bot)
{
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;

    // The group's real players decide how much: a bot party is meant to keep pace with the person it is
    // playing with, not with the board it cannot see.
    uint32 points = 0;
    uint32 counted = 0;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource();
                member && member->GetSession() && !member->GetSession()->IsBot())
            {
                points += SpentPoints(GetState(member));
                ++counted;
            }

    uint32 const target = counted ? (points / counted) * AverageNodeValue() : 0;
    BotParagon* applied = bot->CustomData.GetDefault<BotParagon>(BotKey);
    PermanentStat const stat = BotStat(bot);
    if (applied->applied == target && applied->stat == stat)
        return;

    if (applied->applied)
        ApplyPermanentStat(bot, applied->stat, applied->applied, false);
    if (target)
        ApplyPermanentStat(bot, stat, target, true);

    applied->applied = target;
    applied->stat = stat;
}

void TryAwardParagonPoint(Player* player, Creature* killed)
{
    if (!player || !killed || !player->GetSession() || player->GetSession()->IsBot())
        return;

    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled))
        return;

    ParagonState* state = GetState(player);
    if (!state)
        return;

    float const chance = GetParagonDropChance(player, killed);
    if (chance <= 0.0f || !roll_chance_f(chance))
        return;

    ++state->earned;
    SaveEarned(player, state->earned);

    ChatHandler chat(player->GetSession());
    uint32 const available = AvailablePoints(state);
    if (available)
        chat.PSendSysMessage(IsFrench(player)
            ? "|cffa335eeUn point de parangon !|r |cff00ff00{} point(s) à dépenser.|r"
            : "|cffa335eeA paragon point!|r |cff00ff00{} point(s) to spend.|r", available);
    else
        // Banked: the cap is full, but the point was not thrown away.
        chat.PSendSysMessage(IsFrench(player)
            ? "|cffa335eeUn point de parangon est mis de côté.|r |cff888888Limite atteinte ({}).|r"
            : "|cffa335eeA paragon point is banked.|r |cff888888Cap reached ({}).|r", PointCap());

    Send(player, Acore::StringFormat("POINT\t{}\t{}", available, state->earned));
}

void SendParagonBoard(Player* player)
{
    if (!player)
        return;

    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled) || Board.empty())
    {
        ChatHandler(player->GetSession()).SendSysMessage(IsFrench(player)
            ? "Le tableau de parangon n'est pas disponible."
            : "The paragon board is not available.");
        return;
    }

    SendState(player, true);
}

void HandleParagonAddonMessage(Player* player, uint32 language, std::string const& message)
{
    if (!player || language != LANG_ADDON || !message.starts_with(Prefix))
        return;

    std::string_view body(message);
    body.remove_prefix(Prefix.size());
    if (body.empty() || body.front() != '\t')
        return;
    body.remove_prefix(1);

    ParagonState* state = GetState(player);
    if (!state)
        return;

    ChatHandler chat(player->GetSession());
    bool const french = IsFrench(player);

    if (body == "OPEN")
    {
        SendParagonBoard(player);
        return;
    }

    if (body == "RESET")
    {
        if (player->IsInCombat())
        {
            Send(player, std::string("ERROR\t") + (french ? "Impossible en combat." : "Not while in combat."));
            return;
        }

        for (uint32 nodeId : state->allocated)
            if (auto const node = Board.find(nodeId); node != Board.end())
                ApplyNode(player, node->second, false);

        state->allocated.clear();
        CharacterDatabase.Execute("DELETE FROM character_paragon WHERE guid = {}",
            player->GetGUID().GetCounter());

        SendState(player, false);
        chat.SendSysMessage(french ? "Votre tableau de parangon a été réinitialisé."
                                   : "Your paragon board has been reset.");
        return;
    }

    constexpr std::string_view allocate = "ALLOC\t";
    if (!body.starts_with(allocate))
        return;

    body.remove_prefix(allocate.size());
    uint32 const nodeId = static_cast<uint32>(std::strtoul(std::string(body).c_str(), nullptr, 10));

    auto const entry = Board.find(nodeId);
    if (entry == Board.end() || entry->second.free)
        return;

    // Each of these is checked here and not in the frame: the frame is a convenience, this is the rule.
    if (state->allocated.count(nodeId))
        return;

    if (!AvailablePoints(state))
    {
        Send(player, std::string("ERROR\t") + (french ? "Aucun point disponible." : "No points available."));
        return;
    }

    if (!IsReachable(state, nodeId))
    {
        Send(player, std::string("ERROR\t") + (french ? "Ce noeud n'est pas accessible."
                                                      : "That node is not connected to your board."));
        return;
    }

    state->allocated.insert(nodeId);
    ApplyNode(player, entry->second, true);
    CharacterDatabase.Execute("REPLACE INTO character_paragon (guid, node) VALUES ({}, {})",
        player->GetGUID().GetCounter(), nodeId);

    // The frame animates from this, so it carries the node that was taken rather than just the new totals.
    Send(player, Acore::StringFormat("GAINED\t{}\t{}\t{}", nodeId, AvailablePoints(state), SpentPoints(state)));

    // Bots track the group's real players, so they move the moment the player does.
    if (Group* group = player->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource();
                member && member->GetSession() && member->GetSession()->IsBot())
                ApplyBotParagon(member);
}

namespace
{
// Talking to the keeper opens the frame. There is no gossip menu worth showing: the board is the interface,
// and a menu in front of it would only be something to click through.
class npc_stat_growth_paragon_keeper : public CreatureScript
{
public:
    npc_stat_growth_paragon_keeper() : CreatureScript("npc_stat_growth_paragon_keeper") { }

    bool OnGossipHello(Player* player, Creature* /*creature*/) override
    {
        CloseGossipMenuFor(player);
        SendParagonBoard(player);
        return true;
    }
};
}

void AddParagonScripts()
{
    new npc_stat_growth_paragon_keeper();
}
