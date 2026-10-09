#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "World.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Retail-style talent trees for the custom classes that have one (`talentTree` in
// localTools/customClasses/classes.json; the Oathblade first), and for the stock classes moved to them
// (`stockTalentTrees` there; the Mage). Such a class gets no WotLK talent points: a class tree and a spec tree take
// their place, each with its own points, and the window drawing them is FrameXML/TalentTree.lua. A stock class's
// WotLK talents are taken back from anyone who still has them.
//
// The trees are data (custom_talent_tree, custom_talent_node, written by localTools/talentTree/buildTalentTree.py).
// A character's build is one digit per node, in the data's order: the rank taken, or for a choice node the option
// taken. The client edits a build locally and sends the whole of it; everything is checked here, and what the build
// holds becomes spells: the spell of each node's current rank is learned, every other rank's spell of the class's
// trees is removed. The rank spells are the talents' effects (auras, or dummies the class's scripts read).
//
// A class may have several spec trees (the Pestiféré: a tank and a healer). Each talent slot (dual specialization)
// then holds one chosen specialization: its tree's ranks are learned, the other spec trees' are kept in the build
// but not learned, so going back to a specialization finds its talents as they were. A spec tree may name spells
// (SpecSpells) learned while it is the chosen one: its core abilities, and how a class's scripts know which
// specialization is on.
// The chosen tree is saved with the build, as node 0.
//
// A node that teaches an ability, and a spec tree's spells, come in ranks like any trainer spell (Pyroblast, Ice
// Barrier): the character holds the highest rank its level allows, and losing the node takes every rank.
//
// Bots have no window: each tree names the order a bot takes its nodes in (BotOrder), and a bot's build is filled
// from it, as far as its level's points go, whenever it logs in, levels or changes specialization. A spec tree also
// carries the class's recommended builds (SingleBuild, AoeBuild: its class and spec nodes): in a five-man dungeon,
// where the fights are packs, a bot takes its specialization's AoE build instead (in a raid, its single-target one),
// and goes back to its bot order as it leaves (a tree without a bot order uses its single-target build there). They are placed as a player applying
// the build would place them, so what the level does not allow yet simply waits. A spec tree that holds a tank and a
// damage dealer (the Druid's Combat farouche: a cat and a bear) also carries a tank build (TankBuild): a bot whose
// build holds a node of it that neither other build holds (the guardian's path) is a tank, and keeps that build
// wherever it goes; the bots' factory and the combat bench put a bot on it (SetTalentBuildPreset, preset 3).
//
// Addon whispers, prefix "TalentTree":
//   client -> server  OPEN                    the state, please
//                     APPLY <spec> <build>    make this the active build
//                     SPEC <tree>             make this spec tree the active slot's specialization
//                     LSAVE <slot> <name> <tree> <build>   save a loadout: a build planned at the level cap, for
//                                             the class tree and one specialization, under a name (slot 1-10)
//                     LDEL <slot>             delete a loadout
//                     LAPPLY <slot>           apply a loadout: its specialization, then as much of its build as
//                                             the level's points allow (again after a level-up: a little more)
//                     LBUILD <tree> <build>   apply a build not saved (a recommended one), the same way
//   server -> client  S <signature> <active spec> <spec count> <level> <applied> <build 1> <build 2>
//                       <specialization 1> <specialization 2>
//                     E <error> <node>
//                     LC                      the loadouts follow (after OPEN, and after every change to them)
//                     L <slot> <tree> <name> <build>   one loadout (its build in the window's digit format)
//                     LA <slot> <applied> <total>      a loadout applied: points placed of those it holds
namespace
{
constexpr std::string_view Prefix = "TalentTree";

enum class NodeKind : uint8
{
    Passive = 0,
    Active  = 1,
    Choice  = 2
};

// The codes the window turns into text (TalentTree.lua, TEXT.errors)
enum class BuildError : uint8
{
    None        = 0,
    Malformed   = 1,    // wrong length, not digits, or a rank past the node's last
    Combat      = 2,
    Locked      = 3,    // no parent fully ranked
    Gate        = 4,    // not enough points spent above a gate
    Points      = 5,    // more points than the level gives
    Level       = 6,    // a node's own level requirement
    Spec        = 7,    // sent for a spec that is no longer the active one
    Unavailable = 8,
    NoSpec      = 9,    // a specialization this class does not have
    Loadout     = 10    // a loadout that does not exist, or a name or build that could not be read
};

// Loadouts a character keeps
constexpr uint8 MaxLoadouts = 10;
constexpr std::size_t MaxLoadoutName = 32;

struct TreeGate
{
    uint8 row = 0;
    uint8 cost = 0;
};

// A step of a tree's bot order: a node, every rank of it, or one option of a choice node
struct BotPick
{
    uint16 node = 0;
    uint8 option = 1;
};

// A node of a recommended build, at the rank (or option) the build holds
struct PresetPick
{
    uint16 node = 0;
    uint8 value = 0;
};

struct Tree
{
    uint8 id = 0;
    bool spec = false;              // a spec tree (else the class tree)
    std::vector<uint32> specSpells; // learned while this spec tree is the chosen specialization
    std::vector<BotPick> botOrder;
    std::vector<PresetPick> singleBuild;    // a spec tree's recommended builds, its class tree nodes included
    std::vector<PresetPick> aoeBuild;
    std::vector<PresetPick> tankBuild;      // a spec tree holding a tank and a damage dealer (the Druid's Combat
                                            // farouche): the tank's
    uint8 firstLevel = 10;
    uint8 levelStep = 2;
    std::array<TreeGate, 2> gates{};
    uint8 role = 0;                 // a spec tree's way of fighting (GetTalentSpecRole)
};

struct TreeNode
{
    uint16 id = 0;
    uint8 tree = 0;
    uint8 row = 0;
    NodeKind kind = NodeKind::Passive;
    uint8 minLevel = 0;
    std::vector<uint32> spells;     // rank 1..n, or option 1..n of a choice
    std::vector<uint16> parents;    // node ids, same tree
};

struct ClassTrees
{
    uint32 signature = 0;
    std::vector<Tree> trees;
    std::vector<TreeNode> nodes;                        // in build order: nodes[i] is digit i
    std::unordered_map<uint16, std::size_t> positions;  // node id -> index in nodes
    std::vector<uint8> specTrees;                       // the spec trees' ids, in order
};

// Node id under which a slot's chosen specialization is saved in character_talent_tree: no tree has a node 0
constexpr uint16 SpecializationNode = 0;

std::unordered_map<uint8, ClassTrees> Classes;

// The classes that have trees, by id: what a player update asks first, so the others pay nothing
std::array<bool, 32> ClassHasTrees{};

// How often a character on the trees is checked for a stale WotLK talent aura (DropStaleTalentAuras)
constexpr uint32 StaleTalentCheckMs = 3000;

// A character's builds, one per talent spec slot (dual specialization). Kept on the player, so it dies with them.
struct Loadout
{
    uint8 slot = 0;
    std::string name;
    uint8 specialization = 0;
    std::string build;          // the window's digit format; only the class tree and its specialization's digits
};

struct TalentTreeState : public DataMap::Base
{
    std::array<std::string, MAX_TALENT_SPECS> builds;
    std::array<uint8, MAX_TALENT_SPECS> specializations{};     // spec tree id per slot, 0 for the first
    std::vector<Loadout> loadouts;
};

constexpr char const* StateKey = "TalentTreeState";

ClassTrees const* GetTrees(uint8 classId)
{
    auto const itr = Classes.find(classId);
    return itr == Classes.end() ? nullptr : &itr->second;
}

uint8 Value(std::string const& build, std::size_t position)
{
    return position < build.size() ? uint8(build[position] - '0') : 0;
}

// What a node costs at a value: a rank is a point each, a choice is one point whichever option
uint32 Cost(TreeNode const& node, uint8 value)
{
    if (!value)
        return 0;
    return node.kind == NodeKind::Choice ? 1 : value;
}

bool IsMaxed(TreeNode const& node, uint8 value)
{
    return node.kind == NodeKind::Choice ? value > 0 : std::size_t(value) >= node.spells.size();
}

// A character above the level cap (a game master's test character, levelled past it) has the cap's points: the
// recommended builds and every check are made for them
uint32 PointsAt(Tree const& tree, uint8 level)
{
    level = uint8(std::min<uint32>(level, sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)));
    return level < tree.firstLevel ? 0 : uint32(level - tree.firstLevel) / tree.levelStep + 1;
}

