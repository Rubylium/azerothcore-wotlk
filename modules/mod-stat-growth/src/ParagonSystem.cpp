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
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Random.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StatGrowthConfig.h"
#include "StatGrowthSystem.h"
#include "StringFormat.h"
#include "World.h"
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
    // The outer zones (Ascension, Transcendance): the nodes that make a character something else.
    DamagePct,          // `value`% more damage dealt, always
    ReductionPct,       // `value`% less damage taken, always; the total is capped at MaxReductionPct
    Leech,              // `value`% of the damage dealt comes back as health; the total is capped at MaxLeechPct
    DoubleStrike,       // dealing damage: `chance` for the hit to deal `value`% more
    Execute,            // `value`% more damage against targets under `value2`% health
    Explosion,          // killing something: `value`% of its maximum health to enemies within `value2` yards
    Undying,            // a lethal hit leaves 1 health and no damage lands for `duration`, on `cooldown`
    HealthPct,          // `value`% more maximum health
    KillStreak,         // killing something: a stack of `value`% more damage, up to `value2` stacks, for `duration`
    Splash,             // dealing damage: `value`% of the hit to up to MaxSplashTargets other enemies within `value2` yards
    Count
};

// However the outer zones stack, a character can still be hurt and cannot heal off every hit in full.
constexpr uint32 MaxReductionPct = 75;
constexpr uint32 MaxLeechPct = 50;
// The explosion scales with what died, not with the character: on a Mythic+ pack at a high key a mob has more health
// than a player deals in several seconds, and a few nodes of it summed turned every kill into a one-shot of the pack.
// It stays a small bonus; the nodes past it scale with the character's own damage instead (KillStreak, Splash).
constexpr uint32 MaxExplosionPct = 15;
constexpr uint32 MaxSplashTargets = 4;

// Paragon levels, earned from experience at the level cap. Each level is a point. The bar grows a little each
// level, so the first few come quickly and the hundredth is a commitment.
constexpr uint32 ParagonXpBase = 150000;
constexpr uint32 ParagonXpPerLevel = 7500;

// The procs' own spells (localTools/patchSinisterStrike.ps1). The damage and heal ones are never cast: they name
// what the board deals or heals, so it reaches the combat log, floating text and meters such as Details. The buffs
// mark what is running, with the proc's duration; the effect itself is computed here.
constexpr uint32 SPELL_PARAGON_EXPLOSION = 90650;
constexpr uint32 SPELL_PARAGON_RETALIATE = 90651;
constexpr uint32 SPELL_PARAGON_DOUBLE_STRIKE = 90652;
constexpr uint32 SPELL_PARAGON_EXECUTE = 90653;
constexpr uint32 SPELL_PARAGON_LEECH = 90654;
constexpr uint32 SPELL_PARAGON_FURY = 90655;
constexpr uint32 SPELL_PARAGON_SURGE = 90656;
constexpr uint32 SPELL_PARAGON_GUARD = 90657;
constexpr uint32 SPELL_PARAGON_LAST_STAND = 90658;
constexpr uint32 SPELL_PARAGON_UNDYING = 90659;
constexpr uint32 SPELL_PARAGON_UNDYING_SPENT = 90660;
constexpr uint32 SPELL_PARAGON_KILL_STREAK = 90661;
constexpr uint32 SPELL_PARAGON_SPLASH = 90662;

