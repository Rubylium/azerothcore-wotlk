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
#include "GameTime.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StatGrowthConfig.h"
#include "StatGrowthSystem.h"
#include "StringFormat.h"
#include "WorldSession.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
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
// What a node does. Most of the board is still stats - a tree of nothing but procs would be noise - but the
// nodes worth walking to are the ones that change how a fight goes rather than how big a number is.
enum class ParagonEffect : uint8
{
    Stat = 0,           // `value` of `stat`
    Armor,              // flat armour
    ArmorPct,           // armour, as a percentage of what the character already has
    GuardOnHit,         // taking a hit: `chance` to gain `value`% armour for `duration`
    RetaliateOnHit,     // taking a hit: `chance` to deal `value`% of the hit back to the attacker
    LastStand,          // dropping below `value2`% health: take `value`% less damage for `duration`, on cooldown
    FuryOnHit,          // dealing damage: `chance` to deal `value`% more for `duration`
    SurgeOnKill,        // killing something: `value` attack and spell power for `duration`
    Count
};

struct ParagonNode
{
    uint32 id = 0;
    uint8 type = 0;
    uint8 effect = 0;
    uint8 stat = 0;
    uint32 value = 0;
    uint32 value2 = 0;
    float chance = 0.0f;
    uint32 duration = 0;        // milliseconds
    uint32 cooldown = 0;        // milliseconds
    bool free = false;
};

// A proc a character currently owns, lifted out of the board so a damage event does not have to walk every
// allocated node. Rebuilt whenever the allocation changes.
struct ParagonProc
{
    ParagonEffect effect = ParagonEffect::Stat;
    uint32 value = 0;
    uint32 value2 = 0;
    float chance = 0.0f;
    uint32 duration = 0;
    uint32 cooldown = 0;
    uint32 readyAt = 0;         // ms, against World::GetGameTimeMS
};

// A proc that has fired and is still running. `applied` is what was actually handed out, so taking it back is
// exact rather than a second calculation that might not agree with the first.
struct ParagonBuff
{
    ParagonEffect effect = ParagonEffect::Stat;
    uint32 expiresAt = 0;
    int32 applied = 0;
};

std::unordered_map<uint32, ParagonNode> Board;
std::unordered_map<uint32, std::vector<uint32>> Adjacency;
uint32 BoardSignature = 0;

// A character's own board. Kept on the player so it dies with the session.
struct ParagonState : public DataMap::Base
{
    uint32 earned = 0;                      // everything ever awarded, including past the cap
    uint32 prestige = 0;                    // how many times this character has reset; each one raises the cap
    std::unordered_set<uint32> allocated;   // paid-for nodes only; free ones are never stored
    bool applied = false;
    std::vector<ParagonProc> procs;         // from the allocated nodes, rebuilt when they change
    std::vector<ParagonBuff> buffs;         // currently running
};

constexpr char const* StateKey = "ParagonState";

ParagonState* GetState(Player* player)
{
    return player ? player->CustomData.Get<ParagonState>(StateKey) : nullptr;
}

uint32 PointCap(ParagonState const* state)
{
    uint32 const base = statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointCap);
    uint32 const per = statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointsPerPrestige);
    uint32 const prestige = state ? state->prestige : 0;
    if (per == 0)
        return base;
    if (prestige > (std::numeric_limits<uint32>::max() - base) / per)
        return std::numeric_limits<uint32>::max();
    return base + prestige * per;
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

    uint32 const usable = std::min(state->earned, PointCap(state));
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

// Armour as a percentage is handed out as the flat amount it was worth when it was granted, so removing it
// takes back exactly what was given. Recomputing the percentage on removal would not, because the armour it
// is a percentage of has moved in the meantime.
int32 FlatArmorFor(Player* player, uint32 percent)
{
    return static_cast<int32>(player->GetArmor() * percent / 100.0f);
}

void ApplyArmor(Player* player, int32 amount, bool apply)
{
    if (!amount)
        return;

    player->HandleStatFlatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, static_cast<float>(amount), apply);
}