Tree const* FindTree(ClassTrees const& data, uint8 treeId)
{
    for (Tree const& tree : data.trees)
        if (tree.id == treeId)
            return &tree;
    return nullptr;
}

// Points spent in a tree, or only in its rows above `row` when a gate asks
uint32 Spent(ClassTrees const& data, std::string const& build, uint8 treeId, uint8 belowRow = 255)
{
    uint32 spent = 0;
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
    {
        TreeNode const& node = data.nodes[position];
        if (node.tree == treeId && node.row < belowRow)
            spent += Cost(node, Value(build, position));
    }
    return spent;
}

// Whether a whole build may be held at a level. The order it was clicked in does not matter: every rule is a
// property of the finished build, so a build is either legal or it is not.
BuildError Validate(ClassTrees const& data, std::string const& build, uint8 level, uint16& offender)
{
    offender = 0;
    if (build.size() != data.nodes.size())
        return BuildError::Malformed;

    for (std::size_t position = 0; position < data.nodes.size(); ++position)
    {
        TreeNode const& node = data.nodes[position];
        char const digit = build[position];
        if (digit < '0' || digit > '9' || std::size_t(digit - '0') > node.spells.size())
        {
            offender = node.id;
            return BuildError::Malformed;
        }
    }

    for (Tree const& tree : data.trees)
    {
        if (Spent(data, build, tree.id) <= PointsAt(tree, level))
            continue;
        for (std::size_t position = 0; position < data.nodes.size(); ++position)
            if (data.nodes[position].tree == tree.id && Value(build, position))
                offender = data.nodes[position].id;
        return BuildError::Points;
    }

    for (std::size_t position = 0; position < data.nodes.size(); ++position)
    {
        TreeNode const& node = data.nodes[position];
        if (!Value(build, position))
            continue;
        offender = node.id;

        if (node.minLevel > level)
            return BuildError::Level;

        if (!node.parents.empty())
        {
            bool open = false;
            for (uint16 parentId : node.parents)
            {
                auto const parent = data.positions.find(parentId);
                if (parent != data.positions.end() &&
                    IsMaxed(data.nodes[parent->second], Value(build, parent->second)))
                {
                    open = true;
                    break;
                }
            }
            if (!open)
                return BuildError::Locked;
        }

        if (Tree const* tree = FindTree(data, node.tree))
            for (TreeGate const& gate : tree->gates)
                if (gate.cost && node.row >= gate.row && Spent(data, build, node.tree, gate.row) < gate.cost)
                    return BuildError::Gate;
    }

    offender = 0;
    return BuildError::None;
}

// Brings a build back within what a level allows, one whole tree at a time: a tree that no longer fits is handed
// back entirely rather than guessed at node by node. Returns whether anything was taken back.
bool Trim(ClassTrees const& data, std::string& build, uint8 level)
{
    if (build.size() != data.nodes.size())
    {
        build.assign(data.nodes.size(), '0');
        return true;
    }

    bool trimmed = false;
    for (std::size_t attempt = 0; attempt <= data.trees.size(); ++attempt)
    {
        uint16 offender = 0;
        BuildError const error = Validate(data, build, level, offender);
        if (error == BuildError::None)
            break;

        auto const position = data.positions.find(offender);
        uint8 const treeId = position == data.positions.end() ? 0 : data.nodes[position->second].tree;
        for (std::size_t index = 0; index < data.nodes.size(); ++index)
            if (!treeId || data.nodes[index].tree == treeId)
                build[index] = '0';
        trimmed = true;
    }
    return trimmed;
}

// A slot's specialization: the spec tree it chose, or the first when it chose none (or one the class lost)
uint8 Specialization(ClassTrees const& data, uint8 chosen)
{
    if (std::find(data.specTrees.begin(), data.specTrees.end(), chosen) != data.specTrees.end())
        return chosen;
    return data.specTrees.empty() ? 0 : data.specTrees.front();
}

void SetSpell(Player* player, uint32 spellId, bool wanted)
{
    if (!spellId || !sSpellMgr->GetSpellInfo(spellId))
        return;
    if (wanted && !player->HasSpell(spellId))
        player->learnSpell(spellId, false);
    else if (!wanted && player->HasSpell(spellId))
        player->removeSpell(spellId, SPEC_MASK_ALL, false);
}

// Every spell of the trees, and whether anything in the build still wants it: one tree or node wanting a spell keeps
// it, whatever another says
using WantedSpells = std::unordered_map<uint32, bool>;

void WantSpell(WantedSpells& spells, uint32 spellId, bool wanted)
{
    bool& entry = spells[spellId];
    entry = entry || wanted;
}

// An ability and its ranks: wanted, every rank up to the highest the character's level allows (as a trainer teaches
// them, one after the other), none above; not wanted, none at all
void WantAbility(Player* player, WantedSpells& spells, uint32 spellId, bool wanted)
{
    uint32 const first = sSpellMgr->GetFirstSpellInChain(spellId);
    bool reachable = wanted;
    for (uint32 rank = first; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(rank);
        if (rank != first && (!spellInfo || spellInfo->SpellLevel > player->GetLevel()))
            reachable = false;
        WantSpell(spells, rank, reachable);
    }
}

// Makes the character's spells match a build: each node's current rank (or chosen option) learned, every other
// spell of the trees removed, so a lower rank, a refunded node or the other option never lingers. Only the class tree
// and the chosen spec tree count; the other spec trees keep their picks in the build, unlearned.
//
// What the build wants is settled first, then only the difference is applied. Removing a spell and learning it again
// takes it off the player's action bars: Protection and the Gladiateur share Shield Slam and Devastate, and the two
// left the bar at every level and every talent point while Protection's copy was removed before the Gladiateur's was
// learned back.
void Reconcile(Player* player, ClassTrees const& data, std::string const& build, uint8 specialization)
{
    WantedSpells spells;
    for (Tree const& tree : data.trees)
        if (tree.spec)
            for (uint32 spellId : tree.specSpells)
                WantAbility(player, spells, spellId, tree.id == specialization);

    for (std::size_t position = 0; position < data.nodes.size(); ++position)
    {
        TreeNode const& node = data.nodes[position];
        Tree const* tree = FindTree(data, node.tree);
        bool const counts = !tree || !tree->spec || tree->id == specialization;
        uint8 const value = !counts || node.minLevel > player->GetLevel() ? 0 : Value(build, position);
        for (std::size_t index = 0; index < node.spells.size(); ++index)
        {
            bool const wanted = std::size_t(value) == index + 1;
            if (node.kind == NodeKind::Active)
                WantAbility(player, spells, node.spells[index], wanted);
            else
                WantSpell(spells, node.spells[index], wanted);
        }
    }

    // What goes first, then what comes: a lower rank is gone before the higher one is taught
    for (bool const learn : { false, true })
        for (auto const& [spellId, wanted] : spells)
            if (wanted == learn)
                SetSpell(player, spellId, wanted);
}