// Stock SpellVisualKit ids, played where the effect happens so it is seen and not only read in the log
constexpr uint32 VisualExplosion = 984;     // Blast Wave's ring of fire
constexpr uint32 VisualUndying = 417;       // Divine Shield's flash

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
    uint32 required = 0;        // points already spent on the board before this one can be taken
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

    // The always-on effects of the outer zones, summed from the allocation with the procs
    uint32 damagePct = 0;
    uint32 reductionPct = 0;
    uint32 leechPct = 0;
    uint32 healthPct = 0;
    uint32 explosionPct = 0;
    uint32 explosionRange = 0;
    uint32 killStreakPct = 0;               // per stack, summed over the nodes
    uint32 killStreakMax = 0;               // the most stacks any node allows
    uint32 killStreakDuration = 0;
    uint32 splashPct = 0;
    uint32 splashRange = 0;

    // The kill streak currently running: one counter rather than a buff per kill
    uint32 killStreakStacks = 0;
    uint32 killStreakExpiresAt = 0;

    uint32 level = 0;                       // paragon level: experience earned at the level cap
    uint32 experience = 0;                  // towards the next level

    // Damage the procs owe, dealt on the character's next update rather than from inside the hit or the death that
    // set them off: dealing damage from within the damage hook, or from a kill, can kill a unit the core is still in
    // the middle of handling
    struct PendingHit
    {
        ObjectGuid target;
        uint32 spellId = 0;
        uint32 amount = 0;
        SpellSchoolMask school = SPELL_SCHOOL_MASK_NORMAL;
    };
    std::vector<PendingHit> pending;
};

// Set while the board deals its own damage, so that damage is neither boosted by the board nor sets off more procs,
// and a kill it makes does not explode again: one pull of trash would otherwise chain through the instance.
// Per thread, because maps update on several.
thread_local bool DealingProcDamage = false;

void QueueHit(ParagonState* state, Unit* target, uint32 spellId, uint64 amount, SpellSchoolMask school)
{
    if (!target || !amount)
        return;
    state->pending.push_back({ target->GetGUID(), spellId,
        static_cast<uint32>(std::min<uint64>(amount, std::numeric_limits<uint32>::max())), school });
}

// Damage under one of the board's own spells: logged as that spell, so it can be read and counted
void DealProcDamage(Player* player, Unit* target, uint32 spellId, uint32 amount, SpellSchoolMask school)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        Unit::DealDamage(player, target, amount, nullptr, SPELL_DIRECT_DAMAGE, school, nullptr, false);
        return;
    }

    SpellNonMeleeDamage log(player, target, spellInfo, school);
    log.damage = amount;
    Unit::DealDamageMods(target, log.damage, &log.absorb);
    player->SendSpellNonMeleeDamageLog(&log);
    player->DealSpellDamage(&log, false);
}

// A buff marker for a running proc, lasting as long as the proc does
void ShowBuff(Player* player, uint32 spellId, uint32 durationMs)
{
    if (!durationMs)
        return;
    if (Aura* aura = player->AddAura(spellId, player))
    {
        aura->SetMaxDuration(static_cast<int32>(durationMs));
        aura->SetDuration(static_cast<int32>(durationMs));
    }
}

uint32 BuffSpell(ParagonEffect effect)
{
    switch (effect)
    {
        case ParagonEffect::FuryOnHit: return SPELL_PARAGON_FURY;
        case ParagonEffect::SurgeOnKill: return SPELL_PARAGON_SURGE;
        case ParagonEffect::GuardOnHit: return SPELL_PARAGON_GUARD;
        case ParagonEffect::LastStand: return SPELL_PARAGON_LAST_STAND;
        case ParagonEffect::Undying: return SPELL_PARAGON_UNDYING;
        default: return 0;
    }
}

uint32 ExperienceForLevel(uint32 level)
{
    return ParagonXpBase + ParagonXpPerLevel * level;
}