void ApplyNode(Player* player, ParagonNode const& node, bool apply)
{
    switch (static_cast<ParagonEffect>(node.effect))
    {
        case ParagonEffect::Stat:
            if (node.value && node.stat < static_cast<uint8>(PermanentStat::Count))
                ApplyPermanentStat(player, static_cast<PermanentStat>(node.stat), node.value, apply);
            return;
        case ParagonEffect::Armor:
            ApplyArmor(player, static_cast<int32>(node.value), apply);
            return;
        case ParagonEffect::ArmorPct:
            // Percentage armour from a node is permanent, so it is recomputed from base armour each login
            // rather than stored; taking it off uses the same figure because nothing else has changed yet.
            ApplyArmor(player, FlatArmorFor(player, node.value), apply);
            return;
        default:
            // Everything else is a proc: nothing to apply until it fires.
            return;
    }
}

// The procs from whatever is currently allocated. Walking the whole allocation on every damage event would
// be wasteful, and damage events are the hottest path this module has.
void RebuildProcs(ParagonState* state)
{
    state->procs.clear();
    if (!state)
        return;

    for (uint32 nodeId : state->allocated)
    {
        auto const entry = Board.find(nodeId);
        if (entry == Board.end())
            continue;

        ParagonEffect const effect = static_cast<ParagonEffect>(entry->second.effect);
        if (effect == ParagonEffect::Stat || effect == ParagonEffect::Armor
            || effect == ParagonEffect::ArmorPct)
            continue;

        ParagonProc proc;
        proc.effect = effect;
        proc.value = entry->second.value;
        proc.value2 = entry->second.value2;
        proc.chance = entry->second.chance;
        proc.duration = entry->second.duration;
        proc.cooldown = entry->second.cooldown;
        state->procs.push_back(proc);
    }
}

bool HasBuff(ParagonState const* state, ParagonEffect effect)
{
    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == effect)
            return true;
    return false;
}

void StartBuff(Player* player, ParagonState* state, ParagonProc const& proc, std::string_view announce)
{
    ParagonBuff buff;
    buff.effect = proc.effect;
    buff.expiresAt = GameTime::GetGameTimeMS().count() + proc.duration;

    if (proc.effect == ParagonEffect::GuardOnHit)
    {
        buff.applied = FlatArmorFor(player, proc.value);
        ApplyArmor(player, buff.applied, true);
    }

    state->buffs.push_back(buff);

    if (!announce.empty())
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335ee%s|r", std::string(announce).c_str());
}

void ExpireBuffs(Player* player, ParagonState* state)
{
    if (state->buffs.empty())
        return;

    uint32 const now = GameTime::GetGameTimeMS().count();
    for (std::size_t index = state->buffs.size(); index > 0; --index)
    {
        ParagonBuff& buff = state->buffs[index - 1];
        if (buff.expiresAt > now)
            continue;

        if (buff.effect == ParagonEffect::GuardOnHit)
            ApplyArmor(player, buff.applied, false);

        state->buffs.erase(state->buffs.begin() + (index - 1));
    }
}

void ClearBuffs(Player* player, ParagonState* state)
{
    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == ParagonEffect::GuardOnHit)
            ApplyArmor(player, buff.applied, false);
    state->buffs.clear();
}