// The trees reuse WotLK talent ranks as nodes (Deep Wounds, Flurry, Ignite...). Reconcile takes a rank off with the
// spell, but its passive aura could outlive it until the next login: a bot moved from Arms to Fury kept Deep Wounds'.
// A passive WotLK talent aura of the character's own whose spell it no longer knows goes. Every class on the trees,
// a few times a minute, each character at its own moment (no state kept): an aura list is short.
void DropStaleTalentAuras(Player* player)
{
    std::vector<uint32> stale;
    for (auto const& [spellId, aura] : player->GetOwnedAuras())
        if (aura->IsPassive() && aura->GetCasterGUID() == player->GetGUID() && GetTalentSpellPos(spellId) &&
            !player->HasSpell(spellId))
            stale.push_back(spellId);
    for (uint32 spellId : stale)
        player->RemoveOwnedAura(spellId);
}

TalentTreeState* GetState(Player* player)
{
    return player ? player->CustomData.Get<TalentTreeState>(StateKey) : nullptr;
}

uint8 ActiveSlot(Player* player)
{
    return player->GetActiveSpec() < MAX_TALENT_SPECS ? player->GetActiveSpec() : 0;
}

void ReconcileActive(Player* player, ClassTrees const& data, TalentTreeState* state)
{
    uint8 const slot = ActiveSlot(player);
    Reconcile(player, data, state->builds[slot], Specialization(data, state->specializations[slot]));
    DropStaleTalentAuras(player);
}

void Send(Player* player, std::string const& body)
{
    if (!player->GetSession() || player->GetSession()->IsBot())
        return;

    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(Prefix) + "\t" + body);
    player->GetSession()->SendPacket(&packet);
}

void SendState(Player* player, bool applied)
{
    ClassTrees const* data = GetTrees(player->getClass());
    TalentTreeState* state = GetState(player);
    if (!data || !state)
        return;

    Send(player, Acore::StringFormat("S\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", data->signature,
        player->GetActiveSpec(), player->GetSpecsCount(), player->GetLevel(), applied ? 1 : 0, state->builds[0],
        state->builds[1], Specialization(*data, state->specializations[0]),
        Specialization(*data, state->specializations[1])));
}

void SendError(Player* player, BuildError error, uint16 node)
{
    Send(player, Acore::StringFormat("E\t{}\t{}", uint32(error), node));
}

void SaveBuild(Player* player, ClassTrees const& data, TalentTreeState const* state, uint8 spec)
{
    std::string const& build = state->builds[spec];
    uint32 const guid = player->GetGUID().GetCounter();
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append("DELETE FROM character_talent_tree WHERE guid = {} AND spec = {}", guid, spec);
    if (!data.specTrees.empty())
        trans->Append("INSERT INTO character_talent_tree (guid, spec, node, `value`) VALUES ({}, {}, {}, {})",
            guid, spec, SpecializationNode, Specialization(data, state->specializations[spec]));
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
        if (uint8 const value = Value(build, position))
            trans->Append("INSERT INTO character_talent_tree (guid, spec, node, `value`) VALUES ({}, {}, {}, {})",
                guid, spec, data.nodes[position].id, value);
    CharacterDatabase.CommitTransaction(trans);
}

// Every build brought back within the character's level; the active one is then what the spells follow
void TrimAndReconcile(Player* player, ClassTrees const& data, TalentTreeState* state)
{
    bool trimmedActive = false;
    for (uint8 spec = 0; spec < MAX_TALENT_SPECS; ++spec)
    {
        if (!Trim(data, state->builds[spec], player->GetLevel()))
            continue;
        SaveBuild(player, data, state, spec);
        trimmedActive = trimmedActive || spec == player->GetActiveSpec();
    }

    if (trimmedActive && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(
            player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR
                ? "|cffffd100Talents :|r votre niveau ne permet plus certains de vos talents, "
                  "leurs points vous sont rendus."
                : "|cffffd100Talents:|r some of your talents are beyond your level now; their points are given back.");

    ReconcileActive(player, data, state);
}

void LoadForPlayer(Player* player, ClassTrees const& data)
{
    TalentTreeState* state = player->CustomData.GetDefault<TalentTreeState>(StateKey);
    for (std::string& build : state->builds)
        build.assign(data.nodes.size(), '0');

    if (QueryResult result = CharacterDatabase.Query(
            "SELECT spec, node, `value` FROM character_talent_tree WHERE guid = {}", player->GetGUID().GetCounter()))
        do
        {
            Field* field = result->Fetch();
            uint8 const spec = field[0].Get<uint8>();
            uint16 const nodeId = field[1].Get<uint16>();
            uint8 const value = field[2].Get<uint8>();
            if (nodeId == SpecializationNode)
            {
                if (spec < MAX_TALENT_SPECS)
                    state->specializations[spec] = Specialization(data, value);
                continue;
            }
            auto const position = data.positions.find(nodeId);
            // A node the trees no longer have, or a rank past its last, is dropped: its points are simply free again
            if (spec >= MAX_TALENT_SPECS || position == data.positions.end() || value > 9 ||
                std::size_t(value) > data.nodes[position->second].spells.size())
                continue;
            state->builds[spec][position->second] = char('0' + value);
        } while (result->NextRow());

    state->loadouts.clear();
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT slot, name, specialization, build FROM character_talent_loadout WHERE guid = {} ORDER BY slot",
            player->GetGUID().GetCounter()))
        do
        {
            Field* field = result->Fetch();
            Loadout loadout;
            loadout.slot = field[0].Get<uint8>();
            loadout.name = field[1].Get<std::string>();
            loadout.specialization = field[2].Get<uint8>();
            loadout.build.assign(data.nodes.size(), '0');
            for (std::string_view pair : Acore::Tokenize(field[3].Get<std::string_view>(), ',', false))
            {
                std::vector<std::string_view> const parts = Acore::Tokenize(pair, ':', false);
                Optional<uint16> const nodeId = parts.size() == 2 ? Acore::StringTo<uint16>(parts[0]) : std::nullopt;
                Optional<uint8> const value = parts.size() == 2 ? Acore::StringTo<uint8>(parts[1]) : std::nullopt;
                auto const position = nodeId ? data.positions.find(*nodeId) : data.positions.end();
                // A node the trees no longer have is dropped, like in a build
                if (position == data.positions.end() || !value || *value > 9 ||
                    std::size_t(*value) > data.nodes[position->second].spells.size())
                    continue;
                loadout.build[position->second] = char('0' + *value);
            }
            state->loadouts.push_back(std::move(loadout));
        } while (result->NextRow());
}

void HandleApply(Player* player, ClassTrees const& data, TalentTreeState* state, std::string_view arguments)
{
    std::vector<std::string_view> const parts = Acore::Tokenize(arguments, '\t', false);
    if (parts.size() != 2)
        return SendError(player, BuildError::Malformed, 0);

    Optional<uint8> const spec = Acore::StringTo<uint8>(parts[0]);
    if (!spec || *spec != player->GetActiveSpec())
        return SendError(player, BuildError::Spec, 0);

    if (player->IsInCombat())
        return SendError(player, BuildError::Combat, 0);

    std::string build(parts[1]);
    uint16 offender = 0;
    BuildError const error = Validate(data, build, player->GetLevel(), offender);
    if (error != BuildError::None)
    {
        SendError(player, error, offender);
        return SendState(player, false);
    }

    state->builds[*spec] = build;
    SaveBuild(player, data, state, *spec);
    ReconcileActive(player, data, state);
    SendState(player, true);
}