// Whether anything on the damage path has work to do for this character
bool HasCombatEffects(ParagonState const* state)
{
    return state && state->applied && (!state->procs.empty() || state->damagePct || state->reductionPct
        || state->leechPct || state->explosionPct || state->killStreakPct || state->splashPct);
}

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
    if (!state)
        return;

    state->procs.clear();
    state->damagePct = state->reductionPct = state->leechPct = state->healthPct = 0;
    state->explosionPct = state->explosionRange = 0;
    state->killStreakPct = state->killStreakMax = state->killStreakDuration = 0;
    state->splashPct = state->splashRange = 0;

    for (uint32 nodeId : state->allocated)
    {
        auto const entry = Board.find(nodeId);
        if (entry == Board.end())
            continue;

        ParagonNode const& node = entry->second;
        ParagonEffect const effect = static_cast<ParagonEffect>(node.effect);
        switch (effect)
        {
            case ParagonEffect::Stat:
            case ParagonEffect::Armor:
            case ParagonEffect::ArmorPct:
                continue;
            case ParagonEffect::DamagePct:
                state->damagePct += node.value;
                continue;
            case ParagonEffect::ReductionPct:
                state->reductionPct += node.value;
                continue;
            case ParagonEffect::Leech:
                state->leechPct += node.value;
                continue;
            case ParagonEffect::HealthPct:
                state->healthPct += node.value;
                continue;
            case ParagonEffect::Explosion:
                // One blast per kill, however many nodes feed it: the percentages add up, the widest reach wins
                state->explosionPct += node.value;
                state->explosionRange = std::max(state->explosionRange, node.value2);
                continue;
            case ParagonEffect::KillStreak:
                // One streak, however many nodes feed it: each stack is worth their sum, the longest one wins
                state->killStreakPct += node.value;
                state->killStreakMax = std::max(state->killStreakMax, node.value2);
                state->killStreakDuration = std::max(state->killStreakDuration, node.duration);
                continue;
            case ParagonEffect::Splash:
                state->splashPct += node.value;
                state->splashRange = std::max(state->splashRange, node.value2);
                continue;
            default:
                break;
        }

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
    if (uint32 const marker = BuffSpell(proc.effect))
        ShowBuff(player, marker, proc.duration);

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
    {
        if (buff.effect == ParagonEffect::GuardOnHit)
            ApplyArmor(player, buff.applied, false);
        else if (buff.effect == ParagonEffect::SurgeOnKill)
        {
            ApplyPermanentStat(player, PermanentStat::AttackPower, static_cast<uint32>(buff.applied), false);
            ApplyPermanentStat(player, PermanentStat::SpellPower, static_cast<uint32>(buff.applied), false);
        }
        if (uint32 const marker = BuffSpell(buff.effect))
            player->RemoveAurasDueToSpell(marker);
    }
    state->buffs.clear();
    state->pending.clear();
    if (state->killStreakStacks)
        player->RemoveAurasDueToSpell(SPELL_PARAGON_KILL_STREAK);
    state->killStreakStacks = state->killStreakExpiresAt = 0;
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

    Send(player, Acore::StringFormat("PXP\t{}\t{}\t{}", state->level, state->experience,
        ExperienceForLevel(state->level)));
    Send(player, "DONE");
}

// A bot is handed the stat a real board would be worth rather than a board of its own: it has no UI to spend
// points in, nobody would ever see its tree, and 200 bots each carrying an allocation table would be a lot of
// rows for something invisible. What matters is that a bot party keeps pace with the player.
//
// Counted from the stat nodes the player actually holds rather than points times an average: a point in the
// outer zones is worth several in the first, so the same count is a very different board.
uint32 StatValueOf(ParagonState const* state)
{
    if (!state)
        return 0;

    uint32 total = 0;
    for (uint32 nodeId : state->allocated)
        if (auto const node = Board.find(nodeId);
            node != Board.end() && node->second.effect == static_cast<uint8>(ParagonEffect::Stat))
            total += node->second.value;
    return total;
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
        "SELECT id, type, effect, stat, value, value2, chance, duration, cooldown, free, required FROM paragon_node");
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
        node.required = field[10].Get<uint16>();
        if (node.effect >= static_cast<uint8>(ParagonEffect::Count))
        {
            LOG_ERROR("sql.sql", "paragon_node {} has effect {}, which does not exist", node.id, node.effect);
            node.effect = static_cast<uint8>(ParagonEffect::Stat);
        }
        Board[node.id] = node;

        // Cheap order-independent signature, matched against the client's copy of the board
        BoardSignature += node.id * 31 + node.value * 7 + node.stat + node.effect * 3 + node.required * 5;
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

    state->level = 0;
    state->experience = 0;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT level, experience FROM character_paragon_experience WHERE guid = {}", guid))
    {
        Field* field = result->Fetch();
        state->level = field[0].Get<uint32>();
        state->experience = field[1].Get<uint32>();
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
    player->UpdateMaxHealth();
}