void SaveEarned(Player* player, ParagonState const* state)
{
    CharacterDatabase.Execute(
        "REPLACE INTO character_paragon_points (guid, earned, prestige) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), state->earned, state->prestige);
}

// The whole board a character holds, as the frame needs it. Chunked because an addon whisper is capped well
// below what a full allocation would be once the point cap is raised.
void SendState(Player* player, bool open)
{
    ParagonState* state = GetState(player);
    if (!state)
        return;

    Send(player, Acore::StringFormat("{}\t{}\t{}\t{}\t{}\t{}", open ? "OPEN" : "STATE", BoardSignature,
        PointCap(state), AvailablePoints(state), state->earned, SpentPoints(state)));

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
        // A key pays its point for being finished (MythicDungeonSystem.cpp), guaranteed, so its bosses do not
        // roll for one as well. Rolling here too would mean a run sometimes paid double and sometimes not at
        // all, when the whole point is that a key is worth a known amount.
        if (map->GetMythicLevel() > 0)
            return 0.0f;
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

    QueryResult nodes = WorldDatabase.Query(
        "SELECT id, type, effect, stat, value, value2, chance, duration, cooldown, free FROM paragon_node");
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
        node.effect = field[2].Get<uint8>();
        node.stat = field[3].Get<uint8>();
        node.value = field[4].Get<uint32>();
        node.value2 = field[5].Get<uint32>();
        node.chance = field[6].Get<float>();
        node.duration = field[7].Get<uint32>();
        node.cooldown = field[8].Get<uint32>();
        node.free = field[9].Get<uint8>() != 0;
        if (node.effect >= static_cast<uint8>(ParagonEffect::Count))
        {
            LOG_ERROR("sql.sql", "paragon_node {} has effect {}, which does not exist", node.id, node.effect);
            node.effect = static_cast<uint8>(ParagonEffect::Stat);
        }
        Board[node.id] = node;

        // Cheap order-independent signature, matched against the client's copy of the board
        BoardSignature += node.id * 31 + node.value * 7 + node.stat + node.effect * 3;
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
    state->prestige = 0;
    state->allocated.clear();
    state->applied = false;

    uint32 const guid = player->GetGUID().GetCounter();
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT earned, prestige FROM character_paragon_points WHERE guid = {}", guid))
    {
        Field* field = result->Fetch();
        state->earned = field[0].Get<uint32>();
        state->prestige = field[1].Get<uint32>();
    }

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
    if (!player)
        return;

    // Take the running procs off first: the modifiers they applied live on the character, not in the state
    // that is about to be thrown away.
    if (ParagonState* state = GetState(player))
        ClearBuffs(player, state);
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

    RebuildProcs(state);
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

void AwardParagonPoints(Player* player, uint32 count, std::string_view reason)
{
    if (!player || !count || !player->GetSession() || player->GetSession()->IsBot())
        return;

    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled))
        return;

    ParagonState* state = GetState(player);
    if (!state)
        return;

    state->earned += count;
    SaveEarned(player, state);

    ChatHandler chat(player->GetSession());
    uint32 const available = AvailablePoints(state);
    bool const french = IsFrench(player);
    if (available)
        chat.PSendSysMessage(french
            ? "|cffa335eeParangon : +{} point ({}).|r |cff00ff00{} à dépenser.|r"
            : "|cffa335eeParagon: +{} point ({}).|r |cff00ff00{} to spend.|r",
            count, reason, available);
    else
        chat.PSendSysMessage(french
            ? "|cffa335eeParangon : +{} point ({}), mis de côté.|r |cff888888Limite atteinte ({}).|r"
            : "|cffa335eeParagon: +{} point ({}), banked.|r |cff888888Cap reached ({}).|r",
            count, reason, PointCap(state));

    Send(player, Acore::StringFormat("POINT\t{}\t{}", available, state->earned));
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
    SaveEarned(player, state);

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
            : "|cffa335eeA paragon point is banked.|r |cff888888Cap reached ({}).|r", PointCap(state));

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
        ClearBuffs(player, state);
        RebuildProcs(state);
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
    RebuildProcs(state);
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


// ---------------------------------------------------------------------------------------------------------
// Procs
//
// These sit on the damage path, so they do as little as possible: a character with no procs allocated leaves
// each of them after two pointer checks and a vector that is empty.
// ---------------------------------------------------------------------------------------------------------