// Makes a spec tree the active slot's specialization: its talents (as last left) are learned, the old one's not
void ChangeSpecialization(Player* player, ClassTrees const& data, TalentTreeState* state, uint8 tree)
{
    uint8 const slot = ActiveSlot(player);
    if (Specialization(data, state->specializations[slot]) == tree)
        return;
    state->specializations[slot] = tree;
    SaveBuild(player, data, state, slot);
    ReconcileActive(player, data, state);
}

bool IsBot(Player* player)
{
    return player->GetSession() && player->GetSession()->IsBot();
}

// A node of the class tree or of a specialization's tree: what a loadout (and a recommended build) holds
bool IsInLoadout(ClassTrees const& data, TreeNode const& node, uint8 specialization)
{
    Tree const* tree = FindTree(data, node.tree);
    return tree && (!tree->spec || tree->id == specialization);
}

// Places a wanted build (digit format) into a build the way a player applying it would click it: the class tree
// first, then row by row, each node ranked as far as it goes while the build stays legal at the level. The class
// tree's and the specialization's nodes start from nothing; the other spec trees' are left as they are.
void PlaceBuild(ClassTrees const& data, std::string& build, std::string const& wanted, uint8 specialization,
    uint8 level)
{
    std::vector<std::size_t> order;
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
        if (IsInLoadout(data, data.nodes[position], specialization))
            order.push_back(position);
    std::stable_sort(order.begin(), order.end(), [&data](std::size_t a, std::size_t b)
    {
        TreeNode const& left = data.nodes[a];
        TreeNode const& right = data.nodes[b];
        bool const leftClass = !FindTree(data, left.tree) || !FindTree(data, left.tree)->spec;
        bool const rightClass = !FindTree(data, right.tree) || !FindTree(data, right.tree)->spec;
        if (leftClass != rightClass)
            return leftClass;
        return left.row < right.row;
    });

    for (std::size_t position : order)
        build[position] = '0';

    uint16 offender = 0;
    // Twice over: a node whose parent sits later in the same row gets its turn the second time
    for (int pass = 0; pass < 2; ++pass)
        for (std::size_t position : order)
        {
            TreeNode const& node = data.nodes[position];
            uint8 const target = Value(wanted, position);
            char& digit = build[position];
            if (!target || Value(build, position) >= target)
                continue;
            if (node.kind == NodeKind::Choice)
            {
                digit = char('0' + target);
                if (Validate(data, build, level, offender) != BuildError::None)
                    digit = '0';
                continue;
            }
            while (Value(build, position) < target)
            {
                ++digit;
                if (Validate(data, build, level, offender) != BuildError::None)
                {
                    --digit;
                    break;
                }
            }
        }
}

// Where a bot fights packs: a five-man dungeon (a Mythic+ key included)
bool WantsAoeBuild(Player* player)
{
    Map const* map = player->FindMap();
    return map && map->IsNonRaidDungeon();
}

// Where a bot fights bosses: a raid (the board's own bosses included). It takes its specialization's single-target
// build there, the one the balance is measured on (the simulation bench's single-target tests); its bot order, built
// for levelling, put some specs on their AoE talents against a raid boss (a Beast Mastery hunter 20% over the others).
bool WantsSingleBuild(Player* player)
{
    Map const* map = player->FindMap();
    return map && map->IsRaid();
}

// A recommended build a bot keeps wherever it is (the combat bench's single-target or AoE tests), until cleared
enum class BuildPreset : uint8
{
    Auto    = 0,
    Single  = 1,
    Aoe     = 2,
    Tank    = 3     // placed, never kept as forced: the build itself says the bot is a tank (IsOnTankBuild)
};

constexpr char const* PresetKey = "TalentTreeBuildPreset";
struct ForcedPreset : DataMap::Base
{
    BuildPreset preset = BuildPreset::Auto;
};

BuildPreset GetForcedPreset(Player* player)
{
    ForcedPreset const* forced = player->CustomData.Get<ForcedPreset>(PresetKey);
    return forced ? forced->preset : BuildPreset::Auto;
}

// A recommended build as the window's digit format
std::string PresetDigits(ClassTrees const& data, std::vector<PresetPick> const& preset)
{
    std::string digits(data.nodes.size(), '0');
    for (PresetPick const& pick : preset)
    {
        auto const position = data.positions.find(pick.node);
        if (position == data.positions.end() || !pick.value)
            continue;
        digits[position->second] = char('0' + std::min<std::size_t>(pick.value,
            data.nodes[position->second].spells.size()));
    }
    return digits;
}

// Whether a build is on its specialization's tank build: it holds a node of it, at the tank build's value, that
// neither the single-target nor the AoE build holds at that value (the Druid's guardian's path)
bool IsOnTankBuild(ClassTrees const& data, Tree const& specTree, std::string const& build)
{
    if (specTree.tankBuild.empty())
        return false;
    auto const valueIn = [](std::vector<PresetPick> const& preset, uint16 node)
    {
        auto const pick = std::find_if(preset.begin(), preset.end(),
            [node](PresetPick const& candidate) { return candidate.node == node; });
        return pick == preset.end() ? uint8(0) : pick->value;
    };
    for (PresetPick const& pick : specTree.tankBuild)
    {
        if (valueIn(specTree.singleBuild, pick.node) == pick.value ||
            valueIn(specTree.aoeBuild, pick.node) == pick.value)
            continue;
        auto const position = data.positions.find(pick.node);
        if (position != data.positions.end() && Value(build, position->second) == pick.value)
            return true;
    }
    return false;
}

// A bot's build: on a tank build (`tank`, or a build that already holds it) its specialization's tank build; in a
// dungeon its AoE build, in a raid its single-target build; elsewhere its trees' bot orders taken in turn (or, for a spec tree without one, its
// single-target build), each node ranked as far as it goes, until the level's points run out. Only the class tree
// and the chosen specialization's tree are filled. Returns whether the build changed (it is then saved; the caller
// makes the spells follow).
bool FillBotBuild(Player* player, ClassTrees const& data, TalentTreeState* state, bool tank = false)
{
    uint8 const slot = ActiveSlot(player);
    uint8 const specialization = Specialization(data, state->specializations[slot]);
    std::string build(data.nodes.size(), '0');

    Tree const* specTree = FindTree(data, specialization);
    std::vector<PresetPick> const* preset = nullptr;
    if (specTree && specTree->spec)
    {
        BuildPreset const forced = GetForcedPreset(player);
        if (!specTree->tankBuild.empty() && (tank ||
                (forced == BuildPreset::Auto && IsOnTankBuild(data, *specTree, state->builds[slot]))))
            preset = &specTree->tankBuild;
        else if (forced == BuildPreset::Aoe && !specTree->aoeBuild.empty())
            preset = &specTree->aoeBuild;
        else if (forced == BuildPreset::Single && !specTree->singleBuild.empty())
            preset = &specTree->singleBuild;
        else if (forced != BuildPreset::Auto)
            preset = nullptr;
        else if (WantsAoeBuild(player) && !specTree->aoeBuild.empty())
            preset = &specTree->aoeBuild;
        else if (WantsSingleBuild(player) && !specTree->singleBuild.empty())
            preset = &specTree->singleBuild;
        else if (specTree->botOrder.empty() && !specTree->singleBuild.empty())
            preset = &specTree->singleBuild;
    }

    if (preset)
    {
        PlaceBuild(data, build, PresetDigits(data, *preset), specialization, player->GetLevel());
        if (build == state->builds[slot])
            return false;
        state->builds[slot] = build;
        SaveBuild(player, data, state, slot);
        return true;
    }

    uint16 offender = 0;
    for (Tree const& tree : data.trees)
    {
        if (tree.spec && tree.id != specialization)
            continue;

        for (BotPick const& pick : tree.botOrder)
        {
            auto const position = data.positions.find(pick.node);
            if (position == data.positions.end())
                continue;

            TreeNode const& node = data.nodes[position->second];
            char& digit = build[position->second];
            if (node.kind == NodeKind::Choice)
            {
                char const before = digit;
                digit = char('0' + std::min<std::size_t>(pick.option, node.spells.size()));
                if (Validate(data, build, player->GetLevel(), offender) != BuildError::None)
                    digit = before;
                continue;
            }

            while (std::size_t(digit - '0') < node.spells.size())
            {
                ++digit;
                if (Validate(data, build, player->GetLevel(), offender) != BuildError::None)
                {
                    --digit;
                    break;
                }
            }
        }
    }

    if (build == state->builds[slot])
        return false;
    state->builds[slot] = build;
    SaveBuild(player, data, state, slot);
    return true;
}