void ApplyBotParagon(Player* bot)
{
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;

    // The group's real players decide how much: a bot party is meant to keep pace with the person it is
    // playing with, not with the board it cannot see.
    uint32 value = 0;
    uint32 counted = 0;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource();
                member && member->GetSession() && !member->GetSession()->IsBot())
            {
                value += StatValueOf(GetState(member));
                ++counted;
            }

    uint32 const target = counted ? value / counted : 0;
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
        player->UpdateMaxHealth();
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

    // The far board is reached by building a character, not by a straight run from the hub
    if (SpentPoints(state) < entry->second.required)
    {
        Send(player, std::string("ERROR\t") + Acore::StringFormat(french
            ? "Ce noeud demande {} points déjà dépensés sur le tableau." : "This node needs {} points already spent.",
            entry->second.required));
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
    if (entry->second.effect == static_cast<uint8>(ParagonEffect::HealthPct))
        player->UpdateMaxHealth();
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
    if (!HasCombatEffects(state))
        return;

    uint32 const now = GameTime::GetGameTimeMS().count();
    bool const french = IsFrench(player);
    (void)french;

    // While Undying holds, nothing lands at all
    if (HasBuff(state, ParagonEffect::Undying))
    {
        damage = 0;
        return;
    }

    // Damage reduction is applied before anything rolls, so a proc that fires on this hit does not also
    // soften the hit that set it off.
    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == ParagonEffect::LastStand && buff.applied > 0)
            damage = damage * (100 - std::min<int32>(buff.applied, 90)) / 100;
    if (state->reductionPct)
        damage = damage * (100 - std::min(state->reductionPct, MaxReductionPct)) / 100;

    for (ParagonProc& proc : state->procs)
    {
        switch (proc.effect)
        {
            case ParagonEffect::GuardOnHit:
                // Refreshing rather than stacking: a tank is hit constantly, and stacking would mean the
                // armour never settles anywhere a healer could read.
                if (!HasBuff(state, ParagonEffect::GuardOnHit) && roll_chance_f(proc.chance))
                    StartBuff(player, state, proc, "");
                break;

            case ParagonEffect::RetaliateOnHit:
                if (!DealingProcDamage && roll_chance_f(proc.chance) && attacker->IsAlive())
                    QueueHit(state, attacker, SPELL_PARAGON_RETALIATE,
                        std::max<uint64>(1, uint64(damage) * proc.value / 100), SPELL_SCHOOL_MASK_HOLY);
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
                ShowBuff(player, SPELL_PARAGON_LAST_STAND, proc.duration);
                break;
            }

            case ParagonEffect::Undying:
            {
                // Checked last, against the hit as it will actually land. Never a revive: the character is
                // simply not allowed to die from this hit, and is left on 1 health to get out.
                if (damage < player->GetHealth() || now < proc.readyAt)
                    break;

                damage = player->GetHealth() > 1 ? player->GetHealth() - 1 : 0;
                proc.readyAt = now + proc.cooldown;
                ParagonBuff buff;
                buff.effect = ParagonEffect::Undying;
                buff.expiresAt = now + proc.duration;
                state->buffs.push_back(buff);
                player->SendPlaySpellVisual(VisualUndying);
                ShowBuff(player, SPELL_PARAGON_UNDYING, proc.duration);
                ShowBuff(player, SPELL_PARAGON_UNDYING_SPENT, proc.cooldown);
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
    if (!player || !damage || !victim || attacker == victim || DealingProcDamage)
        return;

    ParagonState* state = GetState(player);
    if (!HasCombatEffects(state))
        return;

    // Worked in 64 bits: the outer zones multiply several times over, and a big hit times a big bonus is past
    // what 32 bits hold before it is divided back down.
    uint64 dealt = damage;
    if (state->damagePct)
        dealt = dealt * (100 + state->damagePct) / 100;

    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == ParagonEffect::FuryOnHit && buff.applied > 0)
            dealt = dealt * (100 + buff.applied) / 100;

    if (state->killStreakStacks && state->killStreakPct)
        dealt = dealt * (100 + state->killStreakStacks * state->killStreakPct) / 100;

    for (ParagonProc& proc : state->procs)
    {
        switch (proc.effect)
        {
            case ParagonEffect::FuryOnHit:
            {
                if (HasBuff(state, ParagonEffect::FuryOnHit) || !roll_chance_f(proc.chance))
                    break;

                ParagonBuff buff;
                buff.effect = ParagonEffect::FuryOnHit;
                buff.expiresAt = GameTime::GetGameTimeMS().count() + proc.duration;
                buff.applied = static_cast<int32>(proc.value);
                state->buffs.push_back(buff);
                ShowBuff(player, SPELL_PARAGON_FURY, proc.duration);
                break;
            }
            default:
                break;
        }
    }

    // The strikes that are their own hits: dealt a moment after this one, under their own name, so they can be seen
    // and counted rather than folded silently into the hit that set them off
    uint64 extra = 0;
    uint64 finishing = 0;
    for (ParagonProc const& proc : state->procs)
    {
        if (proc.effect == ParagonEffect::DoubleStrike && roll_chance_f(proc.chance))
            extra += dealt * proc.value / 100;
        else if (proc.effect == ParagonEffect::Execute && victim->GetHealthPct() < static_cast<float>(proc.value2))
            finishing += dealt * proc.value / 100;
    }
    QueueHit(state, victim, SPELL_PARAGON_DOUBLE_STRIKE, extra, SPELL_SCHOOL_MASK_NORMAL);
    QueueHit(state, victim, SPELL_PARAGON_EXECUTE, finishing, SPELL_SCHOOL_MASK_NORMAL);

    // A share of the hit, never of anything's health: it scales with the character, and a pack is hurt no faster than
    // the character hurts its target. The splash's own hits are proc damage, so they do not splash again.
    if (state->splashPct && state->splashRange)
    {
        uint64 const share = dealt * state->splashPct / 100;
        float const range = static_cast<float>(state->splashRange);
        std::list<Unit*> targets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(victim, player, range);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(victim, targets, check);
        Cell::VisitObjects(victim, searcher, range);

        uint32 hit = 0;
        for (Unit* target : targets)
        {
            if (hit >= MaxSplashTargets)
                break;
            if (target == victim || !target->IsAlive() || !player->IsValidAttackTarget(target))
                continue;
            QueueHit(state, target, SPELL_PARAGON_SPLASH, share, SPELL_SCHOOL_MASK_FIRE);
            ++hit;
        }
    }

    damage = static_cast<uint32>(std::min<uint64>(dealt, std::numeric_limits<uint32>::max()));

    if (state->leechPct && player->IsAlive())
    {
        uint64 const healed = uint64(damage) * std::min(state->leechPct, MaxLeechPct) / 100;
        if (healed)
        {
            // A spell heal, not a silent ModifyHealth, so it is logged and counted
            uint32 const amount = static_cast<uint32>(std::min<uint64>(healed, player->GetMaxHealth()));
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_PARAGON_LEECH))
            {
                HealInfo healInfo(player, player, amount, spellInfo, spellInfo->GetSchoolMask());
                player->HealBySpell(healInfo);
            }
            else
                player->ModifyHealth(static_cast<int32>(amount));
        }
    }
}