void OnParagonDamageTaken(Unit* victim, Unit* attacker, uint32& damage)
{
    Player* player = victim ? victim->ToPlayer() : nullptr;
    if (!player || !damage || !attacker || attacker == victim)
        return;

    ParagonState* state = GetState(player);
    if (!state || state->procs.empty())
        return;

    uint32 const now = GameTime::GetGameTimeMS().count();
    bool const french = IsFrench(player);

    // Damage reduction is applied before anything rolls, so a proc that fires on this hit does not also
    // soften the hit that set it off.
    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == ParagonEffect::LastStand && buff.applied > 0)
            damage = damage * (100 - std::min<int32>(buff.applied, 90)) / 100;

    for (ParagonProc& proc : state->procs)
    {
        switch (proc.effect)
        {
            case ParagonEffect::GuardOnHit:
                // Refreshing rather than stacking: a tank is hit constantly, and stacking would mean the
                // armour never settles anywhere a healer could read.
                if (!HasBuff(state, ParagonEffect::GuardOnHit) && roll_chance_f(proc.chance))
                    StartBuff(player, state, proc, french ? "Carapace : armure renforcée." : "");
                break;

            case ParagonEffect::RetaliateOnHit:
                if (roll_chance_f(proc.chance) && attacker->IsAlive())
                {
                    uint32 const back = std::max<uint32>(1, damage * proc.value / 100);
                    Unit::DealDamage(player, attacker, back, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL,
                        nullptr, false);
                }
                break;

            case ParagonEffect::LastStand:
            {
                // Health is checked after the hit lands, which is the moment that matters: the point is to
                // survive what comes next, not what just happened.
                uint32 const remaining = player->GetHealth() > damage ? player->GetHealth() - damage : 0;
                uint32 const threshold = player->GetMaxHealth() * proc.value2 / 100;
                if (remaining > threshold || now < proc.readyAt
                    || HasBuff(state, ParagonEffect::LastStand))
                    break;

                proc.readyAt = now + proc.cooldown;
                ParagonBuff buff;
                buff.effect = ParagonEffect::LastStand;
                buff.expiresAt = now + proc.duration;
                buff.applied = static_cast<int32>(proc.value);
                state->buffs.push_back(buff);
                ChatHandler(player->GetSession()).PSendSysMessage(french
                    ? "|cffa335eeDernier rempart !|r" : "|cffa335eeLast Stand!|r");
                break;
            }

            default:
                break;
        }
    }
}

void OnParagonDamageDealt(Unit* attacker, Unit* victim, uint32& damage)
{
    Player* player = attacker ? attacker->ToPlayer() : nullptr;
    if (!player || !damage || !victim || attacker == victim)
        return;

    ParagonState* state = GetState(player);
    if (!state || state->procs.empty())
        return;

    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == ParagonEffect::FuryOnHit && buff.applied > 0)
            damage = damage * (100 + buff.applied) / 100;

    for (ParagonProc& proc : state->procs)
    {
        if (proc.effect != ParagonEffect::FuryOnHit)
            continue;
        if (HasBuff(state, ParagonEffect::FuryOnHit) || !roll_chance_f(proc.chance))
            continue;

        ParagonBuff buff;
        buff.effect = ParagonEffect::FuryOnHit;
        buff.expiresAt = GameTime::GetGameTimeMS().count() + proc.duration;
        buff.applied = static_cast<int32>(proc.value);
        state->buffs.push_back(buff);
    }
}

void OnParagonKill(Player* player, Unit* killed)
{
    if (!player || !killed || killed->GetTypeId() != TYPEID_UNIT)
        return;

    ParagonState* state = GetState(player);
    if (!state || state->procs.empty())
        return;

    for (ParagonProc const& proc : state->procs)
    {
        if (proc.effect != ParagonEffect::SurgeOnKill)
            continue;

        // Refreshed rather than stacked, for the same reason as the armour: a pull of trash would otherwise
        // end with a number nobody planned for.
        for (std::size_t index = state->buffs.size(); index > 0; --index)
            if (state->buffs[index - 1].effect == ParagonEffect::SurgeOnKill)
            {
                ApplyPermanentStat(player, PermanentStat::AttackPower,
                    static_cast<uint32>(state->buffs[index - 1].applied), false);
                ApplyPermanentStat(player, PermanentStat::SpellPower,
                    static_cast<uint32>(state->buffs[index - 1].applied), false);
                state->buffs.erase(state->buffs.begin() + (index - 1));
            }

        ParagonBuff buff;
        buff.effect = ParagonEffect::SurgeOnKill;
        buff.expiresAt = GameTime::GetGameTimeMS().count() + proc.duration;
        buff.applied = static_cast<int32>(proc.value);
        ApplyPermanentStat(player, PermanentStat::AttackPower, proc.value, true);
        ApplyPermanentStat(player, PermanentStat::SpellPower, proc.value, true);
        state->buffs.push_back(buff);
    }
}

