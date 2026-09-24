#include "Chat.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Retail-style talent trees for the custom classes that have one (`talentTree` in
// localTools/customClasses/classes.json; the Oathblade first). Such a class gets no WotLK talent points: a class tree
// and a spec tree take their place, each with its own points, and the window drawing them is FrameXML/TalentTree.lua.
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
// Addon whispers, prefix "TalentTree":
//   client -> server  OPEN                    the state, please
//                     APPLY <spec> <build>    make this the active build
//                     SPEC <tree>             make this spec tree the active slot's specialization
//   server -> client  S <signature> <active spec> <spec count> <level> <applied> <build 1> <build 2>
//                       <specialization 1> <specialization 2>
//                     E <error> <node>
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
    NoSpec      = 9     // a specialization this class does not have
};

struct TreeGate
{
    uint8 row = 0;
    uint8 cost = 0;
};

struct Tree
{
    uint8 id = 0;
    bool spec = false;              // a spec tree (else the class tree)
    std::vector<uint32> specSpells; // learned while this spec tree is the chosen specialization
    uint8 firstLevel = 10;
    uint8 levelStep = 2;
    std::array<TreeGate, 2> gates{};
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

// A character's builds, one per talent spec slot (dual specialization). Kept on the player, so it dies with them.
struct TalentTreeState : public DataMap::Base
{
    std::array<std::string, MAX_TALENT_SPECS> builds;
    std::array<uint8, MAX_TALENT_SPECS> specializations{};     // spec tree id per slot, 0 for the first
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

uint32 PointsAt(Tree const& tree, uint8 level)
{
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

// Makes the character's spells match a build: each node's current rank (or chosen option) learned, every other
// spell of the trees removed, so a lower rank, a refunded node or the other option never lingers. Only the class tree
// and the chosen spec tree count; the other spec trees keep their picks in the build, unlearned.
void Reconcile(Player* player, ClassTrees const& data, std::string const& build, uint8 specialization)
{
    // The chosen tree's spells last: one spell granted by two trees stays learned
    for (bool const wanted : { false, true })
        for (Tree const& tree : data.trees)
            if (tree.spec && (tree.id == specialization) == wanted)
                for (uint32 spellId : tree.specSpells)
                    SetSpell(player, spellId, wanted);

    for (std::size_t position = 0; position < data.nodes.size(); ++position)
    {
        TreeNode const& node = data.nodes[position];
        Tree const* tree = FindTree(data, node.tree);
        bool const counts = !tree || !tree->spec || tree->id == specialization;
        uint8 const value = !counts || node.minLevel > player->GetLevel() ? 0 : Value(build, position);
        for (std::size_t index = 0; index < node.spells.size(); ++index)
            SetSpell(player, node.spells[index], std::size_t(value) == index + 1);
    }
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
        return SendState(player, false);

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

    QueryResult trees = WorldDatabase.Query(
        "SELECT `ClassId`, `TreeId`, `FirstLevel`, `LevelStep`, `Gate1Row`, `Gate1Cost`, `Gate2Row`, `Gate2Cost`, "
        "`Signature`, `Kind`, `SpecSpells` FROM `custom_talent_tree` ORDER BY `ClassId`, `TreeId`");
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
        for (std::string_view token : Acore::Tokenize(field[10].Get<std::string_view>(), ',', false))
            if (Optional<uint32> spellId = Acore::StringTo<uint32>(token))
                tree.specSpells.push_back(*spellId);
        if (tree.spec)
            data.specTrees.push_back(tree.id);
        data.trees.push_back(tree);
    } while (trees->NextRow());

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
                {
                    if (!sSpellMgr->GetSpellInfo(*spellId))
                        LOG_ERROR("sql.sql", "custom_talent_node {} names spell {}, which does not exist",
                            node.id, *spellId);
                    node.spells.push_back(*spellId);
                }
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

class TalentTreeWorldScript final : public WorldScript
{
public:
    TalentTreeWorldScript() : WorldScript("TalentTreeWorldScript", {
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE
    }) { }

    void OnLoadCustomDatabaseTable() override
    {
        LoadTrees();
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
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_ON_DELETE
    }) { }

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

        LoadForPlayer(player, *data);
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

        ReconcileActive(player, *data, state);
        SendState(player, false);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
                                       std::string& message) override
    {
        HandleMessage(player, language, message);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute("DELETE FROM character_talent_tree WHERE guid = {}", guid.GetCounter());
    }
};
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
    ChangeSpecialization(player, *data, state, tree);
    SendState(player, false);
}

void AddTalentTreeScripts()
{
    new TalentTreeWorldScript();
    new TalentTreePlayerScript();
}