void OnParagonKill(Player* player, Unit* killed)
{
    if (!player || !killed || killed->GetTypeId() != TYPEID_UNIT)
        return;

    ParagonState* state = GetState(player);
    if (!HasCombatEffects(state))
        return;

    // The corpse goes up. A kill the blast itself made does not blast again (DealingProcDamage).
    if (state->explosionPct && state->explosionRange && !DealingProcDamage)
    {
        uint64 const blast = uint64(killed->GetMaxHealth()) * std::min(state->explosionPct, MaxExplosionPct) / 100;
        float const range = static_cast<float>(state->explosionRange);
        std::list<Unit*> targets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(killed, player, range);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(killed, targets, check);
        Cell::VisitObjects(killed, searcher, range);

        killed->SendPlaySpellVisual(VisualExplosion);
        for (Unit* target : targets)
            if (target != killed && target->IsAlive() && player->IsValidAttackTarget(target))
                QueueHit(state, target, SPELL_PARAGON_EXPLOSION, blast, SPELL_SCHOOL_MASK_FIRE);
    }

    // Each kill adds a stack and restarts the timer; the stacks go all at once when it runs out
    if (state->killStreakPct && state->killStreakMax && state->killStreakDuration)
    {
        state->killStreakStacks = std::min(state->killStreakStacks + 1, state->killStreakMax);
        state->killStreakExpiresAt = GameTime::GetGameTimeMS().count() + state->killStreakDuration;
        ShowBuff(player, SPELL_PARAGON_KILL_STREAK, state->killStreakDuration);
        if (Aura* aura = player->GetAura(SPELL_PARAGON_KILL_STREAK))
            aura->SetStackAmount(static_cast<uint8>(state->killStreakStacks));
    }

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
        ShowBuff(player, SPELL_PARAGON_SURGE, proc.duration);
    }
}