void UpdateParagonBuffs(Player* player)
{
    ParagonState* state = GetState(player);
    if (!state || state->buffs.empty())
        return;

    uint32 const now = GameTime::GetGameTimeMS().count();
    for (std::size_t index = state->buffs.size(); index > 0; --index)
    {
        ParagonBuff& buff = state->buffs[index - 1];
        if (buff.expiresAt > now)
            continue;

        if (buff.effect == ParagonEffect::GuardOnHit)
            ApplyArmor(player, buff.applied, false);
        else if (buff.effect == ParagonEffect::SurgeOnKill)
        {
            ApplyPermanentStat(player, PermanentStat::AttackPower,
                static_cast<uint32>(buff.applied), false);
            ApplyPermanentStat(player, PermanentStat::SpellPower,
                static_cast<uint32>(buff.applied), false);
        }

        state->buffs.erase(state->buffs.begin() + (index - 1));
    }
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

// The character select screen shows each character's Prestige and Paragon level (GlueXML/EvolutionsRoster.lua), but
// the character list the client receives has no field for them. They ride in its guild id, which the client does
// not use there: the top byte is CharacterListMarker, the next one the Prestige and the low 16 bits the Paragon
// level (points spent). The client extension DLL (awesome_wotlk, CharacterEvolution.cpp) reads them back for the
// glue screen.
constexpr uint32 CharacterListMarker = 0xE7;

class ParagonCharacterListScript : public PlayerScript
{
public:
    ParagonCharacterListScript() : PlayerScript("ParagonCharacterListScript", { PLAYERHOOK_ON_ENUM_GUILD_ID }) { }

    void OnPlayerEnumGuildId(ObjectGuid guid, uint32& guildId) override
    {
        uint32 const counter = guid.GetCounter();
        uint32 prestige = 0;
        if (QueryResult result = CharacterDatabase.Query(
                "SELECT prestige FROM character_paragon_points WHERE guid = {}", counter))
            prestige = result->Fetch()[0].Get<uint32>();

        // Counted as the character will have them once in game: nodes the board no longer has are not
        uint32 level = 0;
        if (QueryResult result = CharacterDatabase.Query("SELECT node FROM character_paragon WHERE guid = {}", counter))
            do
            {
                if (Board.count(result->Fetch()[0].Get<uint32>()))
                    ++level;
            } while (result->NextRow());

        guildId = CharacterListMarker << 24 | std::min<uint32>(prestige, 0xFF) << 16 | std::min<uint32>(level, 0xFFFF);
    }
};
}

void SuspendParagon(Player* player)
{
    ParagonState* state = GetState(player);
    if (!state || !state->applied)
        return;

    ClearBuffs(player, state);
    for (uint32 nodeId : state->allocated)
        if (auto const node = Board.find(nodeId); node != Board.end())
            ApplyNode(player, node->second, false);
    state->applied = false;
}

void RestoreParagon(Player* player)
{
    ApplyStoredParagon(player);
}

static ParagonState* StateOf(Player* player)
{
    if (!player)
        return nullptr;
    if (!GetState(player))
        LoadParagonForPlayer(player);
    return GetState(player);
}

uint32 GetParagonPrestige(Player* player)
{
    ParagonState const* state = StateOf(player);
    return state ? state->prestige : 0;
}

uint32 GetParagonEarned(Player* player)
{
    ParagonState const* state = StateOf(player);
    return state ? state->earned : 0;
}

uint32 GetParagonSpent(Player* player)
{
    return SpentPoints(StateOf(player));
}

uint32 GetParagonPointCap(Player* player)
{
    return PointCap(StateOf(player));
}

void SetParagonPrestige(Player* player, uint32 prestige)
{
    if (ParagonState* state = StateOf(player))
        state->prestige = prestige;
}

void SaveParagonPoints(Player* player, CharacterDatabaseTransaction trans)
{
    ParagonState const* state = GetState(player);
    if (!player || !state)
        return;

    CharacterDatabase.ExecuteOrAppend(trans, Acore::StringFormat(
        "REPLACE INTO character_paragon_points (guid, earned, prestige) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), state->earned, state->prestige));
}

void AddParagonScripts()
{
    new npc_stat_growth_paragon_keeper();
    new ParagonCharacterListScript();
}