// The WotLK talent tab a bot had spent the most points in (the active slot's), -1 without any: read before the WotLK
// talents are wiped, it is the specialization the bot played
int8 DominantStockTab(Player* player)
{
    std::array<uint32, 3> points{};
    uint32 const* tabs = GetTalentTabPages(player->getClass());
    for (auto const& [spellId, talent] : player->GetTalentMap())
    {
        if (talent->State == PLAYERSPELL_REMOVED || !(talent->specMask & player->GetActiveSpecMask()))
            continue;
        TalentSpellPos const* position = GetTalentSpellPos(spellId);
        TalentEntry const* entry = position ? sTalentStore.LookupEntry(position->talent_id) : nullptr;
        if (!entry)
            continue;
        for (uint8 tab = 0; tab < points.size(); ++tab)
            if (entry->TalentTab == tabs[tab])
                points[tab] += position->rank + 1;
    }
    auto const most = std::max_element(points.begin(), points.end());
    return *most ? int8(most - points.begin()) : -1;
}

// The WotLK talent that made a bot a tank where one spec tree now holds a tank and a damage dealer: the Druid's Thick
// Hide at its last rank (the bots' own test of a bear). Read before the WotLK talents are wiped: such a bot moved to
// the trees takes the tank build.
constexpr uint32 DruidThickHide = 16931;

bool WasStockTank(Player* player)
{
    return player->getClass() == CLASS_DRUID && player->HasTalent(DruidThickHide, player->GetActiveSpec());
}

// A bot that never chose a specialization takes one: the WotLK tab it played, else a draw weighted by the bots'
// AiPlayerbot.RandomClassSpecProb.<class>.<tab> (even without it). The spec trees come in the tabs' order.
void ChooseBotSpecialization(Player* player, ClassTrees const& data, TalentTreeState* state, int8 stockTab)
{
    uint8 const slot = ActiveSlot(player);
    if (state->specializations[slot] || data.specTrees.empty())
        return;

    std::size_t index = 0;
    if (stockTab >= 0)
        index = std::size_t(stockTab);
    else
    {
        std::vector<uint32> weights;
        for (std::size_t tab = 0; tab < data.specTrees.size(); ++tab)
            weights.push_back(sConfigMgr->GetOption<uint32>(
                Acore::StringFormat("AiPlayerbot.RandomClassSpecProb.{}.{}", player->getClass(), tab), 1, false));
        uint32 total = 0;
        for (uint32 weight : weights)
            total += weight;
        if (total)
        {
            uint32 roll = urand(0, total - 1);
            while (roll >= weights[index])
                roll -= weights[index++];
        }
    }
    state->specializations[slot] = data.specTrees[std::min(index, data.specTrees.size() - 1)];
    SaveBuild(player, data, state, slot);
}

// A class moved from WotLK talents to the trees (the Mage) takes the WotLK ones back: the active talent slot's here,
// the other slot's when the character switches to it
void WipeStockTalents(Player* player)
{
    if (!player->GetTalentMap().empty())
        player->resetTalents(true);
}

// A player whose class has just moved to the trees (stockTab: the WotLK tab it had spent the most points in, read
// before those talents are wiped) keeps that specialization rather than landing in the first spec tree, and is told
// where its talents went. The build itself starts empty: the WotLK talents do not map onto the nodes.
void KeepStockSpecialization(Player* player, ClassTrees const& data, TalentTreeState* state, int8 stockTab)
{
    if (stockTab < 0 || data.specTrees.empty())
        return;

    uint8 const slot = ActiveSlot(player);
    if (!state->specializations[slot] && std::size_t(stockTab) < data.specTrees.size())
    {
        state->specializations[slot] = data.specTrees[stockTab];
        SaveBuild(player, data, state, slot);
    }

    if (player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(
            player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR
                ? "|cffffd100Talents :|r votre classe passe aux nouveaux arbres de talents. Vos anciens talents vous "
                  "sont rendus et votre spécialisation est conservée : ouvrez la fenêtre des talents (N) pour "
                  "dépenser vos points, ou appliquer une configuration recommandée."
                : "|cffffd100Talents:|r your class moved to the new talent trees. Your old talents are refunded and "
                  "your specialization is kept: open the talent window (N) to spend your points, or apply a "
                  "recommended build.");
}

void HandleSpecialization(Player* player, ClassTrees const& data, TalentTreeState* state, std::string_view argument)
{
    Optional<uint8> const tree = Acore::StringTo<uint8>(argument);
    if (!tree || std::find(data.specTrees.begin(), data.specTrees.end(), *tree) == data.specTrees.end())
        return SendError(player, BuildError::NoSpec, 0);

    if (player->IsInCombat())
        return SendError(player, BuildError::Combat, 0);

    ChangeSpecialization(player, data, state, *tree);
    SendState(player, true);
}

// --- Loadouts ---------------------------------------------------------------------------------------------------

void SendLoadouts(Player* player, TalentTreeState const* state)
{
    Send(player, "LC");
    for (Loadout const& loadout : state->loadouts)
        Send(player, Acore::StringFormat("L\t{}\t{}\t{}\t{}", loadout.slot, loadout.specialization, loadout.name,
            loadout.build));
}

Loadout* FindLoadout(TalentTreeState* state, uint8 slot)
{
    for (Loadout& loadout : state->loadouts)
        if (loadout.slot == slot)
            return &loadout;
    return nullptr;
}

// A name as the window may show it: no tabs or escape codes, trimmed, at most MaxLoadoutName characters
std::string CleanName(std::string_view raw)
{
    std::string name;
    for (char const c : raw)
        if (c != '\t' && c != '|' && c != '\n' && c != '\r')
            name += c;
    std::size_t const first = name.find_first_not_of(' ');
    if (first == std::string::npos)
        return {};
    name = name.substr(first, name.find_last_not_of(' ') - first + 1);
    if (name.size() > MaxLoadoutName)
    {
        // Cut on a character boundary: never in the middle of an accented letter
        std::size_t cut = MaxLoadoutName;
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80)
            --cut;
        name.resize(cut);
    }
    return name;
}