void ApplyParagonHealth(Player* player, float& value)
{
    ParagonState const* state = GetState(player);
    if (state && state->applied && state->healthPct)
        value *= 1.0f + state->healthPct / 100.0f;
}

void AddParagonExperience(Player* player, uint32 amount)
{
    if (!player || !amount || !player->GetSession() || player->GetSession()->IsBot())
        return;
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled))
        return;
    if (player->GetLevel() < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        return;

    ParagonState* state = GetState(player);
    if (!state)
        return;

    uint32 gained = 0;
    uint64 experience = uint64(state->experience) + amount;
    while (experience >= ExperienceForLevel(state->level))
    {
        experience -= ExperienceForLevel(state->level);
        ++state->level;
        ++gained;
    }
    state->experience = static_cast<uint32>(experience);

    CharacterDatabase.Execute(
        "REPLACE INTO character_paragon_experience (guid, level, experience) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), state->level, state->experience);

    Send(player, Acore::StringFormat("PXP\t{}\t{}\t{}", state->level, state->experience,
        ExperienceForLevel(state->level)));

    if (gained)
    {
        Send(player, Acore::StringFormat("PLEVEL\t{}", state->level));
        AwardParagonPoints(player, gained, IsFrench(player)
            ? Acore::StringFormat("niveau de parangon {}", state->level)
            : Acore::StringFormat("paragon level {}", state->level));
    }
}

void UpdateParagonBuffs(Player* player)
{
    ParagonState* state = GetState(player);
    if (!state)
        return;

    if (!state->pending.empty())
    {
        std::vector<ParagonState::PendingHit> hits;
        hits.swap(state->pending);
        DealingProcDamage = true;
        for (ParagonState::PendingHit const& hit : hits)
            if (Unit* target = ObjectAccessor::GetUnit(*player, hit.target);
                target && target->IsAlive() && target->IsInWorld() && player->IsInMap(target) &&
                player->IsValidAttackTarget(target))
                DealProcDamage(player, target, hit.spellId, hit.amount, hit.school);
        DealingProcDamage = false;
    }

    uint32 const now = GameTime::GetGameTimeMS().count();
    if (state->killStreakStacks && state->killStreakExpiresAt <= now)
    {
        state->killStreakStacks = state->killStreakExpiresAt = 0;
        player->RemoveAurasDueToSpell(SPELL_PARAGON_KILL_STREAK);
    }

    if (state->buffs.empty())
        return;

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
        if (uint32 const marker = BuffSpell(buff.effect))
            player->RemoveAurasDueToSpell(marker);

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