void HandleLoadoutSave(Player* player, ClassTrees const& data, TalentTreeState* state, std::string_view arguments)
{
    std::vector<std::string_view> const parts = Acore::Tokenize(arguments, '\t', true);
    if (parts.size() != 4)
        return SendError(player, BuildError::Loadout, 0);

    Optional<uint8> const slot = Acore::StringTo<uint8>(parts[0]);
    std::string const name = CleanName(parts[1]);
    Optional<uint8> const tree = Acore::StringTo<uint8>(parts[2]);
    if (!slot || *slot < 1 || *slot > MaxLoadouts || name.empty() || !tree)
        return SendError(player, BuildError::Loadout, 0);
    uint8 const specialization = data.specTrees.empty() ? 0 : *tree;
    if (!data.specTrees.empty() &&
        std::find(data.specTrees.begin(), data.specTrees.end(), specialization) == data.specTrees.end())
        return SendError(player, BuildError::NoSpec, 0);

    // Planned at the level cap: legal there, and only the class tree and its specialization's nodes are kept
    std::string build(parts[3]);
    if (build.size() != data.nodes.size())
        return SendError(player, BuildError::Malformed, 0);
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
        if (!IsInLoadout(data, data.nodes[position], specialization))
            build[position] = '0';
    uint16 offender = 0;
    BuildError const error = Validate(data, build, uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)), offender);
    if (error != BuildError::None)
        return SendError(player, error, offender);

    Loadout* loadout = FindLoadout(state, *slot);
    if (!loadout)
    {
        state->loadouts.push_back({});
        loadout = &state->loadouts.back();
        loadout->slot = *slot;
    }
    loadout->name = name;
    loadout->specialization = specialization;
    loadout->build = build;
    std::sort(state->loadouts.begin(), state->loadouts.end(),
        [](Loadout const& a, Loadout const& b) { return a.slot < b.slot; });

    std::string pairs;
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
        if (uint8 const value = Value(build, position))
            pairs += Acore::StringFormat("{}{}:{}", pairs.empty() ? "" : ",", data.nodes[position].id, value);
    std::string escaped = name;
    CharacterDatabase.EscapeString(escaped);
    CharacterDatabase.Execute("REPLACE INTO character_talent_loadout (guid, slot, name, specialization, build) "
        "VALUES ({}, {}, '{}', {}, '{}')", player->GetGUID().GetCounter(), *slot, escaped, specialization, pairs);
    SendLoadouts(player, state);
}

void HandleLoadoutDelete(Player* player, TalentTreeState* state, std::string_view argument)
{
    Optional<uint8> const slot = Acore::StringTo<uint8>(argument);
    if (!slot)
        return SendError(player, BuildError::Loadout, 0);
    std::erase_if(state->loadouts, [slot](Loadout const& loadout) { return loadout.slot == *slot; });
    CharacterDatabase.Execute("DELETE FROM character_talent_loadout WHERE guid = {} AND slot = {}",
        player->GetGUID().GetCounter(), *slot);
    SendLoadouts(player, state);
}

// Applies a loadout (or a recommended build, slot 0): its specialization, then its nodes in the order a player would
// take them (row by row, each ranked as far as it goes), each rank only while the build stays legal at the
// character's level. At the level cap that is the whole of it; while levelling, as much as the points allow, and a
// little more after each level-up.
void ApplyLoadoutBuild(Player* player, ClassTrees const& data, TalentTreeState* state, Loadout const* loadout)
{
    if (player->IsInCombat())
        return SendError(player, BuildError::Combat, 0);

    uint8 const slot = ActiveSlot(player);
    if (!data.specTrees.empty() && Specialization(data, state->specializations[slot]) != loadout->specialization)
        ChangeSpecialization(player, data, state, loadout->specialization);

    std::string build = state->builds[slot];
    PlaceBuild(data, build, loadout->build, loadout->specialization, player->GetLevel());

    uint32 total = 0;
    uint32 placed = 0;
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
    {
        if (!IsInLoadout(data, data.nodes[position], loadout->specialization))
            continue;
        total += Cost(data.nodes[position], Value(loadout->build, position));
        placed += Cost(data.nodes[position], Value(build, position));
    }

    state->builds[slot] = build;
    SaveBuild(player, data, state, slot);
    ReconcileActive(player, data, state);
    SendState(player, true);
    Send(player, Acore::StringFormat("LA\t{}\t{}\t{}", loadout->slot, placed, total));
}

void HandleLoadoutApply(Player* player, ClassTrees const& data, TalentTreeState* state, std::string_view argument)
{
    Optional<uint8> const slotId = Acore::StringTo<uint8>(argument);
    Loadout const* loadout = slotId ? FindLoadout(state, *slotId) : nullptr;
    if (!loadout)
        return SendError(player, BuildError::Loadout, 0);
    ApplyLoadoutBuild(player, data, state, loadout);
}

// A build the window holds but the character did not save (a recommended build): checked like a loadout being
// saved, then applied like one
void HandleBuildApply(Player* player, ClassTrees const& data, TalentTreeState* state, std::string_view arguments)
{
    std::vector<std::string_view> const parts = Acore::Tokenize(arguments, '\t', true);
    Optional<uint8> const tree = parts.size() == 2 ? Acore::StringTo<uint8>(parts[0]) : std::nullopt;
    if (!tree)
        return SendError(player, BuildError::Malformed, 0);
    Loadout loadout;
    loadout.specialization = data.specTrees.empty() ? 0 : *tree;
    if (!data.specTrees.empty() &&
        std::find(data.specTrees.begin(), data.specTrees.end(), loadout.specialization) == data.specTrees.end())
        return SendError(player, BuildError::NoSpec, 0);
    loadout.build = std::string(parts[1]);
    if (loadout.build.size() != data.nodes.size())
        return SendError(player, BuildError::Malformed, 0);
    for (std::size_t position = 0; position < data.nodes.size(); ++position)
        if (!IsInLoadout(data, data.nodes[position], loadout.specialization))
            loadout.build[position] = '0';
    uint16 offender = 0;
    BuildError const error = Validate(data, loadout.build, uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)),
        offender);
    if (error != BuildError::None)
        return SendError(player, error, offender);
    ApplyLoadoutBuild(player, data, state, &loadout);
}

void HandleMessage(Player* player, uint32 language, std::string const& message)
{
    if (language != LANG_ADDON || !message.starts_with(Prefix))
        return;

    std::string_view body(message);
    body.remove_prefix(Prefix.size());
    if (body.empty() || body.front() != '\t')
        return;
    body.remove_prefix(1);

    ClassTrees const* data = GetTrees(player->getClass());
    TalentTreeState* state = GetState(player);
    if (!data || !state)
        return SendError(player, BuildError::Unavailable, 0);

    if (body == "OPEN")
    {
        SendState(player, false);
        return SendLoadouts(player, state);
    }

    constexpr std::string_view loadoutSave = "LSAVE\t";
    if (body.starts_with(loadoutSave))
        return HandleLoadoutSave(player, *data, state, body.substr(loadoutSave.size()));
    constexpr std::string_view loadoutDelete = "LDEL\t";
    if (body.starts_with(loadoutDelete))
        return HandleLoadoutDelete(player, state, body.substr(loadoutDelete.size()));
    constexpr std::string_view buildApply = "LBUILD\t";
    if (body.starts_with(buildApply))
        return HandleBuildApply(player, *data, state, body.substr(buildApply.size()));
    constexpr std::string_view loadoutApply = "LAPPLY\t";
    if (body.starts_with(loadoutApply))
        return HandleLoadoutApply(player, *data, state, body.substr(loadoutApply.size()));

    constexpr std::string_view apply = "APPLY\t";
    if (body.starts_with(apply))
        return HandleApply(player, *data, state, body.substr(apply.size()));

    constexpr std::string_view specialization = "SPEC\t";
    if (body.starts_with(specialization))
        HandleSpecialization(player, *data, state, body.substr(specialization.size()));
}

void LoadTrees()
{
    Classes.clear();
    ClassHasTrees.fill(false);

    QueryResult trees = WorldDatabase.Query(
        "SELECT `ClassId`, `TreeId`, `FirstLevel`, `LevelStep`, `Gate1Row`, `Gate1Cost`, `Gate2Row`, `Gate2Cost`, "
        "`Signature`, `Kind`, `SpecSpells`, `BotOrder`, `Role` FROM `custom_talent_tree` ORDER BY `ClassId`, `TreeId`");
    if (!trees)
    {
        LOG_INFO("server.loading", ">> No talent trees (localTools/talentTree/buildTalentTree.py writes them)");
        return;
    }

    do
    {
        Field* field = trees->Fetch();
        ClassTrees& data = Classes[field[0].Get<uint8>()];
        Tree tree;
        tree.id = field[1].Get<uint8>();
        tree.firstLevel = std::max<uint8>(1, field[2].Get<uint8>());
        tree.levelStep = std::max<uint8>(1, field[3].Get<uint8>());
        tree.gates[0] = { field[4].Get<uint8>(), field[5].Get<uint8>() };
        tree.gates[1] = { field[6].Get<uint8>(), field[7].Get<uint8>() };
        data.signature = field[8].Get<uint32>();
        tree.spec = field[9].Get<uint8>() == 1;
        tree.role = field[12].Get<uint8>();
        for (std::string_view token : Acore::Tokenize(field[10].Get<std::string_view>(), ',', false))
            if (Optional<uint32> spellId = Acore::StringTo<uint32>(token))
                tree.specSpells.push_back(*spellId);
        for (std::string_view token : Acore::Tokenize(field[11].Get<std::string_view>(), ',', false))
        {
            std::vector<std::string_view> const parts = Acore::Tokenize(token, ':', false);
            Optional<uint16> const node = parts.empty() ? Optional<uint16>() : Acore::StringTo<uint16>(parts[0]);
            if (!node)
                continue;
            BotPick pick;
            pick.node = *node;
            if (parts.size() > 1)
                pick.option = Acore::StringTo<uint8>(parts[1]).value_or(1);
            tree.botOrder.push_back(pick);
        }
        if (tree.spec)
            data.specTrees.push_back(tree.id);
        data.trees.push_back(tree);
    } while (trees->NextRow());

    ClassHasTrees.fill(false);
    for (auto const& [classId, data] : Classes)
        if (classId < ClassHasTrees.size())
            ClassHasTrees[classId] = true;

    // The recommended builds on their own: a database the generated SQL has not reached yet keeps its trees
    if (QueryResult presets = WorldDatabase.Query(
            "SELECT `ClassId`, `TreeId`, `SingleBuild`, `AoeBuild`, `TankBuild` FROM `custom_talent_tree`"))
        do
        {
            Field* field = presets->Fetch();
            auto const data = Classes.find(field[0].Get<uint8>());
            if (data == Classes.end())
                continue;
            auto const tree = std::find_if(data->second.trees.begin(), data->second.trees.end(),
                [treeId = field[1].Get<uint8>()](Tree const& candidate) { return candidate.id == treeId; });
            if (tree == data->second.trees.end())
                continue;

            for (auto [column, preset] : { std::make_pair(2, &tree->singleBuild), std::make_pair(3, &tree->aoeBuild),
                     std::make_pair(4, &tree->tankBuild) })
                for (std::string_view token : Acore::Tokenize(field[column].Get<std::string_view>(), ',', false))
                {
                    std::vector<std::string_view> const parts = Acore::Tokenize(token, ':', false);
                    Optional<uint16> const node = parts.size() == 2 ? Acore::StringTo<uint16>(parts[0]) : std::nullopt;
                    Optional<uint8> const value = parts.size() == 2 ? Acore::StringTo<uint8>(parts[1]) : std::nullopt;
                    if (node && value && *value > 0 && *value <= 9)
                        preset->push_back({ *node, *value });
                }
        } while (presets->NextRow());

    QueryResult nodes = WorldDatabase.Query(
        // `Row` is a reserved word in MySQL 8: every column is quoted
        "SELECT `ClassId`, `NodeId`, `TreeId`, `Row`, `Kind`, `MinLevel`, `Spells`, `Parents` "
        "FROM `custom_talent_node` ORDER BY `ClassId`, `Position`");
    if (nodes)
        do
        {
            Field* field = nodes->Fetch();
            auto data = Classes.find(field[0].Get<uint8>());
            if (data == Classes.end())
                continue;

            TreeNode node;
            node.id = field[1].Get<uint16>();
            node.tree = field[2].Get<uint8>();
            node.row = field[3].Get<uint8>();
            node.kind = NodeKind(field[4].Get<uint8>());
            node.minLevel = field[5].Get<uint8>();
            for (std::string_view token : Acore::Tokenize(field[6].Get<std::string_view>(), ',', false))
                if (Optional<uint32> spellId = Acore::StringTo<uint32>(token))
                    node.spells.push_back(*spellId);
            for (std::string_view token : Acore::Tokenize(field[7].Get<std::string_view>(), ',', false))
                if (Optional<uint16> parent = Acore::StringTo<uint16>(token))
                    node.parents.push_back(*parent);

            if (node.spells.empty() || node.spells.size() > 9)
            {
                LOG_ERROR("sql.sql", "custom_talent_node {} has {} spells (1 to 9)", node.id, node.spells.size());
                continue;
            }
            data->second.positions[node.id] = data->second.nodes.size();
            data->second.nodes.push_back(std::move(node));
        } while (nodes->NextRow());

    std::size_t total = 0;
    for (auto const& [classId, data] : Classes)
        total += data.nodes.size();
    LOG_INFO("server.loading", ">> Loaded talent trees for {} class(es), {} nodes", Classes.size(), total);
}

// The trees load with the custom tables, before the spells: their spells are checked once the world is up
void CheckTreeSpells()
{
    for (auto const& [classId, data] : Classes)
        for (TreeNode const& node : data.nodes)
            for (uint32 spellId : node.spells)
                if (!sSpellMgr->GetSpellInfo(spellId))
                    LOG_ERROR("sql.sql", "custom_talent_node {} (class {}) names spell {}, which does not exist",
                        node.id, classId, spellId);
}

class TalentTreeWorldScript final : public WorldScript
{
public:
    TalentTreeWorldScript() : WorldScript("TalentTreeWorldScript", {
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnLoadCustomDatabaseTable() override
    {
        LoadTrees();
    }

    void OnStartup() override
    {
        CheckTreeSpells();
    }
};

class TalentTreePlayerScript final : public PlayerScript
{
public:
    TalentTreePlayerScript() : PlayerScript("TalentTreePlayerScript", {
        PLAYERHOOK_ON_CALCULATE_TALENTS_POINTS,
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED,
        PLAYERHOOK_ON_MAP_CHANGED,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_UPDATE
    }) { }

    // A stale WotLK talent aura dropped every StaleTalentCheckMs, each character at a moment of its own (its guid
    // shifts the period), without a timer to keep: a class without trees stops at the first test
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        uint8 const classId = player->getClass();
        if (classId >= ClassHasTrees.size() || !ClassHasTrees[classId] || !player->IsInWorld())
            return;
        uint64 const now = uint64(GameTime::GetGameTimeMS().count()) + player->GetGUID().GetCounter() * 97;
        if (now / StaleTalentCheckMs == (now - std::min<uint64>(diff, now)) / StaleTalentCheckMs)
            return;
        DropStaleTalentAuras(player);
    }

    // A class with trees spends its points there; the WotLK talent window never has any to give it
    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& talentPointsForLevel) override
    {
        if (GetTrees(player->getClass()))
            talentPointsForLevel = 0;
    }

    void OnPlayerLogin(Player* player) override
    {
        ClassTrees const* data = GetTrees(player->getClass());
        if (!data)
            return;

        int8 const stockTab = DominantStockTab(player);
        bool const stockTank = WasStockTank(player);
        WipeStockTalents(player);
        LoadForPlayer(player, *data);
        if (IsBot(player))
        {
            ChooseBotSpecialization(player, *data, GetState(player), stockTab);
            FillBotBuild(player, *data, GetState(player), stockTank);
        }
        else
            KeepStockSpecialization(player, *data, GetState(player), stockTab);
        TrimAndReconcile(player, *data, GetState(player));
        SendState(player, false);
    }

    // Down a level (a prestige, a GM) can leave a build worth more points than the level gives; up a level brings
    // a point, which the window wants to know about
    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        ClassTrees const* data = GetTrees(player->getClass());
        TalentTreeState* state = GetState(player);
        if (!data || !state)
            return;

        if (IsBot(player))
            FillBotBuild(player, *data, state);
        TrimAndReconcile(player, *data, state);
        SendState(player, false);
    }

    // Dual specialization: each slot keeps its own build, and switching puts the other one's spells in place
    void OnPlayerAfterSpecSlotChanged(Player* player, uint8 /*newSlot*/) override
    {
        ClassTrees const* data = GetTrees(player->getClass());
        TalentTreeState* state = GetState(player);
        if (!data || !state)
            return;

        WipeStockTalents(player);
        if (IsBot(player))
            FillBotBuild(player, *data, state);
        ReconcileActive(player, *data, state);
        SendState(player, false);
    }

    // A bot entering a five-man dungeon takes its AoE build, a raid its single-target build, and its usual one back
    // as it leaves. At login the map
    // comes before the build is loaded: OnPlayerLogin fills it then.
    void OnPlayerMapChanged(Player* player) override
    {
        if (!IsBot(player))
            return;
        ClassTrees const* data = GetTrees(player->getClass());
        TalentTreeState* state = GetState(player);
        if (!data || !state)
            return;

        if (FillBotBuild(player, *data, state))
            ReconcileActive(player, *data, state);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
                                       std::string& message) override
    {
        HandleMessage(player, language, message);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute("DELETE FROM character_talent_tree WHERE guid = {}", guid.GetCounter());
        CharacterDatabase.Execute("DELETE FROM character_talent_loadout WHERE guid = {}", guid.GetCounter());
    }
};
}

// A player's specialization's way of fighting, from its spec tree: 1 melee damage, 2 caster damage, 3 healer, 4 tank;
// 0 for a class without trees. The stock checks (Player::HasHealSpec, HasCasterSpec) read the WotLK talent tabs, which
// a class on the trees no longer has: a Holy priest or an Elemental shaman was neither there.
uint8 GetTalentSpecRole(Player* player)
{
    ClassTrees const* data = player ? GetTrees(player->getClass()) : nullptr;
    TalentTreeState* state = GetState(player);
    if (!data || !state)
        return 0;
    Tree const* tree = FindTree(*data, Specialization(*data, state->specializations[ActiveSlot(player)]));
    return tree && tree->spec ? tree->role : 0;
}

// For the classes' own scripts and the bots (TalentTree.h): a player's specialization, its spec tree id; 0 for a
// class without trees
uint8 GetTalentSpecialization(Player* player)
{
    ClassTrees const* data = player ? GetTrees(player->getClass()) : nullptr;
    TalentTreeState* state = GetState(player);
    return data && state ? Specialization(*data, state->specializations[ActiveSlot(player)]) : 0;
}

// The window's SPEC without the window (a bot choosing its role)
void SetTalentSpecialization(Player* player, uint8 tree)
{
    ClassTrees const* data = player ? GetTrees(player->getClass()) : nullptr;
    TalentTreeState* state = GetState(player);
    if (!data || !state || std::find(data->specTrees.begin(), data->specTrees.end(), tree) == data->specTrees.end())
        return;
    if (IsBot(player))
    {
        uint8 const slot = ActiveSlot(player);
        state->specializations[slot] = tree;
        FillBotBuild(player, *data, state);
        SaveBuild(player, *data, state, slot);
        ReconcileActive(player, *data, state);
        return;
    }
    ChangeSpecialization(player, *data, state, tree);
    SendState(player, false);
}

// Whether a class has talent trees at all (it then has no WotLK talents)
bool HasTalentTrees(uint8 classId)
{
    return GetTrees(classId) != nullptr;
}

// The specialization as an index among the class's spec trees (0 for the first), -1 for a class without trees. The
// Mage's spec trees come in the order of its WotLK talent tabs, so this is the tab the bots' strategies expect.
int8 GetTalentSpecializationIndex(Player* player)
{
    ClassTrees const* data = player ? GetTrees(player->getClass()) : nullptr;
    if (!data || data->specTrees.empty())
        return -1;
    uint8 const tree = GetTalentSpecialization(player);
    auto const itr = std::find(data->specTrees.begin(), data->specTrees.end(), tree);
    return itr == data->specTrees.end() ? 0 : int8(itr - data->specTrees.begin());
}

void SetTalentSpecializationIndex(Player* player, uint8 index)
{
    ClassTrees const* data = player ? GetTrees(player->getClass()) : nullptr;
    if (data && index < data->specTrees.size())
        SetTalentSpecialization(player, data->specTrees[index]);
}

// The combat bench (mod-playerbots Script/CombatBench.cpp) and the bots' factory: a bot of a class on the trees takes
// the specialization of that index (-1 keeps its own) and its recommended build - 1 the single-target one, 2 the AoE
// one, 0 back to what it takes by itself (the AoE build in a dungeon), 3 the tank one of a tree that has one - and
// keeps it wherever it goes (the tank one through the build itself: back to 0, a tank stays a tank; 1 or 2 makes it
// a damage dealer again). Returns false for a class without trees, or a player who is not a bot.
bool SetTalentBuildPreset(Player* player, int8 index, uint8 preset)
{
    ClassTrees const* data = player ? GetTrees(player->getClass()) : nullptr;
    TalentTreeState* state = GetState(player);
    if (!data || !state || !IsBot(player))
        return false;

    bool const tank = preset == uint8(BuildPreset::Tank);
    if (preset == uint8(BuildPreset::Auto) || tank)
        player->CustomData.Erase(PresetKey);
    else
        player->CustomData.GetDefault<ForcedPreset>(PresetKey)->preset =
            preset == uint8(BuildPreset::Aoe) ? BuildPreset::Aoe : BuildPreset::Single;

    uint8 const slot = ActiveSlot(player);
    if (index >= 0 && std::size_t(index) < data->specTrees.size())
        state->specializations[slot] = data->specTrees[index];
    FillBotBuild(player, *data, state, tank);
    SaveBuild(player, *data, state, slot);
    ReconcileActive(player, *data, state);
    return true;
}

void AddTalentTreeScripts()
{
    new TalentTreeWorldScript();
    new TalentTreePlayerScript();
}
