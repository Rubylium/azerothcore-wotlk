#include "ParagonSystem.h"

#include "Chat.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "CharacterDatabase.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Item.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Mail.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "MythicDungeonSystem.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StatGrowthConfig.h"
#include "StatGrowthSystem.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// mod-custom-classes TalentTree.cpp: a specialization's way of fighting, for a class on the talent trees (0 otherwise)
uint8 GetTalentSpecRole(Player* player);

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
    ThreatPct,          // `value`% more threat generated, always (mod-stat-growth's tank aura carries it)
    Grudge,             // `value`% of the damage taken lately as attack and spell power, up to `value2`% of max health
    // The caster side's own procs. Spells alone set them off - a cast, or a spell's damage - so a melee character
    // gains nothing from walking there, as a caster gains nothing from the melee side's weapon procs.
    Echo,               // a spell's critical strike: `chance` to deal `value`% of it again
    Arc,                // a spell's damage: `chance` to arc `value`% of it to `value2` enemies nearby, on `cooldown`
    Quicken,            // casting a spell: `chance` to cast `value`% faster for `duration`
    Insight,            // casting a spell: `chance` to gain `value` spell power for `duration`
    Ward,               // casting a spell: `chance` to absorb `value`% of spell power for `duration`, on `cooldown`
    ManaSurge,          // casting a spell: `chance` to restore `value`% of maximum mana, on `cooldown`
    // The Pantheon's and the glyphs'. A socket holds a glyph; the rest are what the Blessings and the glyph bonuses add
    // beside the effects above, each through a cap of its own.
    Socket,             // holds a glyph: the allocated nodes within GlyphRadius links grow by the glyph's share
    HealPct,            // `value`% more healing done by the character's spells, up to MaxHealPct
    HealShare,          // `value`% of the healing received also goes to the most hurt ally near, up to MaxHealSharePct
    AreaReach,          // the splash and the arc reach `value` more enemies, up to MaxAreaTargets
    ExecuteReach,       // the execute nodes apply `value`% of health higher, up to MaxExecuteThreshold
    Count
};

// What sets a node off, from its branch: the melee branches answer to weapon attacks and abilities only, the caster
// branches to spells only, and the armour branch, the hub and the bridges to anything.
enum class ParagonScope : uint8
{
    Any = 0,
    Weapon,
    Spell,
    Count
};

// What a hit was, as far as the board is concerned: a weapon's (white swings, and abilities of the melee or ranged
// damage class), a spell's (the magic damage class), or something else - a damage class of none, a script's damage
// - which only the unscoped nodes answer to.
enum class HitKind : uint8
{
    Other = 0,
    Weapon,
    Spell
};

bool Answers(uint8 scope, HitKind kind)
{
    switch (static_cast<ParagonScope>(scope))
    {
        case ParagonScope::Any: return true;
        case ParagonScope::Weapon: return kind == HitKind::Weapon;
        case ParagonScope::Spell: return kind == HitKind::Spell;
        default: return false;
    }
}

// The hub and the bridges have no side, and so no tier gate
constexpr uint8 NoSide = 255;
// Eveil, Ascension, Transcendance, and the Pantheon: a side's Pantheon opens once its Transcendance is complete
constexpr uint8 TierCount = 4;

// However the outer zones stack, a character can still be hurt and cannot heal off every hit in full. The reduction
// is low on purpose: it multiplies with armour, which a geared tank already has at WotLK's 75% cap, and at 75% on top
// of that a tank took a sixteenth of every hit. The tank nodes give threat and Rancune instead.
constexpr uint32 MaxReductionPct = 25;
constexpr uint32 MaxLeechPct = 50;
// The explosion scales with what died, not with the character: on a Mythic+ pack at a high key a mob has more health
// than a player deals in several seconds, and a few nodes of it summed turned every kill into a one-shot of the pack.
// It stays a small bonus; the nodes past it scale with the character's own damage instead (KillStreak, Splash).
constexpr uint32 MaxExplosionPct = 15;
constexpr uint32 MaxSplashTargets = 4;
// A reflected hit is a share of what the mob dealt, and a mob at a high key deals far more than any character does:
// uncapped, a tank sent a whole pull's damage back several times over. Each reflection is held to a share of the
// character's own maximum health, which grows with the character and not with the key.
constexpr uint32 MaxRetaliateHealthPct = 4;
// Double strike is one roll per hit, on the nodes' chances summed and capped here, and strikes at most once: each node
// used to roll on its own, and four of them with the Titan's 150% came to three quarters more on every hit, 40% of a
// tank's damage. Capped, the strikes are at most a fifth of what a character deals.
constexpr float MaxDoubleStrikeChance = 25.0f;
// The area procs are a chance, not a certainty: a blast on every kill and a splash on every hit made a pack of trash
// melt on its own. Each is one roll on the nodes' chances summed, capped here.
constexpr float MaxExplosionChance = 40.0f;
constexpr float MaxKillStreakChance = 75.0f;
constexpr float MaxSplashChance = 35.0f;
constexpr float MaxArcChance = 35.0f;
constexpr float ArcRange = 10.0f;
// The echo of a spell's critical strike is the caster's double strike: summed and capped the same way, and only a
// critical strike can echo, so the cap sits higher than a weapon's
constexpr float MaxEchoChance = 60.0f;
constexpr uint32 MaxQuickenPct = 30;

// The Pantheon's and the glyphs' guardrails. An area hit of the board's reaches at most MaxAreaTargets enemies, and
// past AreaFalloffFrom each one takes sqrt(AreaFalloffFrom / n) of its share, so a pack of twenty is barely worse off
// than one of twelve. The execute bonuses add up to MaxExecutePct at most, below MaxExecuteThreshold% of health at
// most: one more hit beside the one that set them off, never a multiplier on it.
constexpr uint32 MaxAreaTargets = 12;
constexpr uint32 AreaFalloffFrom = 5;
constexpr uint32 MaxExecutePct = 60;
constexpr uint32 MaxExecuteThreshold = 40;
constexpr uint32 MaxHealPct = 20;
constexpr uint32 MaxHealSharePct = 25;
constexpr float HealShareRange = 30.0f;

// Glyphs: a socketed glyph raises every allocated node within GlyphRadius links by GlyphBasePct% of its values, and
// GlyphPctPerLevel% more a level, up to GlyphMaxLevel (+60%). A node within reach of two sockets takes the larger.
// Experience comes only while socketed: GlyphXpPerLevel times the level to reach the next one (9 000 from 1 to 25).
constexpr uint32 GlyphRadius = 3;
constexpr uint32 GlyphMaxLevel = 25;
constexpr uint32 GlyphBasePct = 10;
constexpr uint32 GlyphPctPerLevel = 2;
constexpr uint32 GlyphXpPerLevel = 30;
// What earns it: a key of +10 and up (GlyphKeyXp, GlyphKeyXpPerLevel more a level past +10), a floor of the Infinite
// Dungeon's gearing ladder from GlyphFloorFrom on (GlyphFloorXp, one more every GlyphFloorXpStep floors), a heroic raid
// boss (GlyphRaidBossXp), and a copy of a glyph already known (GlyphDuplicateXp)
constexpr uint32 GlyphKeyFrom = 10;
constexpr uint32 GlyphKeyXp = 40;
constexpr uint32 GlyphKeyXpPerLevel = 4;
constexpr uint32 GlyphFloorFrom = 50;
constexpr uint32 GlyphFloorXp = 10;
constexpr uint32 GlyphFloorXpStep = 5;
constexpr uint32 GlyphRaidBossXp = 60;
constexpr uint32 GlyphDuplicateXp = 300;
// Drops: a finished key's chest, GlyphKeyChance% at +10 and GlyphKeyChancePerLevel% more a level up to
// GlyphKeyMaxChance% (+30); a sure one every GlyphFloorEvery floors past GlyphFloorFrom; a heroic raid boss,
// GlyphRaidBossChance%. A glyph the character does not have yet comes first.
constexpr float GlyphKeyChance = 10.0f;
constexpr float GlyphKeyChancePerLevel = 1.25f;
constexpr float GlyphKeyMaxChance = 35.0f;
constexpr uint32 GlyphFloorEvery = 10;
constexpr float GlyphRaidBossChance = 15.0f;

// Paragon levels, earned from experience at the level cap. Each level is a point. The bar grows each level, and
// compounds (ParagonXpGrowth a level), because the experience itself grows with the character (essences raise its
// rate): the first few come quickly, the fiftieth costs about 9 times the first, the hundredth about 40 times.
constexpr uint32 ParagonXpBase = 150000;
constexpr uint32 ParagonXpPerLevel = 7500;
constexpr double ParagonXpGrowth = 1.02;

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
constexpr uint32 SPELL_PARAGON_GRUDGE = 90663;
// The caster side's
constexpr uint32 SPELL_PARAGON_ECHO = 90666;
constexpr uint32 SPELL_PARAGON_ARC = 90667;
constexpr uint32 SPELL_PARAGON_QUICKEN = 90668;
constexpr uint32 SPELL_PARAGON_WARD = 90669;
constexpr uint32 SPELL_PARAGON_INSIGHT = 90670;
constexpr uint32 SPELL_PARAGON_MANA_SURGE = 90671;

// Rancune: what was taken fades by half every GrudgeHalfLifeMs, so "lately" means the last several seconds
constexpr uint32 GrudgeTickMs = 500;
constexpr double GrudgeHalfLifeMs = 5000.0;

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
    uint8 cost = 1;             // points it takes: 1 for a small node, up to 5 for an Apotheosis
    uint8 side = NoSide;        // its branch
    uint8 tier = 0;             // its zone: 0 Eveil, 1 Ascension, 2 Transcendance, 3 Pantheon
    uint8 scope = 0;            // ParagonScope: what sets it off
    uint8 sigil = 0;            // the Pantheon sigil it belongs to, 0 for none
};

// A paragon glyph: the item it is, its branch, and its bonus - an effect like a node's - given once `need` allocated
// nodes of its branch are within its radius
struct ParagonGlyph
{
    uint32 id = 0;
    uint32 item = 0;
    uint8 side = 0;
    uint8 need = 0;
    ParagonNode bonus;
    std::string name;
};

// A proc a character currently owns, lifted out of the board so a damage event does not have to walk every
// allocated node. Rebuilt whenever the allocation changes.
struct ParagonProc
{
    ParagonEffect effect = ParagonEffect::Stat;
    uint8 scope = 0;
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
    uint8 scope = 0;
    uint32 expiresAt = 0;
    int32 applied = 0;
};

std::unordered_map<uint32, ParagonNode> Board;
std::unordered_map<uint32, std::vector<uint32>> Adjacency;
// Every node of a side's tier, by side * TierCount + tier: what has to be held before the next tier of that side opens
std::unordered_map<uint32, std::vector<uint32>> TierNodes;
uint32 BoardSignature = 0;
// The Pantheon's sigils: their stars, and the Blessing taking all of them grants (an effect like a node's)
std::unordered_map<uint8, std::vector<uint32>> SigilNodes;
std::unordered_map<uint8, ParagonNode> Blessings;
std::unordered_map<uint8, std::string> BlessingNames;
// The glyphs, by id and by item; and each socket's reach, the nodes within GlyphRadius links of it
std::map<uint32, ParagonGlyph> Glyphs;
std::unordered_map<uint32, uint32> GlyphByItem;
std::unordered_map<uint32, std::vector<uint32>> SocketReach;

// Summed per scope: [any, weapon, spell]. A hit counts the unscoped share and the share of its own kind.
using ScopedPct = std::array<uint32, static_cast<std::size_t>(ParagonScope::Count)>;

uint32 ScopedSum(ScopedPct const& values, HitKind kind)
{
    uint32 total = values[0];
    if (kind == HitKind::Weapon)
        total += values[static_cast<std::size_t>(ParagonScope::Weapon)];
    else if (kind == HitKind::Spell)
        total += values[static_cast<std::size_t>(ParagonScope::Spell)];
    return total;
}

// One effect fed by several nodes answers to what they all answer to; nodes of different scopes feeding the same one
// (which the board does not do) fall back to anything.
uint8 MergeScope(uint8 current, uint8 incoming, bool first)
{
    return first || current == incoming ? incoming : static_cast<uint8>(ParagonScope::Any);
}

// A character's own board. Kept on the player so it dies with the session.
struct ParagonState : public DataMap::Base
{
    uint32 earned = 0;                      // everything ever awarded, including past the cap
    uint32 prestige = 0;                    // how many times this character has reset; each one raises the cap
    std::unordered_set<uint32> allocated;   // paid-for nodes only; free ones are never stored
    bool applied = false;
    bool overCapReset = false;              // the allocation broke the board's rules at login and was refunded
    std::vector<ParagonProc> procs;         // from the allocated nodes, rebuilt when they change
    std::vector<ParagonBuff> buffs;         // currently running

    // The always-on effects of the outer zones, summed from the allocation with the procs
    ScopedPct damagePct{};
    uint32 reductionPct = 0;
    ScopedPct leechPct{};
    uint32 healthPct = 0;
    uint32 explosionPct = 0;
    uint32 explosionRange = 0;
    float explosionChance = 0.0f;
    uint8 explosionScope = 0;
    uint32 killStreakPct = 0;               // per stack, summed over the nodes
    uint32 killStreakMax = 0;               // the most stacks any node allows
    uint32 killStreakDuration = 0;
    float killStreakChance = 0.0f;
    uint8 killStreakScope = 0;
    uint32 splashPct = 0;
    uint32 splashRange = 0;
    float splashChance = 0.0f;
    uint32 splashCooldown = 0;
    uint32 splashReadyAt = 0;
    uint8 splashScope = 0;
    uint32 threatPct = 0;
    uint32 grudgePct = 0;
    uint32 grudgeCapPct = 0;

    // The caster side, summed the same way: one roll each, however many nodes feed it
    float echoChance = 0.0f;
    uint32 echoPct = 0;                     // the strongest echo held
    float arcChance = 0.0f;
    uint32 arcPct = 0;
    uint32 arcTargets = 0;
    uint32 arcCooldown = 0;
    uint32 arcReadyAt = 0;
    float quickenChance = 0.0f;
    uint32 quickenPct = 0;
    uint32 quickenDuration = 0;
    float insightChance = 0.0f;
    uint32 insightValue = 0;
    uint32 insightDuration = 0;
    float wardChance = 0.0f;
    uint32 wardPct = 0;
    uint32 wardDuration = 0;
    uint32 wardCooldown = 0;
    uint32 wardReadyAt = 0;
    float manaChance = 0.0f;
    uint32 manaPct = 0;
    uint32 manaCooldown = 0;
    uint32 manaReadyAt = 0;

    // The Pantheon's and the glyphs' own, summed the same way
    uint32 healPct = 0;
    uint32 healSharePct = 0;
    uint32 areaReach = 0;
    uint32 executeReach = 0;

    // The glyphs this character knows, by id: level, experience towards the next, and the socket holding it (0: none)
    struct GlyphState
    {
        uint32 level = 1;
        uint32 experience = 0;
        uint32 socket = 0;
    };
    std::map<uint32, GlyphState> glyphs;
    // What the socketed glyphs do to the board, worked out by ComputeGlyphLayer: each node within reach and its share
    // (percent), and the glyphs whose condition holds. `layer` is what was handed out for the flat stats and armour,
    // so taking it back is exact.
    std::unordered_map<uint32, uint32> nodeBoost;
    std::vector<uint32> activeGlyphs;
    std::unordered_map<uint32, int32> layer;

    // What the character last hit, and with what, so a kill knows whether a weapon or a spell made it
    ObjectGuid lastHitTarget;
    HitKind lastHitKind = HitKind::Other;

    // Rancune running: the damage taken lately, fading, and the attack and spell power it has handed out
    double grudgePool = 0.0;
    uint32 grudgeApplied = 0;
    uint32 grudgeTickAt = 0;

    // The kill streak currently running: one counter rather than a buff per kill
    uint32 killStreakStacks = 0;
    uint32 killStreakExpiresAt = 0;

    uint32 level = 0;                       // paragon level: experience earned at the level cap
    uint32 experience = 0;                  // towards the next level

    // A bot's board: planned from the content it is in (UpdateBotParagon), never saved, never sent, never earned
    bool bot = false;
    uint32 botBudget = 0;                   // the points the plan was built for
    uint8 botRole = 0;                      // BotRole the plan was built for

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
    // Heals the board owes (Freya's share), dealt on the next update like the hits
    struct PendingHeal
    {
        ObjectGuid target;
        uint32 amount = 0;
    };
    std::vector<PendingHeal> pendingHeals;
};

// Set while the board deals its own damage, so that damage is neither boosted by the board nor sets off more procs,
// and a kill it makes does not explode again: one pull of trash would otherwise chain through the instance.
// Per thread, because maps update on several.
thread_local bool DealingProcDamage = false;
// The same for the board's own heals: a shared heal is not shared again
thread_local bool DealingProcHeal = false;

// The damage hook (UnitScript::OnDamage) is not told what dealt the hit. The hooks just before it are: the final
// damage of a melee swing or of a spell (ModifyFinalDamage) and a periodic tick (ModifyPeriodicDamageAurasTick) name
// the spell, or none for a white swing, and the same thread then goes on to deal that damage. It is noted here, and
// read and cleared by the damage hook; a hit it does not match - other units, or damage dealt with no hook before
// it - is Other.
struct DamageSource
{
    Unit const* attacker = nullptr;
    Unit const* victim = nullptr;
    SpellInfo const* spell = nullptr;
    bool periodic = false;
    bool set = false;
};
thread_local DamageSource PendingSource;

HitKind KindOf(SpellInfo const* spell)
{
    if (!spell)
        return HitKind::Weapon;             // a white swing
    switch (spell->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
            return HitKind::Weapon;
        case SPELL_DAMAGE_CLASS_MAGIC:
            return HitKind::Spell;
        default:
            return HitKind::Other;
    }
}

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
        case ParagonEffect::Quicken: return SPELL_PARAGON_QUICKEN;
        case ParagonEffect::Insight: return SPELL_PARAGON_INSIGHT;
        case ParagonEffect::Ward: return SPELL_PARAGON_WARD;
        default: return 0;
    }
}

uint32 ExperienceForLevel(uint32 level)
{
    double const cost = (double(ParagonXpBase) + double(ParagonXpPerLevel) * level) *
        std::pow(ParagonXpGrowth, double(level));
    return static_cast<uint32>(std::min(cost, 2000000000.0));
}

// Whether anything on the damage path has work to do for this character
bool HasCombatEffects(ParagonState const* state)
{
    if (!state || !state->applied)
        return false;
    for (std::size_t scope = 0; scope < state->damagePct.size(); ++scope)
        if (state->damagePct[scope] || state->leechPct[scope])
            return true;
    // A running buff counts too: a ward from a cast has to soak hits on a board with nothing else on the damage path
    return !state->procs.empty() || !state->buffs.empty() || state->reductionPct || state->explosionPct
        || state->killStreakPct || state->splashPct || state->grudgePct || state->echoPct || state->arcPct;
}

// Whether casting has anything to set off
bool HasCastEffects(ParagonState const* state)
{
    return state && state->applied && (state->quickenPct || state->insightValue || state->wardPct || state->manaPct);
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

// What a node costs. Free nodes cost nothing; a node the data leaves at 0 still costs a point.
uint32 NodeCost(ParagonNode const& node)
{
    return node.free ? 0 : std::max<uint32>(1, node.cost);
}

uint32 SpentOn(std::unordered_set<uint32> const& allocated)
{
    uint32 total = 0;
    for (uint32 nodeId : allocated)
        if (auto const node = Board.find(nodeId); node != Board.end())
            total += NodeCost(node->second);
    return total;
}

// Points spent: the nodes' costs, not their count
uint32 SpentPoints(ParagonState const* state)
{
    return state ? SpentOn(state->allocated) : 0;
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

// A side's tier opens once every node of the tier before it on that side is held: the whole of a branch's Eveil before
// its Ascension, the whole of its Ascension before its Transcendance. Returns how many are still missing (0: open).
// The hub, the bridges and the first tier are never gated.
uint32 MissingForTier(std::unordered_set<uint32> const& allocated, ParagonNode const& node)
{
    if (node.side == NoSide || node.tier == 0)
        return 0;

    auto const previous = TierNodes.find(uint32(node.side) * TierCount + node.tier - 1);
    if (previous == TierNodes.end())
        return 0;

    uint32 missing = 0;
    for (uint32 nodeId : previous->second)
        if (!allocated.count(nodeId))
            ++missing;
    return missing;
}

// Whether an allocation keeps the board's rules as they stand now: within the cap at the nodes' costs, and nothing
// held behind a tier gate that is still shut. Checked at login, when the rules may have changed since it was built.
bool IsAllocationValid(ParagonState const* state)
{
    if (SpentOn(state->allocated) > PointCap(state))
        return false;

    for (uint32 nodeId : state->allocated)
        if (auto const node = Board.find(nodeId);
            node != Board.end() && MissingForTier(state->allocated, node->second))
            return false;
    return true;
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

// Whether a glyph's share applies to what a node's `value` is: the stats and the percentages that make a character
// stronger. Never the chances, the thresholds, the mitigation or the survival nodes: those the caps hold tight.
bool IsBoostable(ParagonEffect effect)
{
    switch (effect)
    {
        case ParagonEffect::Stat:
        case ParagonEffect::Armor:
        case ParagonEffect::ArmorPct:
        case ParagonEffect::GuardOnHit:
        case ParagonEffect::RetaliateOnHit:
        case ParagonEffect::FuryOnHit:
        case ParagonEffect::SurgeOnKill:
        case ParagonEffect::DamagePct:
        case ParagonEffect::Leech:
        case ParagonEffect::Execute:
        case ParagonEffect::HealthPct:
        case ParagonEffect::KillStreak:
        case ParagonEffect::Splash:
        case ParagonEffect::ThreatPct:
        case ParagonEffect::Grudge:
        case ParagonEffect::Echo:
        case ParagonEffect::Arc:
        case ParagonEffect::Insight:
        case ParagonEffect::Ward:
            return true;
        default:
            return false;
    }
}

// A node's value with its glyph's share, if a socketed glyph reaches it
uint32 BoostedValue(ParagonState const* state, ParagonNode const& node)
{
    auto const boost = state->nodeBoost.find(node.id);
    if (boost == state->nodeBoost.end() || !IsBoostable(static_cast<ParagonEffect>(node.effect)))
        return node.value;
    return node.value + node.value * boost->second / 100;
}

// Every star of a sigil held: its Blessing is granted
bool IsSigilComplete(std::unordered_set<uint32> const& allocated, uint8 sigil)
{
    auto const stars = SigilNodes.find(sigil);
    if (stars == SigilNodes.end() || stars->second.empty())
        return false;
    return std::all_of(stars->second.begin(), stars->second.end(),
        [&allocated](uint32 nodeId) { return allocated.count(nodeId) > 0; });
}

// One effect into the character's sums, or its procs: a node's, a Blessing's or a glyph bonus's, all the same way, so
// all of them meet the same caps
void FeedEffect(ParagonState* state, ParagonNode const& node, uint32 value)
{
    ParagonEffect const effect = static_cast<ParagonEffect>(node.effect);
    std::size_t const scope = std::min<std::size_t>(node.scope, state->damagePct.size() - 1);
    switch (effect)
    {
        case ParagonEffect::Stat:
        case ParagonEffect::Armor:
        case ParagonEffect::ArmorPct:
        case ParagonEffect::Socket:
            return;
        case ParagonEffect::DamagePct:
            state->damagePct[scope] += value;
            return;
        case ParagonEffect::ReductionPct:
            state->reductionPct += value;
            return;
        case ParagonEffect::Leech:
            state->leechPct[scope] += value;
            return;
        case ParagonEffect::HealthPct:
            state->healthPct += value;
            return;
        case ParagonEffect::Explosion:
            // One roll per kill, however many nodes feed it: the percentages and the chances add up, the widest
            // reach wins
            state->explosionScope = MergeScope(state->explosionScope, node.scope, !state->explosionPct);
            state->explosionPct += value;
            state->explosionRange = std::max(state->explosionRange, node.value2);
            state->explosionChance += node.chance;
            return;
        case ParagonEffect::KillStreak:
            // One streak, however many nodes feed it: each stack is worth their sum, the longest one wins
            state->killStreakScope = MergeScope(state->killStreakScope, node.scope, !state->killStreakPct);
            state->killStreakPct += value;
            state->killStreakMax = std::max(state->killStreakMax, node.value2);
            state->killStreakDuration = std::max(state->killStreakDuration, node.duration);
            state->killStreakChance += node.chance;
            return;
        case ParagonEffect::Splash:
            state->splashScope = MergeScope(state->splashScope, node.scope, !state->splashPct);
            state->splashPct += value;
            state->splashRange = std::max(state->splashRange, node.value2);
            state->splashChance += node.chance;
            state->splashCooldown = std::max(state->splashCooldown, node.cooldown);
            return;
        case ParagonEffect::ThreatPct:
            state->threatPct += value;
            return;
        case ParagonEffect::Grudge:
            state->grudgePct += value;
            state->grudgeCapPct = std::max(state->grudgeCapPct, node.value2);
            return;
        // The caster side: each is one roll however many nodes feed it. The chances add up where the effect is a
        // hit of its own (echo, arc); where it is a buff the best chance and the longest duration win and the
        // amounts add up, so a second node makes the same proc stronger rather than a second one to track.
        case ParagonEffect::Echo:
            state->echoChance += node.chance;
            state->echoPct = std::max(state->echoPct, value);
            return;
        case ParagonEffect::Arc:
            state->arcChance += node.chance;
            state->arcPct += value;
            state->arcTargets = std::max(state->arcTargets, node.value2);
            state->arcCooldown = std::max(state->arcCooldown, node.cooldown);
            return;
        case ParagonEffect::Quicken:
            state->quickenChance = std::max(state->quickenChance, node.chance);
            state->quickenPct += value;
            state->quickenDuration = std::max(state->quickenDuration, node.duration);
            return;
        case ParagonEffect::Insight:
            state->insightChance = std::max(state->insightChance, node.chance);
            state->insightValue += value;
            state->insightDuration = std::max(state->insightDuration, node.duration);
            return;
        case ParagonEffect::Ward:
            state->wardChance = std::max(state->wardChance, node.chance);
            state->wardPct += value;
            state->wardDuration = std::max(state->wardDuration, node.duration);
            state->wardCooldown = std::max(state->wardCooldown, node.cooldown);
            return;
        case ParagonEffect::ManaSurge:
            state->manaChance = std::max(state->manaChance, node.chance);
            state->manaPct += value;
            state->manaCooldown = std::max(state->manaCooldown, node.cooldown);
            return;
        case ParagonEffect::HealPct:
            state->healPct += value;
            return;
        case ParagonEffect::HealShare:
            state->healSharePct += value;
            return;
        case ParagonEffect::AreaReach:
            state->areaReach += value;
            return;
        case ParagonEffect::ExecuteReach:
            state->executeReach += value;
            return;
        default:
            break;
    }

    ParagonProc proc;
    proc.effect = effect;
    proc.scope = node.scope;
    proc.value = value;
    proc.value2 = node.value2;
    proc.chance = node.chance;
    proc.duration = node.duration;
    proc.cooldown = node.cooldown;
    state->procs.push_back(proc);
}

// The procs from whatever is currently allocated, the Blessings of the sigils wholly taken and the bonuses of the
// glyphs whose condition holds. Walking the whole allocation on every damage event would be wasteful, and damage
// events are the hottest path this module has.
void RebuildProcs(ParagonState* state)
{
    if (!state)
        return;

    state->procs.clear();
    state->damagePct.fill(0);
    state->leechPct.fill(0);
    state->reductionPct = state->healthPct = 0;
    state->explosionPct = state->explosionRange = 0;
    state->explosionChance = 0.0f;
    state->killStreakPct = state->killStreakMax = state->killStreakDuration = 0;
    state->killStreakChance = 0.0f;
    state->splashPct = state->splashRange = state->splashCooldown = 0;
    state->splashChance = 0.0f;
    state->threatPct = state->grudgePct = state->grudgeCapPct = 0;
    state->echoChance = state->arcChance = state->quickenChance = state->insightChance = 0.0f;
    state->wardChance = state->manaChance = 0.0f;
    state->echoPct = state->arcPct = state->arcTargets = state->arcCooldown = 0;
    state->quickenPct = state->quickenDuration = state->insightValue = state->insightDuration = 0;
    state->wardPct = state->wardDuration = state->wardCooldown = 0;
    state->manaPct = state->manaCooldown = 0;
    state->healPct = state->healSharePct = state->areaReach = state->executeReach = 0;

    for (uint32 nodeId : state->allocated)
        if (auto const entry = Board.find(nodeId); entry != Board.end())
            FeedEffect(state, entry->second, BoostedValue(state, entry->second));

    for (auto const& [sigil, blessing] : Blessings)
        if (IsSigilComplete(state->allocated, sigil))
            FeedEffect(state, blessing, blessing.value);

    for (uint32 glyphId : state->activeGlyphs)
        if (auto const glyph = Glyphs.find(glyphId); glyph != Glyphs.end())
            FeedEffect(state, glyph->second.bonus, glyph->second.bonus.value);
}

// A glyph's share of the nodes it reaches, in percent, at its level
uint32 GlyphShare(uint32 level)
{
    return GlyphBasePct + GlyphPctPerLevel * std::min(level, GlyphMaxLevel);
}

uint32 GlyphExperienceFor(uint32 level)
{
    return GlyphXpPerLevel * std::max<uint32>(level, 1);
}

// What the socketed glyphs do: every allocated node within reach of one takes its share (the larger, where two reach
// it), and a glyph whose reach holds `need` allocated nodes of its own branch gives its bonus. A glyph counts only in a
// socket that is itself allocated.
void ComputeGlyphLayer(ParagonState* state)
{
    state->nodeBoost.clear();
    state->activeGlyphs.clear();
    for (auto const& [glyphId, glyph] : state->glyphs)
    {
        if (!glyph.socket || !state->allocated.count(glyph.socket))
            continue;
        auto const definition = Glyphs.find(glyphId);
        auto const reach = SocketReach.find(glyph.socket);
        if (definition == Glyphs.end() || reach == SocketReach.end())
            continue;

        uint32 const share = GlyphShare(glyph.level);
        uint32 own = 0;
        for (uint32 nodeId : reach->second)
        {
            if (!state->allocated.count(nodeId))
                continue;
            uint32& boost = state->nodeBoost[nodeId];
            boost = std::max(boost, share);
            if (Board.at(nodeId).side == definition->second.side)
                ++own;
        }
        if (own >= definition->second.need)
            state->activeGlyphs.push_back(glyphId);
    }
}

// The glyphs' share of the flat stats and the armour, handed out as amounts and kept, so taking it back is exact. The
// percentages' share goes through RebuildProcs instead.
void AddGlyphLayer(Player* player, ParagonState* state)
{
    for (auto const& [nodeId, share] : state->nodeBoost)
    {
        ParagonNode const& node = Board.at(nodeId);
        int32 amount = 0;
        switch (static_cast<ParagonEffect>(node.effect))
        {
            case ParagonEffect::Stat:
                if (node.stat >= static_cast<uint8>(PermanentStat::Count))
                    continue;
                amount = static_cast<int32>(node.value * share / 100);
                if (amount)
                    ApplyPermanentStat(player, static_cast<PermanentStat>(node.stat), uint32(amount), true);
                break;
            case ParagonEffect::Armor:
                amount = static_cast<int32>(node.value * share / 100);
                ApplyArmor(player, amount, true);
                break;
            case ParagonEffect::ArmorPct:
                amount = FlatArmorFor(player, node.value) * static_cast<int32>(share) / 100;
                ApplyArmor(player, amount, true);
                break;
            default:
                continue;
        }
        if (amount)
            state->layer[nodeId] = amount;
    }
}

void RemoveGlyphLayer(Player* player, ParagonState* state)
{
    for (auto const& [nodeId, amount] : state->layer)
    {
        auto const node = Board.find(nodeId);
        if (node == Board.end())
            continue;
        if (node->second.effect == static_cast<uint8>(ParagonEffect::Stat))
            ApplyPermanentStat(player, static_cast<PermanentStat>(node->second.stat), uint32(amount), false);
        else
            ApplyArmor(player, amount, false);
    }
    state->layer.clear();
}

// After anything that moves what the glyphs reach - a node taken, a glyph socketed or levelled, a reset - the layer is
// taken off, worked out again and put back, and the procs rebuilt with it
void RefreshGlyphs(Player* player, ParagonState* state)
{
    bool const live = state->applied;
    if (live)
        RemoveGlyphLayer(player, state);
    ComputeGlyphLayer(state);
    if (live)
        AddGlyphLayer(player, state);
    RebuildProcs(state);
    if (live)
        player->UpdateMaxHealth();
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
    buff.scope = proc.scope;
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

// A kill's surge of power answers to what made the kill: a weapon's gives attack power, a spell's spell power
void ApplySurge(Player* player, uint8 scope, uint32 amount, bool apply)
{
    if (!amount)
        return;
    if (scope != static_cast<uint8>(ParagonScope::Spell))
        ApplyPermanentStat(player, PermanentStat::AttackPower, amount, apply);
    if (scope != static_cast<uint8>(ParagonScope::Weapon))
        ApplyPermanentStat(player, PermanentStat::SpellPower, amount, apply);
}

// Takes back what a running proc handed out, and its marker
void EndBuff(Player* player, ParagonBuff const& buff)
{
    switch (buff.effect)
    {
        case ParagonEffect::GuardOnHit:
            ApplyArmor(player, buff.applied, false);
            break;
        case ParagonEffect::SurgeOnKill:
            ApplySurge(player, buff.scope, static_cast<uint32>(buff.applied), false);
            break;
        case ParagonEffect::Quicken:
            if (buff.applied > 0)
                player->ApplyCastTimePercentMod(static_cast<float>(buff.applied), false);
            break;
        case ParagonEffect::Insight:
            if (buff.applied > 0)
                ApplyPermanentStat(player, PermanentStat::SpellPower, static_cast<uint32>(buff.applied), false);
            break;
        default:
            break;
    }
    if (uint32 const marker = BuffSpell(buff.effect))
        player->RemoveAurasDueToSpell(marker);
}

void ClearBuffs(Player* player, ParagonState* state)
{
    for (ParagonBuff const& buff : state->buffs)
        EndBuff(player, buff);
    state->buffs.clear();
    state->pending.clear();
    state->pendingHeals.clear();
    if (state->killStreakStacks)
        player->RemoveAurasDueToSpell(SPELL_PARAGON_KILL_STREAK);
    state->killStreakStacks = state->killStreakExpiresAt = 0;
    if (state->grudgeApplied)
    {
        ApplyPermanentStat(player, PermanentStat::AttackPower, state->grudgeApplied, false);
        ApplyPermanentStat(player, PermanentStat::SpellPower, state->grudgeApplied, false);
        player->RemoveAurasDueToSpell(SPELL_PARAGON_GRUDGE);
    }
    state->grudgePool = 0.0;
    state->grudgeApplied = 0;
}

void SaveEarned(Player* player, ParagonState const* state)
{
    CharacterDatabase.Execute(
        "REPLACE INTO character_paragon_points (guid, earned, prestige) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), state->earned, state->prestige);
}

void SaveGlyph(Player* player, uint32 glyphId, ParagonState::GlyphState const& glyph)
{
    CharacterDatabase.Execute(
        "REPLACE INTO character_paragon_glyph (guid, glyph, level, experience, socket) VALUES ({}, {}, {}, {}, {})",
        player->GetGUID().GetCounter(), glyphId, glyph.level, glyph.experience, glyph.socket);
}

// A glyph as the frame reads it: id:level:experience:needed:socket (needed is 0 at the top level)
std::string GlyphText(uint32 glyphId, ParagonState::GlyphState const& glyph)
{
    return Acore::StringFormat("{}:{}:{}:{}:{}", glyphId, glyph.level, glyph.experience,
        glyph.level >= GlyphMaxLevel ? 0 : GlyphExperienceFor(glyph.level), glyph.socket);
}

void SendGlyph(Player* player, uint32 glyphId, ParagonState::GlyphState const& glyph)
{
    Send(player, "GLYPH\t" + GlyphText(glyphId, glyph));
}

// The character's whole collection, chunked like the nodes
void SendGlyphs(Player* player, ParagonState const* state)
{
    Send(player, "GLYPHBEGIN");
    std::string chunk;
    for (auto const& [glyphId, glyph] : state->glyphs)
    {
        if (chunk.size() > 180)
        {
            Send(player, "GLYPHS\t" + chunk);
            chunk.clear();
        }
        if (!chunk.empty())
            chunk += ",";
        chunk += GlyphText(glyphId, glyph);
    }
    if (!chunk.empty())
        Send(player, "GLYPHS\t" + chunk);
    Send(player, "GLYPHEND");
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

    SendGlyphs(player, state);
    Send(player, Acore::StringFormat("PXP\t{}\t{}\t{}", state->level, state->experience,
        ExperienceForLevel(state->level)));
    Send(player, "DONE");
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

// ---------------------------------------------------------------------------------------------------------
// Bots
//
// A bot owns a real board, with the same nodes, stats and procs a player's would give, planned rather than bought:
// the content it is in decides how many points (a key's or a challenge tier's recommended paragon, else what the
// group's real players have spent), its role decides where they go. It is never saved and never sent anywhere.
// ---------------------------------------------------------------------------------------------------------

enum class BotRole : uint8
{
    None = 0,
    Tank,
    Strength,       // a fighter on strength: warriors, death knights, paladins, the custom fighters
    Agility,        // a fighter on agility: rogues, hunters, feral druids, enhancement shamans
    Caster,
    Healer
};

// The branches, by side (buildParagonTree.py BRANCHES)
constexpr uint8 SideForce = 0;
constexpr uint8 SidePuissance = 1;
constexpr uint8 SideAgilite = 2;
constexpr uint8 SideCarapace = 3;
constexpr uint8 SideArcanes = 4;
constexpr uint8 SideIntellect = 5;

// How often a bot's content, group and role are looked at again
constexpr int32 BotCheckMs = 3000;

// The instances whose content asks for more paragon than the key level says: the challenge board's tiers
// (mod-playerbots RaidFinder.cpp registers them). Maps update on several threads: the registry is locked.
std::mutex InstanceBudgetLock;
std::unordered_map<uint32, uint32> InstanceBudgets;

// The walks already planned, by role and budget: the plan is deterministic, so each is worked out once
std::mutex BotPlanLock;
std::unordered_map<uint64, std::vector<uint32>> BotPlans;

struct BotParagonTimer : public DataMap::Base
{
    int32 left = 0;
};
constexpr char const* BotTimerKey = "ParagonBotTimer";

// The branches a role walks, first to last. A branch is finished zone by zone (the gates ask for it anyway) before
// the next is started.
std::array<uint8, 3> SidesFor(BotRole role)
{
    switch (role)
    {
        case BotRole::Tank: return { SideCarapace, SideAgilite, SideForce };
        case BotRole::Agility: return { SideAgilite, SidePuissance, SideForce };
        case BotRole::Caster: return { SideArcanes, SideIntellect, SideCarapace };
        case BotRole::Healer: return { SideIntellect, SideArcanes, SideCarapace };
        case BotRole::Strength:
        default:
            return { SideForce, SidePuissance, SideAgilite };
    }
}

// What an effect is worth to a role, against a plain stat: the walk reaches for what the role uses first when the
// points do not cover a whole zone. Everything of a zone is taken in the end; only the order changes.
float Usefulness(BotRole role, ParagonEffect effect)
{
    constexpr float Wanted = 3.0f;
    constexpr float Neutral = 1.0f;
    constexpr float Unwanted = 0.5f;

    switch (effect)
    {
        case ParagonEffect::Stat:
        case ParagonEffect::Armor:
            return Neutral;
        case ParagonEffect::ThreatPct:
            return role == BotRole::Tank ? Wanted : Unwanted;
        case ParagonEffect::ArmorPct:
        case ParagonEffect::GuardOnHit:
        case ParagonEffect::LastStand:
        case ParagonEffect::Undying:
        case ParagonEffect::Grudge:
        case ParagonEffect::RetaliateOnHit:
            return role == BotRole::Tank ? Wanted : Neutral;
        case ParagonEffect::HealthPct:
        case ParagonEffect::ReductionPct:
            return role == BotRole::Tank || role == BotRole::Healer ? Wanted : Neutral;
        case ParagonEffect::Leech:
        case ParagonEffect::DamagePct:
        case ParagonEffect::Execute:
            return role == BotRole::Healer ? Neutral : Wanted;
        case ParagonEffect::FuryOnHit:
        case ParagonEffect::SurgeOnKill:
        case ParagonEffect::DoubleStrike:
        case ParagonEffect::Explosion:
        case ParagonEffect::KillStreak:
        case ParagonEffect::Splash:
            return role == BotRole::Strength || role == BotRole::Agility ? Wanted : Neutral;
        case ParagonEffect::Echo:
        case ParagonEffect::Arc:
            return role == BotRole::Caster ? Wanted : Neutral;
        case ParagonEffect::Quicken:
        case ParagonEffect::Insight:
        case ParagonEffect::ManaSurge:
            return role == BotRole::Caster || role == BotRole::Healer ? Wanted : Neutral;
        case ParagonEffect::Ward:
        case ParagonEffect::HealPct:
        case ParagonEffect::HealShare:
            return role == BotRole::Healer ? Wanted : Neutral;
        case ParagonEffect::AreaReach:
        case ParagonEffect::ExecuteReach:
            return role == BotRole::Healer || role == BotRole::Tank ? Neutral : Wanted;
        default:
            return Neutral;
    }
}

// A plain notable is a bigger stat than its points' worth of minor nodes (30 for 2 against 8 a point in Eveil)
constexpr uint8 NodeTypeNotable = 1;
constexpr float PlainNotableWorth = 1.5f;

float NodeWeight(BotRole role, ParagonNode const& node)
{
    ParagonEffect const effect = static_cast<ParagonEffect>(node.effect);
    bool const plain = effect == ParagonEffect::Stat || effect == ParagonEffect::Armor;
    float const worth = plain && node.type == NodeTypeNotable ? PlainNotableWorth : Usefulness(role, effect);
    return float(NodeCost(node)) * worth;
}

bool IsHeld(std::unordered_set<uint32> const& held, uint32 nodeId)
{
    auto const node = Board.find(nodeId);
    return node != Board.end() && (node->second.free || held.count(nodeId));
}

// Spends as much of `budget` as it can on the nodes `inPhase` accepts, under the player's rules: each node next to
// one already held, its zone's gate open, its spent requirement met, the points there. Each step walks to the target
// worth the most per point spent getting there - the path's nodes' weights over their costs - so a keystone a few
// minor nodes away is reached before the minor nodes around the hub are all bought.
template <typename Accept>
void WalkPhase(BotRole role, uint32 budget, std::unordered_set<uint32>& held, std::vector<uint32>& order,
    uint32& spent, Accept const& inPhase)
{
    std::vector<uint32> candidates;
    for (auto const& [nodeId, node] : Board)
        if (!node.free && !held.count(nodeId) && inPhase(node))
            candidates.push_back(nodeId);
    // Deterministic whatever order the board's map is in
    std::sort(candidates.begin(), candidates.end());
    if (candidates.empty())
        return;

    // Every node of a phase shares its side and zone (the bridges have no gate): one look tells whether it is open
    if (MissingForTier(held, Board.at(candidates.front())))
        return;

    std::unordered_set<uint32> const inside(candidates.begin(), candidates.end());

    struct Step
    {
        uint32 cost = std::numeric_limits<uint32>::max();
        float weight = 0.0f;
        uint32 previous = 0;
        bool settled = false;
    };

    while (spent < budget)
    {
        // The cheapest way to each node of the phase from what is held, through the phase's own nodes
        std::unordered_map<uint32, Step> steps;
        for (uint32 nodeId : candidates)
        {
            if (held.count(nodeId))
                continue;
            auto const links = Adjacency.find(nodeId);
            if (links == Adjacency.end())
                continue;
            for (uint32 neighbour : links->second)
                if (IsHeld(held, neighbour))
                {
                    ParagonNode const& node = Board.at(nodeId);
                    steps[nodeId] = { NodeCost(node), NodeWeight(role, node), 0, false };
                    break;
                }
        }

        while (true)
        {
            uint32 current = 0;
            Step* from = nullptr;
            for (auto& [nodeId, step] : steps)
                if (!step.settled &&
                    (!from || step.cost < from->cost || (step.cost == from->cost && nodeId < current)))
                {
                    current = nodeId;
                    from = &step;
                }
            if (!from)
                break;

            from->settled = true;
            uint32 const fromCost = from->cost;
            float const fromWeight = from->weight;
            auto const links = Adjacency.find(current);
            if (links == Adjacency.end())
                continue;
            for (uint32 neighbour : links->second)
            {
                if (!inside.count(neighbour) || held.count(neighbour))
                    continue;
                ParagonNode const& node = Board.at(neighbour);
                uint32 const cost = fromCost + NodeCost(node);
                float const weight = fromWeight + NodeWeight(role, node);
                Step& to = steps[neighbour];
                if (to.settled)
                    continue;
                if (cost < to.cost || (cost == to.cost && weight > to.weight))
                    to = { cost, weight, current, false };
            }
        }

        // The best target the points reach, whose path keeps every node's spent requirement
        uint32 const left = budget - spent;
        uint32 target = 0;
        float bestScore = 0.0f;
        std::vector<uint32> bestPath;
        for (uint32 nodeId : candidates)
        {
            auto const step = steps.find(nodeId);
            if (step == steps.end() || !step->second.settled || step->second.cost > left)
                continue;

            float const score = step->second.weight / float(step->second.cost);
            if (target && (score < bestScore || (score == bestScore &&
                (step->second.cost > steps[target].cost ||
                (step->second.cost == steps[target].cost && nodeId > target)))))
                continue;

            std::vector<uint32> path;
            for (uint32 at = nodeId; at; at = steps[at].previous)
                path.push_back(at);
            std::reverse(path.begin(), path.end());

            uint32 before = spent;
            bool legal = true;
            for (uint32 at : path)
            {
                ParagonNode const& node = Board.at(at);
                if (before < node.required)
                {
                    legal = false;
                    break;
                }
                before += NodeCost(node);
            }
            if (!legal)
                continue;

            target = nodeId;
            bestScore = score;
            bestPath = std::move(path);
        }

        if (!target)
            return;

        for (uint32 nodeId : bestPath)
        {
            held.insert(nodeId);
            order.push_back(nodeId);
            spent += NodeCost(Board.at(nodeId));
        }
    }
}

std::vector<uint32> PlanBotBoard(BotRole role, uint32 budget)
{
    std::unordered_set<uint32> held;
    std::vector<uint32> order;
    uint32 spent = 0;

    std::array<uint8, 3> const sides = SidesFor(role);
    for (std::size_t index = 0; index < sides.size() && spent < budget; ++index)
    {
        uint8 const side = sides[index];
        for (uint8 tier = 0; tier < TierCount && spent < budget; ++tier)
            WalkPhase(role, budget, held, order, spent,
                [side, tier](ParagonNode const& node) { return node.side == side && node.tier == tier; });

        // The first branch done, the bridges before the next branch: they are open to anyone next to them
        if (index == 0 && spent < budget)
            WalkPhase(role, budget, held, order, spent,
                [](ParagonNode const& node) { return node.side == NoSide; });
    }

    // Whatever is left, anywhere the rules allow: the remaining branches, zone by zone
    for (uint8 tier = 0; tier < TierCount && spent < budget; ++tier)
        for (uint8 side = 0; side <= SideIntellect && spent < budget; ++side)
            WalkPhase(role, budget, held, order, spent,
                [side, tier](ParagonNode const& node) { return node.side == side && node.tier == tier; });

    return order;
}

// A bot's glyphs are sized by its content like its board: level 1 from BotGlyphFromBudget points (a +18 key), one level
// more every BotGlyphBudgetPerLevel (+30: 8, +40: 14, the Pantheon's keys past +55: 25)
constexpr uint32 BotGlyphFromBudget = 40;
constexpr uint32 BotGlyphBudgetPerLevel = 8;

uint32 BotGlyphLevel(uint32 budget)
{
    if (budget <= BotGlyphFromBudget)
        return 1;
    return std::min<uint32>(GlyphMaxLevel, 1 + (budget - BotGlyphFromBudget) / BotGlyphBudgetPerLevel);
}

// One glyph in each socket the bot's plan holds: of the socket's own branch, the one whose bonus its role wants most
void PlanBotGlyphs(ParagonState* state, BotRole role, uint32 budget)
{
    state->glyphs.clear();
    std::vector<uint32> sockets;
    for (uint32 nodeId : state->allocated)
        if (auto const node = Board.find(nodeId);
            node != Board.end() && node->second.effect == static_cast<uint8>(ParagonEffect::Socket))
            sockets.push_back(nodeId);
    std::sort(sockets.begin(), sockets.end());

    uint32 const level = BotGlyphLevel(budget);
    for (uint32 socket : sockets)
    {
        uint8 const side = Board.at(socket).side;
        uint32 best = 0;
        float bestWorth = 0.0f;
        for (auto const& [glyphId, glyph] : Glyphs)
        {
            if (glyph.side != side || state->glyphs.count(glyphId))
                continue;
            float const worth = Usefulness(role, static_cast<ParagonEffect>(glyph.bonus.effect));
            if (!best || worth > bestWorth)
            {
                best = glyphId;
                bestWorth = worth;
            }
        }
        if (best)
            state->glyphs[best] = { level, 0, socket };
    }
}

std::vector<uint32> GetBotPlan(BotRole role, uint32 budget)
{
    uint64 const key = uint64(role) << 32 | budget;
    std::lock_guard<std::mutex> guard(BotPlanLock);
    auto itr = BotPlans.find(key);
    if (itr == BotPlans.end())
    {
        itr = BotPlans.emplace(key, PlanBotBoard(role, budget)).first;
        LOG_DEBUG("module", "Paragon: bot plan role={} budget={} nodes={} spent={}", uint32(role), budget,
            itr->second.size(), SpentOn(std::unordered_set<uint32>(itr->second.begin(), itr->second.end())));
    }
    return itr->second;
}

bool HasGroupRole(Player* player, uint8 role)
{
    if (sLFGMgr->GetRoles(player->GetGUID()) & role)
        return true;
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID())
                return (member.roles & role) != 0;
    return false;
}

constexpr uint8 SpecRoleMelee = 1;
constexpr uint8 SpecRoleCaster = 2;
constexpr uint8 SpecRoleHealer = 3;
constexpr uint8 SpecRoleTank = 4;

bool IsHealer(Player* player)
{
    uint8 const role = GetTalentSpecRole(player);
    return role ? role == SpecRoleHealer : player->HasHealSpec();
}

// Whether a character fights with its weapons - a melee or a hunter, not a caster or a healer spec - whatever its
// hits' damage class: the melee branches answer to all of such a character's damage, its spells too (a rogue's
// poisons, an enhancement shaman's imbues and shocks, a paladin's judgements). They answered to weapon hits alone, and
// at 650 points an enhancement shaman did half of what a fire mage did, a rogue two fifths.
bool FightsWithWeapons(Player* player)
{
    // A class on the talent trees says it from its specialization (mod-custom-classes GetTalentSpecRole)
    if (uint8 const role = GetTalentSpecRole(player))
        return role == SpecRoleMelee || role == SpecRoleTank;
    uint8 const classId = player->getClass();
    switch (classId == 10 ? uint8(CLASS_WARRIOR) : sObjectMgr->GetClassFormulaTemplate(classId))
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return false;
        case CLASS_DRUID:
        case CLASS_SHAMAN:
            return !player->HasCasterSpec() && !player->HasHealSpec();
        case CLASS_PALADIN:
            return !player->HasHealSpec();
        default:
            return true;
    }
}

BotRole RoleOf(Player* bot, BotRole previous)
{
    if (IsGroupTank(bot) || bot->HasTankSpec())
        return BotRole::Tank;
    // A class on the talent trees: its specialization's way of fighting (the stock checks do not see its specs)
    switch (GetTalentSpecRole(bot))
    {
        case SpecRoleTank:
            return BotRole::Tank;
        case SpecRoleHealer:
            return BotRole::Healer;
        case SpecRoleCaster:
            return BotRole::Caster;
        default:
            break;
    }
    // A feral druid tanks in bear form and leaves it between pulls: with no role set, the tank keeps its board
    if (previous == BotRole::Tank && bot->getClass() == CLASS_DRUID &&
        bot->GetSpec() == TALENT_TREE_DRUID_FERAL_COMBAT &&
        !HasGroupRole(bot, uint8(lfg::PLAYER_ROLE_DAMAGE | lfg::PLAYER_ROLE_HEALER)))
        return BotRole::Tank;
    if (HasGroupRole(bot, lfg::PLAYER_ROLE_HEALER) || bot->HasHealSpec())
        return BotRole::Healer;

    // A custom class fights as the class it is built on (mod-custom-classes); the Oathblade as a warrior, whatever
    // its formulas (SmartLootSystem.cpp)
    uint8 const classId = bot->getClass();
    switch (classId == 10 ? uint8(CLASS_WARRIOR) : sObjectMgr->GetClassFormulaTemplate(classId))
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return BotRole::Caster;
        case CLASS_ROGUE:
        case CLASS_HUNTER:
            return BotRole::Agility;
        case CLASS_DRUID:
        case CLASS_SHAMAN:
            return bot->HasCasterSpec() ? BotRole::Caster : BotRole::Agility;
        default:
            return BotRole::Strength;
    }
}

// The points a bot's board is worth where it is: the content's recommended paragon, else the average the group's
// real players have spent. Only players on the bot's own map are read: another map updates on another thread.
// The combat bench's budget for one of its bots (SetBotParagonBudgetOverride)
struct BotBudgetOverride : public DataMap::Base
{
    uint32 points = 0;
};
constexpr char const* BotBudgetOverrideKey = "ParagonBenchBudget";

uint32 BotBudget(Player* bot)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled) || Board.empty())
        return 0;

    if (BotBudgetOverride const* bench = bot->CustomData.Get<BotBudgetOverride>(BotBudgetOverrideKey))
        return bench->points;

    if (Map* map = bot->FindMap(); map && map->IsDungeon())
    {
        if (map->GetMythicLevel() > 0)
            if (uint32 const points = Mythic::GetRecommendedParagon(map->GetMythicLevel()))
                return points;

        std::lock_guard<std::mutex> guard(InstanceBudgetLock);
        if (auto const itr = InstanceBudgets.find(map->GetInstanceId()); itr != InstanceBudgets.end() && itr->second)
            return itr->second;
    }

    uint32 total = 0;
    uint32 counted = 0;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); member && member->GetSession() &&
                !member->GetSession()->IsBot() && member->IsInMap(bot))
            {
                total += SpentPoints(GetState(member));
                ++counted;
            }
    return counted ? total / counted : 0;
}

// Takes a bot's board off: every node's modifiers and whatever proc is running
void StripBotBoard(Player* bot, ParagonState* state)
{
    if (!state->applied)
        return;
    ClearBuffs(bot, state);
    RemoveGlyphLayer(bot, state);
    for (uint32 nodeId : state->allocated)
        if (auto const node = Board.find(nodeId); node != Board.end())
            ApplyNode(bot, node->second, false);
    state->allocated.clear();
    state->glyphs.clear();
    state->applied = false;
}

// Looks at the bot's content and role, and rebuilds its board if either moved. Never in combat: the board would be
// pulled from under a fight, and a proc running would end early.
// force: on a map change, whatever the combat flag it carried over (a bot leaving a key for a raid kept the key's
// board while it stayed in combat)
void RefreshBot(Player* bot, bool force = false)
{
    if ((bot->IsInCombat() && !force) || !bot->IsInWorld())
        return;

    uint32 const budget = BotBudget(bot);
    ParagonState* state = GetState(bot);
    if (!budget)
    {
        if (!state)
            return;
        StripBotBoard(bot, state);
        bot->CustomData.Erase(StateKey);
        bot->UpdateMaxHealth();
        return;
    }

    BotRole const role = RoleOf(bot, state ? static_cast<BotRole>(state->botRole) : BotRole::None);
    if (state && state->applied && state->botBudget == budget && state->botRole == uint8(role))
        return;

    if (!state)
    {
        state = bot->CustomData.GetDefault<ParagonState>(StateKey);
        state->bot = true;
    }
    StripBotBoard(bot, state);
    state->allocated.clear();

    std::vector<uint32> const plan = GetBotPlan(role, budget);
    state->allocated.insert(plan.begin(), plan.end());
    for (uint32 nodeId : plan)
        if (auto const node = Board.find(nodeId); node != Board.end())
            ApplyNode(bot, node->second, true);
    PlanBotGlyphs(state, role, budget);
    ComputeGlyphLayer(state);
    AddGlyphLayer(bot, state);
    RebuildProcs(state);
    state->applied = true;
    state->botBudget = budget;
    state->botRole = uint8(role);
    bot->UpdateMaxHealth();

    LOG_DEBUG("module", "Paragon: bot {} role={} budget={} spent={} procs={} glyphs={}", bot->GetName(), uint32(role),
        budget, SpentPoints(state), state->procs.size(), state->glyphs.size());
}

// The group's bots on the same map look again now: a real player's board just changed
void RefreshGroupBots(Player* player)
{
    if (Group* group = player->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); member && member->GetSession() &&
                member->GetSession()->IsBot() && member->IsInMap(player))
                RefreshBot(member);
}
}

void LoadParagonBoard()
{
    Board.clear();
    Adjacency.clear();
    TierNodes.clear();
    SigilNodes.clear();
    Blessings.clear();
    BlessingNames.clear();
    Glyphs.clear();
    GlyphByItem.clear();
    SocketReach.clear();
    BoardSignature = 0;
    {
        std::lock_guard<std::mutex> guard(BotPlanLock);
        BotPlans.clear();
    }

    QueryResult nodes = WorldDatabase.Query(
        "SELECT id, type, effect, stat, value, value2, chance, duration, cooldown, free, required, cost, side, tier, "
        "scope, sigil FROM paragon_node");
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
        node.cost = field[11].Get<uint8>();
        node.side = field[12].Get<uint8>();
        node.tier = field[13].Get<uint8>();
        node.scope = field[14].Get<uint8>();
        node.sigil = field[15].Get<uint8>();
        if (node.effect >= static_cast<uint8>(ParagonEffect::Count))
        {
            LOG_ERROR("sql.sql", "paragon_node {} has effect {}, which does not exist", node.id, node.effect);
            node.effect = static_cast<uint8>(ParagonEffect::Stat);
        }
        if (node.scope >= static_cast<uint8>(ParagonScope::Count))
        {
            LOG_ERROR("sql.sql", "paragon_node {} has scope {}, which does not exist", node.id, node.scope);
            node.scope = static_cast<uint8>(ParagonScope::Any);
        }
        if (node.tier >= TierCount)
        {
            LOG_ERROR("sql.sql", "paragon_node {} has tier {}, which does not exist", node.id, node.tier);
            node.tier = 0;
        }
        Board[node.id] = node;
        if (!node.free && node.side != NoSide)
            TierNodes[uint32(node.side) * TierCount + node.tier].push_back(node.id);
        if (node.sigil)
            SigilNodes[node.sigil].push_back(node.id);

        // Cheap order-independent signature, matched against the client's copy of the board
        BoardSignature += node.id * 31 + node.value * 7 + node.stat + node.effect * 3 + node.required * 5;
        BoardSignature += uint32(node.cost) * 11 + uint32(node.tier) * 19 + uint32(node.side) * 23 +
            uint32(node.scope) * 29 + uint32(node.sigil) * 37;
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

    // Each socket's reach: the nodes within GlyphRadius links of it, found once here rather than on every change
    for (auto const& [nodeId, node] : Board)
    {
        if (node.effect != static_cast<uint8>(ParagonEffect::Socket))
            continue;
        std::unordered_map<uint32, uint32> distance{ { nodeId, 0 } };
        std::vector<uint32> frontier{ nodeId };
        std::vector<uint32>& reach = SocketReach[nodeId];
        for (uint32 step = 1; step <= GlyphRadius; ++step)
        {
            std::vector<uint32> next;
            for (uint32 at : frontier)
                if (auto const around = Adjacency.find(at); around != Adjacency.end())
                    for (uint32 neighbour : around->second)
                        if (distance.emplace(neighbour, step).second)
                        {
                            next.push_back(neighbour);
                            reach.push_back(neighbour);
                        }
            frontier.swap(next);
        }
    }

    // The Blessings, by sigil: an effect like a node's
    if (QueryResult result = WorldDatabase.Query(
            "SELECT sigil, effect, value, value2, chance, duration, cooldown, scope, name FROM paragon_blessing"))
        do
        {
            Field* field = result->Fetch();
            ParagonNode blessing;
            uint8 const sigil = field[0].Get<uint8>();
            blessing.effect = field[1].Get<uint8>();
            blessing.value = field[2].Get<uint32>();
            blessing.value2 = field[3].Get<uint32>();
            blessing.chance = field[4].Get<float>();
            blessing.duration = field[5].Get<uint32>();
            blessing.cooldown = field[6].Get<uint32>();
            blessing.scope = std::min<uint8>(field[7].Get<uint8>(), uint8(ParagonScope::Count) - 1);
            if (blessing.effect >= static_cast<uint8>(ParagonEffect::Count) || !SigilNodes.count(sigil))
            {
                LOG_ERROR("sql.sql", "paragon_blessing {} names an effect or a sigil that does not exist", sigil);
                continue;
            }
            Blessings[sigil] = blessing;
            BlessingNames[sigil] = field[8].Get<std::string>();
        } while (result->NextRow());

    if (QueryResult result = WorldDatabase.Query(
            "SELECT id, item, side, need, effect, value, value2, chance, duration, cooldown, scope, name "
            "FROM paragon_glyph"))
        do
        {
            Field* field = result->Fetch();
            ParagonGlyph glyph;
            glyph.id = field[0].Get<uint8>();
            glyph.item = field[1].Get<uint32>();
            glyph.side = field[2].Get<uint8>();
            glyph.need = field[3].Get<uint8>();
            glyph.bonus.effect = field[4].Get<uint8>();
            glyph.bonus.value = field[5].Get<uint32>();
            glyph.bonus.value2 = field[6].Get<uint32>();
            glyph.bonus.chance = field[7].Get<float>();
            glyph.bonus.duration = field[8].Get<uint32>();
            glyph.bonus.cooldown = field[9].Get<uint32>();
            glyph.bonus.scope = std::min<uint8>(field[10].Get<uint8>(), uint8(ParagonScope::Count) - 1);
            glyph.name = field[11].Get<std::string>();
            // Loaded before the item templates are: the item is checked when one is handed out
            if (glyph.bonus.effect >= static_cast<uint8>(ParagonEffect::Count))
            {
                LOG_ERROR("sql.sql", "paragon_glyph {} has effect {}, which does not exist", glyph.id,
                    glyph.bonus.effect);
                continue;
            }
            GlyphByItem[glyph.item] = glyph.id;
            Glyphs[glyph.id] = std::move(glyph);
        } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} paragon nodes and {} links (signature {}), {} sigils, {} glyphs",
        Board.size(), links, BoardSignature, Blessings.size(), Glyphs.size());
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

    // An allocation the rules no longer allow - more spent than the cap at the nodes' costs, or a node behind a tier
    // gate that is shut: the whole board is handed back, to be spent again under the rules as they are. Nothing
    // earned is lost; what is past the cap stays banked for the next prestiges.
    if (!IsAllocationValid(state))
    {
        state->allocated.clear();
        state->overCapReset = true;
        CharacterDatabase.Execute("DELETE FROM character_paragon WHERE guid = {}", guid);
    }

    // The glyphs known. One in a socket the board no longer holds (a reset, a reshaped board) goes back to the
    // collection: it is never lost, only unset.
    state->glyphs.clear();
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT glyph, level, experience, socket FROM character_paragon_glyph WHERE guid = {}", guid))
        do
        {
            Field* field = result->Fetch();
            uint32 const glyphId = field[0].Get<uint8>();
            if (!Glyphs.count(glyphId))
                continue;
            ParagonState::GlyphState glyph;
            glyph.level = std::clamp<uint32>(field[1].Get<uint8>(), 1, GlyphMaxLevel);
            glyph.experience = field[2].Get<uint32>();
            glyph.socket = field[3].Get<uint32>();
            if (glyph.socket && (!state->allocated.count(glyph.socket) || !SocketReach.count(glyph.socket)))
            {
                glyph.socket = 0;
                CharacterDatabase.Execute(
                    "UPDATE character_paragon_glyph SET socket = 0 WHERE guid = {} AND glyph = {}", guid, glyphId);
            }
            state->glyphs[glyphId] = glyph;
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

    ComputeGlyphLayer(state);
    AddGlyphLayer(player, state);
    RebuildProcs(state);
    state->applied = true;
    player->UpdateMaxHealth();

    if (state->overCapReset)
    {
        state->overCapReset = false;
        ChatHandler(player->GetSession()).PSendSysMessage(IsFrench(player)
            ? "|cffa335ee[Parangon]|r Les règles du tableau ont changé (coût des nœuds, paliers à compléter par "
              "branche, plafond de {} points, +{} par prestige) : votre tableau a été réinitialisé gratuitement. Vos "
              "points gagnés restent acquis ; ceux au-delà du plafond attendent vos prochains prestiges."
            : "|cffa335ee[Paragon]|r The board's rules changed (node costs, tiers to complete per branch, a cap of {} "
              "points, +{} per prestige): your board was reset for free. Your earned points are kept; those past the "
              "cap wait for your next prestiges.", PointCap(state),
            statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointsPerPrestige));
    }
}

void RefreshBotParagon(Player* bot)
{
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;

    RefreshBot(bot, true);
    if (BotParagonTimer* timer = bot->CustomData.GetDefault<BotParagonTimer>(BotTimerKey))
        timer->left = BotCheckMs;
}

void UpdateBotParagon(Player* bot, uint32 diff)
{
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;

    BotParagonTimer* timer = bot->CustomData.GetDefault<BotParagonTimer>(BotTimerKey);
    timer->left -= static_cast<int32>(diff);
    if (timer->left > 0)
        return;
    timer->left = BotCheckMs;
    RefreshBot(bot);
}

void SetBotParagonBudgetOverride(Player* bot, uint32 points)
{
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;
    if (points)
        bot->CustomData.GetDefault<BotBudgetOverride>(BotBudgetOverrideKey)->points = points;
    else
        bot->CustomData.Erase(BotBudgetOverrideKey);
    RefreshBot(bot, true);
}

void SetParagonInstanceBudget(uint32 instanceId, uint32 points)
{
    std::lock_guard<std::mutex> guard(InstanceBudgetLock);
    if (points)
        InstanceBudgets[instanceId] = points;
    else
        InstanceBudgets.erase(instanceId);
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

// ---------------------------------------------------------------------------------------------------------
// Glyphs
//
// A glyph is an item until it is learnt: using one from the bags (or the frame's collection) puts it in the character's
// collection, or, for one already known, turns the copy into experience. A known glyph is set into an allocated socket
// from the frame, and gains experience only while it sits in one.
// ---------------------------------------------------------------------------------------------------------
namespace
{
std::string GlyphName(Player* player, ParagonGlyph const& glyph)
{
    if (!IsFrench(player))
        if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(glyph.item))
            return itemTemplate->Name1;
    return glyph.name;
}

// Experience for one glyph; true when it went up a level
bool GainGlyphExperience(Player* player, ParagonState* state, uint32 glyphId, uint32 amount)
{
    auto const glyph = state->glyphs.find(glyphId);
    auto const definition = Glyphs.find(glyphId);
    if (glyph == state->glyphs.end() || definition == Glyphs.end() || glyph->second.level >= GlyphMaxLevel)
        return false;

    ParagonState::GlyphState& known = glyph->second;
    known.experience += amount;
    bool levelled = false;
    while (known.level < GlyphMaxLevel && known.experience >= GlyphExperienceFor(known.level))
    {
        known.experience -= GlyphExperienceFor(known.level);
        ++known.level;
        levelled = true;
    }
    if (known.level >= GlyphMaxLevel)
        known.experience = 0;

    SaveGlyph(player, glyphId, known);
    SendGlyph(player, glyphId, known);
    if (levelled)
        ChatHandler(player->GetSession()).PSendSysMessage(IsFrench(player)
            ? "|cffff8000{}|r atteint le niveau {} : +{}% aux nœuds à sa portée."
            : "|cffff8000{}|r reaches level {}: +{}% to the nodes within its reach.",
            GlyphName(player, definition->second), known.level, GlyphShare(known.level));
    return levelled;
}

// Every socketed glyph gains the same experience
void AddGlyphExperience(Player* player, uint32 amount)
{
    if (!player || !amount || !player->GetSession() || player->GetSession()->IsBot())
        return;
    ParagonState* state = GetState(player);
    if (!state)
        return;

    bool levelled = false;
    for (auto const& [glyphId, glyph] : state->glyphs)
        if (glyph.socket && state->allocated.count(glyph.socket))
            levelled |= GainGlyphExperience(player, state, glyphId, amount);
    if (levelled)
        RefreshGlyphs(player, state);
}

// A glyph item: in the bags, or by mail when they are full
void GiveGlyph(Player* player, ParagonGlyph const& glyph)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(glyph.item);
    if (!itemTemplate)
    {
        LOG_ERROR("module", "Paragon: glyph {} names item {}, which has no template", glyph.id, glyph.item);
        return;
    }

    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, glyph.item, 1) == EQUIP_ERR_OK)
    {
        if (Item* item = player->StoreNewItem(destination, glyph.item, true))
            player->SendNewItem(item, 1, true, false, true);
    }
    else
    {
        CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
        MailDraft draft(IsFrench(player) ? "Glyphe de parangon" : "Paragon glyph",
            IsFrench(player) ? "Vos sacs étaient pleins." : "Your bags were full.");
        if (Item* item = Item::CreateItem(glyph.item, 1, player))
        {
            item->SaveToDB(transaction);
            draft.AddItem(item);
        }
        draft.SendMailTo(transaction, MailReceiver(player, player->GetGUID().GetCounter()),
            MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
        CharacterDatabase.CommitTransaction(transaction);
    }

    ChatHandler(player->GetSession()).PSendSysMessage(IsFrench(player)
        ? "|cffff8000Un glyphe de parangon : {}.|r Utilisez-le pour l'apprendre, puis sertissez-le depuis le tableau."
        : "|cffff8000A paragon glyph: {}.|r Use it to learn it, then set it from the board.",
        GlyphName(player, glyph));
}

// A drop: a glyph the character has neither learnt nor carries comes first, so the collection fills without droughts
void DropGlyph(Player* player)
{
    ParagonState* state = GetState(player);
    if (!state || Glyphs.empty())
        return;

    std::vector<ParagonGlyph const*> missing;
    std::vector<ParagonGlyph const*> all;
    for (auto const& [glyphId, glyph] : Glyphs)
    {
        all.push_back(&glyph);
        if (!state->glyphs.count(glyphId) && !player->GetItemCount(glyph.item, true))
            missing.push_back(&glyph);
    }
    std::vector<ParagonGlyph const*> const& pool = missing.empty() ? all : missing;
    GiveGlyph(player, *pool[urand(0, uint32(pool.size()) - 1)]);
}

// A copy from the bags: learnt when new, experience when known
void AbsorbGlyph(Player* player, ParagonState* state, uint32 glyphId)
{
    auto const definition = Glyphs.find(glyphId);
    if (definition == Glyphs.end())
        return;
    bool const french = IsFrench(player);
    if (!player->HasItemCount(definition->second.item, 1))
    {
        Send(player, std::string("ERROR\t") + (french ? "Aucun exemplaire de ce glyphe dans vos sacs."
                                                      : "No copy of this glyph in your bags."));
        return;
    }
    if (player->IsInCombat())
    {
        Send(player, std::string("ERROR\t") + (french ? "Impossible en combat." : "Not while in combat."));
        return;
    }

    player->DestroyItemCount(definition->second.item, 1, true);
    auto const known = state->glyphs.find(glyphId);
    if (known == state->glyphs.end())
    {
        ParagonState::GlyphState& glyph = state->glyphs[glyphId];
        SaveGlyph(player, glyphId, glyph);
        SendGlyph(player, glyphId, glyph);
        ChatHandler(player->GetSession()).PSendSysMessage(french
            ? "|cffff8000{}|r rejoint votre collection. Sertissez-le dans une châsse du tableau de parangon."
            : "|cffff8000{}|r joins your collection. Set it into a socket of the paragon board.",
            GlyphName(player, definition->second));
        Send(player, Acore::StringFormat("GLYPHNEW\t{}", glyphId));
        return;
    }

    bool const levelled = GainGlyphExperience(player, state, glyphId, GlyphDuplicateXp);
    ChatHandler(player->GetSession()).PSendSysMessage(french
        ? "|cffff8000{}|r absorbe son double : +{} points d'expérience."
        : "|cffff8000{}|r absorbs its copy: +{} experience.", GlyphName(player, definition->second), GlyphDuplicateXp);
    if (levelled && known->second.socket)
        RefreshGlyphs(player, state);
}

void SocketGlyph(Player* player, ParagonState* state, uint32 socket, uint32 glyphId)
{
    bool const french = IsFrench(player);
    auto const node = Board.find(socket);
    if (node == Board.end() || node->second.effect != static_cast<uint8>(ParagonEffect::Socket) ||
        !state->allocated.count(socket))
    {
        Send(player, std::string("ERROR\t") + (french ? "Prenez d'abord cette châsse." : "Take that socket first."));
        return;
    }
    auto const glyph = state->glyphs.find(glyphId);
    if (glyph == state->glyphs.end())
    {
        Send(player, std::string("ERROR\t") + (french ? "Apprenez d'abord ce glyphe." : "Learn that glyph first."));
        return;
    }
    if (player->IsInCombat())
    {
        Send(player, std::string("ERROR\t") + (french ? "Impossible en combat." : "Not while in combat."));
        return;
    }

    // Whatever sat there goes back to the collection; the glyph leaves the socket it was in
    for (auto& [otherId, other] : state->glyphs)
        if (other.socket == socket && otherId != glyphId)
        {
            other.socket = 0;
            SaveGlyph(player, otherId, other);
            SendGlyph(player, otherId, other);
        }
    glyph->second.socket = socket;
    SaveGlyph(player, glyphId, glyph->second);
    SendGlyph(player, glyphId, glyph->second);
    RefreshGlyphs(player, state);
    Send(player, Acore::StringFormat("SOCKETED\t{}\t{}", socket, glyphId));
}

void UnsocketGlyph(Player* player, ParagonState* state, uint32 socket)
{
    if (player->IsInCombat())
    {
        Send(player, std::string("ERROR\t") + (IsFrench(player) ? "Impossible en combat." : "Not while in combat."));
        return;
    }
    for (auto& [glyphId, glyph] : state->glyphs)
        if (glyph.socket == socket)
        {
            glyph.socket = 0;
            SaveGlyph(player, glyphId, glyph);
            SendGlyph(player, glyphId, glyph);
        }
    RefreshGlyphs(player, state);
    Send(player, Acore::StringFormat("SOCKETED\t{}\t0", socket));
}

// Real players in a map, for the rewards of a boss or a key
bool IsRealPlayer(Player* player)
{
    return player && player->GetSession() && !player->GetSession()->IsBot();
}
}

void OnParagonKeyCompleted(Player* player, uint32 keyLevel)
{
    if (!IsRealPlayer(player) || keyLevel < GlyphKeyFrom ||
        !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled))
        return;

    AddGlyphExperience(player, GlyphKeyXp + GlyphKeyXpPerLevel * (keyLevel - GlyphKeyFrom));
    float const chance = std::min(GlyphKeyMaxChance,
        GlyphKeyChance + GlyphKeyChancePerLevel * static_cast<float>(keyLevel - GlyphKeyFrom));
    if (roll_chance_f(chance))
        DropGlyph(player);
}

void OnParagonInfiniteFloor(Player* player, uint32 floor, uint32 floorsDown)
{
    if (!IsRealPlayer(player) || !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled))
        return;

    // Each floor behind the player counts, the ones a fast clear's portal passes over too
    uint32 experience = 0;
    for (uint32 passed = floor; passed < floor + std::max<uint32>(floorsDown, 1); ++passed)
    {
        if (passed < GlyphFloorFrom)
            continue;
        experience += GlyphFloorXp + (passed - GlyphFloorFrom) / GlyphFloorXpStep;
        if (passed > GlyphFloorFrom && passed % GlyphFloorEvery == 0)
            DropGlyph(player);
    }
    AddGlyphExperience(player, experience);
}

void OnParagonCreatureDeath(Creature* creature)
{
    if (!creature || (!creature->IsDungeonBoss() && !creature->isWorldBoss()))
        return;
    Map* map = creature->GetMap();
    if (!map || !map->IsRaid() || !map->IsHeroic() ||
        !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled))
        return;

    map->DoForAllPlayers([](Player* player)
    {
        if (!IsRealPlayer(player))
            return;
        AddGlyphExperience(player, GlyphRaidBossXp);
        if (roll_chance_f(GlyphRaidBossChance))
            DropGlyph(player);
    });
}

bool UseParagonGlyphItem(Player* player, uint32 itemEntry)
{
    auto const glyph = GlyphByItem.find(itemEntry);
    ParagonState* state = GetState(player);
    if (glyph == GlyphByItem.end() || !state || state->bot)
        return false;
    AbsorbGlyph(player, state, glyph->second);
    return true;
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

    // A bot's board is planned, not bought: nothing a message says may change it or write it anywhere
    ParagonState* state = GetState(player);
    if (!state || state->bot)
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

        RemoveGlyphLayer(player, state);
        for (uint32 nodeId : state->allocated)
            if (auto const node = Board.find(nodeId); node != Board.end())
                ApplyNode(player, node->second, false);

        state->allocated.clear();
        ClearBuffs(player, state);
        // The sockets are gone with the rest: their glyphs go back to the collection, levels kept
        for (auto& [glyphId, glyph] : state->glyphs)
            if (glyph.socket)
            {
                glyph.socket = 0;
                SaveGlyph(player, glyphId, glyph);
            }
        RefreshGlyphs(player, state);
        player->UpdateMaxHealth();
        CharacterDatabase.Execute("DELETE FROM character_paragon WHERE guid = {}",
            player->GetGUID().GetCounter());

        SendState(player, false);
        chat.SendSysMessage(french ? "Votre tableau de parangon a été réinitialisé."
                                   : "Your paragon board has been reset.");
        RefreshGroupBots(player);
        return;
    }

    // The glyphs: the collection on request, and what the frame's glyph panel asks
    if (body == "GLYPHSYNC")
    {
        SendGlyphs(player, state);
        return;
    }

    auto const numbers = [](std::string_view text)
    {
        std::vector<uint32> values;
        std::string const copy(text);
        char const* at = copy.c_str();
        while (*at)
        {
            char* end = nullptr;
            values.push_back(static_cast<uint32>(std::strtoul(at, &end, 10)));
            if (end == at)
                break;
            at = *end ? end + 1 : end;
        }
        return values;
    };
    if (body.starts_with("SOCKET\t"))
    {
        std::vector<uint32> const values = numbers(body.substr(7));
        if (values.size() == 2)
            SocketGlyph(player, state, values[0], values[1]);
        return;
    }
    if (body.starts_with("UNSOCKET\t"))
    {
        std::vector<uint32> const values = numbers(body.substr(9));
        if (values.size() == 1)
            UnsocketGlyph(player, state, values[0]);
        return;
    }
    if (body.starts_with("ABSORB\t"))
    {
        std::vector<uint32> const values = numbers(body.substr(7));
        if (values.size() == 1)
            AbsorbGlyph(player, state, values[0]);
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

    ParagonNode const& node = entry->second;
    uint32 const cost = NodeCost(node);
    uint32 const available = AvailablePoints(state);
    if (available < cost)
    {
        Send(player, std::string("ERROR\t") + (available == 0
            ? std::string(french ? "Aucun point disponible." : "No points available.")
            : Acore::StringFormat(french ? "Ce noeud coûte {} points ({} disponibles)."
                                         : "This node costs {} points ({} available).", cost, available)));
        return;
    }

    // The far board is reached by building a character, not by a straight run from the hub
    if (SpentPoints(state) < node.required)
    {
        Send(player, std::string("ERROR\t") + Acore::StringFormat(french
            ? "Ce noeud demande {} points déjà dépensés sur le tableau." : "This node needs {} points already spent.",
            node.required));
        return;
    }

    // A side's next tier opens once the whole of the one before it is held
    if (uint32 const missing = MissingForTier(state->allocated, node))
    {
        static constexpr char const* TierNamesFr[] = { "l'Éveil", "l'Ascension", "la Transcendance" };
        static constexpr char const* TierNamesEn[] = { "Awakening", "Ascension", "Transcendence" };
        uint8 const previous = static_cast<uint8>(node.tier - 1);
        Send(player, std::string("ERROR\t") + Acore::StringFormat(french
            ? "Prenez d'abord tous les noeuds de {} de cette branche ({} restants)."
            : "Take every {} node of this branch first ({} left).",
            french ? TierNamesFr[previous] : TierNamesEn[previous], missing));
        return;
    }

    if (!IsReachable(state, nodeId))
    {
        Send(player, std::string("ERROR\t") + (french ? "Ce noeud n'est pas accessible."
                                                      : "That node is not connected to your board."));
        return;
    }

    state->allocated.insert(nodeId);
    ApplyNode(player, node, true);
    // A node within a socketed glyph's reach takes its share at once, and may meet the glyph's condition
    RefreshGlyphs(player, state);
    CharacterDatabase.Execute("REPLACE INTO character_paragon (guid, node) VALUES ({}, {})",
        player->GetGUID().GetCounter(), nodeId);

    // The frame animates from this, so it carries the node that was taken rather than just the new totals.
    Send(player, Acore::StringFormat("GAINED\t{}\t{}\t{}", nodeId, AvailablePoints(state), SpentPoints(state)));

    // The last star of a sigil: its Blessing
    if (node.sigil && IsSigilComplete(state->allocated, node.sigil))
    {
        auto const name = BlessingNames.find(node.sigil);
        chat.PSendSysMessage(french ? "|cffff8000{} vous est accordée.|r" : "|cffff8000{} is granted to you.|r",
            name != BlessingNames.end() ? name->second : std::string("Blessing"));
        Send(player, Acore::StringFormat("BLESSING\t{}", node.sigil));
    }

    // Outside a key or a challenge, bots track the group's real players, so they move the moment the player does
    RefreshGroupBots(player);
}


// ---------------------------------------------------------------------------------------------------------
// Procs
//
// These sit on the damage path, so they do as little as possible: a character with no procs allocated leaves
// each of them after two pointer checks and a vector that is empty.
// ---------------------------------------------------------------------------------------------------------

void NoteParagonDamageSource(Unit* attacker, Unit* victim, SpellInfo const* spellInfo, bool periodic)
{
    // Only a player's board ever asks
    if (!attacker || !attacker->IsPlayer())
        return;

    PendingSource.attacker = attacker;
    PendingSource.victim = victim;
    PendingSource.spell = spellInfo;
    PendingSource.periodic = periodic;
    PendingSource.set = true;
}

// Rancune (Bastion) counts every hit taken as it would have landed without the player's defences: what shields
// absorbed and what damage-taken reductions (Shield Wall, Pain Suppression, ...) took off are counted back, and the
// board's own Ward, Last Stand and reduction come after. Defending oneself must not starve it.
void NoteParagonHitTaken(Unit* victim, Unit* attacker, uint32 amount, SpellSchoolMask schoolMask)
{
    Player* player = victim ? victim->ToPlayer() : nullptr;
    if (!player || !amount || !attacker || attacker == victim || player->IsFriendlyTo(attacker))
        return;

    ParagonState* state = GetState(player);
    if (!state || !state->applied || !state->grudgePct)
        return;

    float const taken = player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);
    state->grudgePool += double(amount) / std::max(taken, 0.1f);
}

void OnParagonDamageTaken(Unit* victim, Unit* attacker, uint32& damage)
{
    Player* player = victim ? victim->ToPlayer() : nullptr;
    if (!player || !damage || !attacker || attacker == victim)
        return;

    ParagonState* state = GetState(player);
    if (!HasCombatEffects(state))
        return;

    uint32 const now = GameTime::GetGameTimeMS().count();

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

    // The caster's ward soaks what is left, until it is spent; a spent ward goes on the next update
    for (ParagonBuff& buff : state->buffs)
        if (buff.effect == ParagonEffect::Ward && buff.applied > 0 && damage)
        {
            uint32 const soaked = std::min<uint32>(damage, static_cast<uint32>(buff.applied));
            damage -= soaked;
            buff.applied -= static_cast<int32>(soaked);
            if (buff.applied <= 0)
                buff.expiresAt = 0;
        }
    if (!damage)
        return;

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
                        std::clamp<uint64>(uint64(damage) * proc.value / 100, 1,
                            uint64(player->GetMaxHealth()) * MaxRetaliateHealthPct / 100), SPELL_SCHOOL_MASK_HOLY);
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

// The enemies within `range` of `centre` the character may hit, bar `centre` itself, up to `count`
static std::vector<Unit*> NearbyEnemies(Player* player, Unit* centre, float range, uint32 count)
{
    std::list<Unit*> found;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(centre, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(centre, found, check);
    Cell::VisitObjects(centre, searcher, range);

    std::vector<Unit*> targets;
    for (Unit* target : found)
    {
        if (targets.size() >= count)
            break;
        if (target == centre || !target->IsAlive() || !player->IsValidAttackTarget(target))
            continue;
        targets.push_back(target);
    }
    return targets;
}

// An area hit's share for each of `targets` enemies: whole up to AreaFalloffFrom, then sqrt(AreaFalloffFrom / n) each
static uint64 AreaShare(uint64 share, std::size_t targets)
{
    if (targets <= AreaFalloffFrom)
        return share;
    return static_cast<uint64>(double(share) * std::sqrt(double(AreaFalloffFrom) / double(targets)));
}

void OnParagonDamageDealt(Unit* attacker, Unit* victim, uint32& damage)
{
    // Read and cleared on every hit, whoever dealt it, so a note is never left over for a later one
    DamageSource const source = PendingSource;
    PendingSource = {};

    Player* player = attacker ? attacker->ToPlayer() : nullptr;
    if (!player || !damage || !victim || attacker == victim || DealingProcDamage)
        return;

    ParagonState* state = GetState(player);
    if (!HasCombatEffects(state))
        return;

    // A weapon's hit sets off the melee branches, a spell's the caster branches; anything else only what answers to
    // anything
    HitKind kind = source.set && source.attacker == attacker && source.victim == victim
        ? KindOf(source.spell) : HitKind::Other;
    // A weapon fighter's spells answer to its own side (FightsWithWeapons)
    if (kind == HitKind::Spell && FightsWithWeapons(player))
        kind = HitKind::Weapon;
    // A periodic tick is not a hit of its own: it keeps the always-on bonuses, but sets off nothing that strikes again
    bool const periodic = source.set && source.periodic;
    state->lastHitTarget = victim->GetGUID();
    state->lastHitKind = kind;

    // Worked in 64 bits: the outer zones multiply several times over, and a big hit times a big bonus is past
    // what 32 bits hold before it is divided back down.
    uint64 dealt = damage;
    if (uint32 const damagePct = ScopedSum(state->damagePct, kind))
        dealt = dealt * (100 + damagePct) / 100;

    for (ParagonBuff const& buff : state->buffs)
        if (buff.effect == ParagonEffect::FuryOnHit && buff.applied > 0 && Answers(buff.scope, kind))
            dealt = dealt * (100 + buff.applied) / 100;

    if (state->killStreakStacks && state->killStreakPct && Answers(state->killStreakScope, kind))
        dealt = dealt * (100 + state->killStreakStacks * state->killStreakPct) / 100;

    for (ParagonProc& proc : state->procs)
    {
        switch (proc.effect)
        {
            case ParagonEffect::FuryOnHit:
            {
                if (!Answers(proc.scope, kind) || HasBuff(state, ParagonEffect::FuryOnHit) ||
                    !roll_chance_f(proc.chance))
                    break;

                ParagonBuff buff;
                buff.effect = ParagonEffect::FuryOnHit;
                buff.scope = proc.scope;
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
    // The execute nodes add up to MaxExecutePct, each below its threshold raised by ExecuteReach (MaxExecuteThreshold
    // at most): one more hit beside this one, never a multiplier on it
    uint32 finishingPct = 0;
    float strikeChance = 0.0f;
    uint32 strikePct = 0;
    for (ParagonProc const& proc : state->procs)
    {
        if (!Answers(proc.scope, kind))
            continue;
        if (proc.effect == ParagonEffect::DoubleStrike && !periodic)
        {
            strikeChance += proc.chance;
            strikePct = std::max(strikePct, proc.value);
        }
        else if (proc.effect == ParagonEffect::Execute && victim->GetHealthPct() <
                 static_cast<float>(std::min(proc.value2 + state->executeReach, MaxExecuteThreshold)))
            finishingPct += proc.value;
    }
    uint64 const finishing = dealt * std::min(finishingPct, MaxExecutePct) / 100;
    uint64 const extra = strikePct && roll_chance_f(std::min(strikeChance, MaxDoubleStrikeChance))
        ? dealt * strikePct / 100 : 0;
    QueueHit(state, victim, SPELL_PARAGON_DOUBLE_STRIKE, extra, SPELL_SCHOOL_MASK_NORMAL);
    QueueHit(state, victim, SPELL_PARAGON_EXECUTE, finishing, SPELL_SCHOOL_MASK_NORMAL);

    // A share of the hit, never of anything's health: it scales with the character, and a pack is hurt no faster than
    // the character hurts its target. A chance on a short cooldown, not every hit: splashing every swing melted a
    // pack by itself. The splash's own hits are proc damage, so they do not splash again.
    if (state->splashPct && state->splashRange && !periodic && Answers(state->splashScope, kind))
    {
        uint32 const now = GameTime::GetGameTimeMS().count();
        if (now >= state->splashReadyAt && roll_chance_f(std::min(state->splashChance, MaxSplashChance)))
        {
            state->splashReadyAt = now + state->splashCooldown;
            std::vector<Unit*> const targets = NearbyEnemies(player, victim, static_cast<float>(state->splashRange),
                std::min(MaxSplashTargets + state->areaReach, MaxAreaTargets));
            uint64 const share = AreaShare(dealt * state->splashPct / 100, targets.size());
            for (Unit* target : targets)
                QueueHit(state, target, SPELL_PARAGON_SPLASH, share, SPELL_SCHOOL_MASK_FIRE);
        }
    }

    damage = static_cast<uint32>(std::min<uint64>(dealt, std::numeric_limits<uint32>::max()));

    uint32 const leechPct = ScopedSum(state->leechPct, kind);
    if (leechPct && player->IsAlive())
    {
        uint64 const healed = uint64(damage) * std::min(leechPct, MaxLeechPct) / 100;
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

// A spell's direct damage, once dealt (Spell.cpp's OnSpellDamageDone): the caster side's echo and arc. A weapon
// ability of the melee or ranged damage class never comes through here as a spell.
void OnParagonSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage, bool critical)
{
    Player* player = caster ? caster->ToPlayer() : nullptr;
    if (!player || !victim || !spellInfo || !damage || caster == victim || DealingProcDamage)
        return;
    // A healer's damage is not doubled by the caster side's echoes and arcs: it walks those branches for the
    // spell power and the casts, and a Holy priest at 650 points dealt 70% of a fire mage's damage, half of it echoes
    if (KindOf(spellInfo) != HitKind::Spell || IsHealer(player))
        return;

    ParagonState* state = GetState(player);
    if (!state || !state->applied || (!state->echoPct && !state->arcPct))
        return;

    SpellSchoolMask const school = spellInfo->GetSchoolMask();

    // A critical strike rings out again, as a hit of its own
    if (critical && state->echoPct && roll_chance_f(std::min(state->echoChance, MaxEchoChance)))
        QueueHit(state, victim, SPELL_PARAGON_ECHO, uint64(damage) * state->echoPct / 100, school);

    // The spell arcs to the pack: a chance, on a short cooldown, so an area spell hitting ten targets rolls once
    // rather than ten times in the same instant
    if (state->arcPct && state->arcTargets)
    {
        uint32 const now = GameTime::GetGameTimeMS().count();
        if (now >= state->arcReadyAt && roll_chance_f(std::min(state->arcChance, MaxArcChance)))
        {
            state->arcReadyAt = now + state->arcCooldown;
            std::vector<Unit*> const targets = NearbyEnemies(player, victim, ArcRange,
                std::min(state->arcTargets + state->areaReach, MaxAreaTargets));
            uint64 const share = AreaShare(uint64(damage) * state->arcPct / 100, targets.size());
            for (Unit* target : targets)
                QueueHit(state, target, SPELL_PARAGON_ARC, share, school);
        }
    }
}

// A spell cast by the player itself, not triggered, of the magic damage class (damage and heals alike): the caster
// side's quickening, spell power, ward and mana. Only in combat, so none of them can be banked before a pull.
void OnParagonSpellCast(Player* player, Spell* spell)
{
    SpellInfo const* spellInfo = spell ? spell->GetSpellInfo() : nullptr;
    if (!player || !spellInfo || spell->IsTriggered() || spellInfo->IsPassive() || KindOf(spellInfo) != HitKind::Spell)
        return;

    ParagonState* state = GetState(player);
    if (!HasCastEffects(state) || !player->IsInCombat())
        return;

    uint32 const now = GameTime::GetGameTimeMS().count();

    if (state->quickenPct && !HasBuff(state, ParagonEffect::Quicken) && roll_chance_f(state->quickenChance))
    {
        ParagonBuff buff;
        buff.effect = ParagonEffect::Quicken;
        buff.expiresAt = now + state->quickenDuration;
        buff.applied = static_cast<int32>(std::min(state->quickenPct, MaxQuickenPct));
        player->ApplyCastTimePercentMod(static_cast<float>(buff.applied), true);
        state->buffs.push_back(buff);
        ShowBuff(player, SPELL_PARAGON_QUICKEN, state->quickenDuration);
    }

    if (state->insightValue && !HasBuff(state, ParagonEffect::Insight) && roll_chance_f(state->insightChance))
    {
        ParagonBuff buff;
        buff.effect = ParagonEffect::Insight;
        buff.expiresAt = now + state->insightDuration;
        buff.applied = static_cast<int32>(state->insightValue);
        ApplyPermanentStat(player, PermanentStat::SpellPower, state->insightValue, true);
        state->buffs.push_back(buff);
        ShowBuff(player, SPELL_PARAGON_INSIGHT, state->insightDuration);
    }

    // The ward is sized on the character's spell power, damage or healing, whichever is greater
    if (state->wardPct && now >= state->wardReadyAt && !HasBuff(state, ParagonEffect::Ward) &&
        roll_chance_f(state->wardChance))
    {
        int32 const power = std::max(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC),
            player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_MAGIC));
        uint64 const amount = uint64(std::max(0, power)) * state->wardPct / 100;
        if (amount)
        {
            state->wardReadyAt = now + state->wardCooldown;
            ParagonBuff buff;
            buff.effect = ParagonEffect::Ward;
            buff.expiresAt = now + state->wardDuration;
            buff.applied = static_cast<int32>(std::min<uint64>(amount, std::numeric_limits<int32>::max()));
            state->buffs.push_back(buff);
            ShowBuff(player, SPELL_PARAGON_WARD, state->wardDuration);
        }
    }

    if (state->manaPct && now >= state->manaReadyAt && player->GetMaxPower(POWER_MANA) &&
        roll_chance_f(state->manaChance))
    {
        state->manaReadyAt = now + state->manaCooldown;
        uint32 const amount = player->GetMaxPower(POWER_MANA) * state->manaPct / 100;
        if (amount)
            player->EnergizeBySpell(player, SPELL_PARAGON_MANA_SURGE, amount, POWER_MANA);
    }
}

void OnParagonKill(Player* player, Unit* killed)
{
    if (!player || !killed || killed->GetTypeId() != TYPEID_UNIT)
        return;

    ParagonState* state = GetState(player);
    if (!HasCombatEffects(state))
        return;

    // What made the kill: the character's last hit, if it was on this unit
    HitKind const kind = state->lastHitTarget == killed->GetGUID() ? state->lastHitKind : HitKind::Other;

    // The corpse goes up, sometimes. A kill the blast itself made does not blast again (DealingProcDamage).
    if (state->explosionPct && state->explosionRange && !DealingProcDamage && Answers(state->explosionScope, kind) &&
        roll_chance_f(std::min(state->explosionChance, MaxExplosionChance)))
    {
        uint64 const blast = uint64(killed->GetMaxHealth()) * std::min(state->explosionPct, MaxExplosionPct) / 100;
        killed->SendPlaySpellVisual(VisualExplosion);
        for (Unit* target : NearbyEnemies(player, killed, static_cast<float>(state->explosionRange),
                 std::numeric_limits<uint32>::max()))
            QueueHit(state, target, SPELL_PARAGON_EXPLOSION, blast, SPELL_SCHOOL_MASK_FIRE);
    }

    // A kill may add a stack and restart the timer; the stacks go all at once when it runs out
    if (state->killStreakPct && state->killStreakMax && state->killStreakDuration &&
        Answers(state->killStreakScope, kind) && roll_chance_f(std::min(state->killStreakChance, MaxKillStreakChance)))
    {
        state->killStreakStacks = std::min(state->killStreakStacks + 1, state->killStreakMax);
        state->killStreakExpiresAt = GameTime::GetGameTimeMS().count() + state->killStreakDuration;
        ShowBuff(player, SPELL_PARAGON_KILL_STREAK, state->killStreakDuration);
        if (Aura* aura = player->GetAura(SPELL_PARAGON_KILL_STREAK))
            aura->SetStackAmount(static_cast<uint8>(state->killStreakStacks));
    }

    for (ParagonProc const& proc : state->procs)
    {
        if (proc.effect != ParagonEffect::SurgeOnKill || !Answers(proc.scope, kind))
            continue;

        // Refreshed rather than stacked, for the same reason as the armour: a pull of trash would otherwise
        // end with a number nobody planned for.
        for (std::size_t index = state->buffs.size(); index > 0; --index)
            if (state->buffs[index - 1].effect == ParagonEffect::SurgeOnKill)
            {
                ApplySurge(player, state->buffs[index - 1].scope,
                    static_cast<uint32>(state->buffs[index - 1].applied), false);
                state->buffs.erase(state->buffs.begin() + (index - 1));
            }

        ParagonBuff buff;
        buff.effect = ParagonEffect::SurgeOnKill;
        buff.scope = proc.scope;
        buff.expiresAt = GameTime::GetGameTimeMS().count() + proc.duration;
        buff.applied = static_cast<int32>(proc.value);
        ApplySurge(player, proc.scope, proc.value, true);
        state->buffs.push_back(buff);
        ShowBuff(player, SPELL_PARAGON_SURGE, proc.duration);
    }
}

// Eonar's: the character's own spell heals grow, capped. The board's own heals (leech, Freya's share) do not.
void OnParagonHealDone(Unit* healer, Unit* /*target*/, uint32& heal, SpellInfo const* spellInfo)
{
    Player* player = healer ? healer->ToPlayer() : nullptr;
    if (!player || !heal || !spellInfo || DealingProcHeal || spellInfo->Id == SPELL_PARAGON_LEECH)
        return;
    ParagonState const* state = GetState(player);
    if (!state || !state->applied || !state->healPct)
        return;
    heal = static_cast<uint32>(std::min<uint64>(uint64(heal) * (100 + std::min(state->healPct, MaxHealPct)) / 100,
        std::numeric_limits<uint32>::max()));
}

// Freya's: a share of what the character was healed for goes to the most hurt ally nearby, on the next update. A shared
// heal is never shared again.
void OnParagonHealReceived(Unit* /*healer*/, Unit* receiver, uint32 gain)
{
    Player* player = receiver ? receiver->ToPlayer() : nullptr;
    if (!player || !gain || DealingProcHeal)
        return;
    ParagonState* state = GetState(player);
    if (!state || !state->applied || !state->healSharePct)
        return;

    Group* group = player->GetGroup();
    if (!group)
        return;
    Player* hurt = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == player || !member->IsAlive() || !member->IsInMap(player) ||
            member->GetHealth() >= member->GetMaxHealth() || !player->IsWithinDistInMap(member, HealShareRange))
            continue;
        if (!hurt || member->GetHealthPct() < hurt->GetHealthPct())
            hurt = member;
    }
    if (!hurt)
        return;
    uint32 const amount = static_cast<uint32>(uint64(gain) * std::min(state->healSharePct, MaxHealSharePct) / 100);
    if (amount)
        state->pendingHeals.push_back({ hurt->GetGUID(), amount });
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

// Rancune: the damage taken lately, fading, turned into attack and spell power. Scaled on the hits it answers, like
// Cataclysm's Vengeance, so a tank's threat keeps up with a harder key, but held to a share of the tank's own health.
void UpdateGrudge(Player* player, ParagonState* state, uint32 now)
{
    if (!state->grudgePct && !state->grudgeApplied)
        return;
    if (now < state->grudgeTickAt)
        return;
    uint32 const elapsed = state->grudgeTickAt ? now - state->grudgeTickAt + GrudgeTickMs : GrudgeTickMs;
    state->grudgeTickAt = now + GrudgeTickMs;

    state->grudgePool *= std::pow(0.5, elapsed / GrudgeHalfLifeMs);
    if (!player->IsInCombat() || !player->IsAlive())
        state->grudgePool = 0.0;

    uint32 const cap = player->GetMaxHealth() * state->grudgeCapPct / 100;
    uint32 const wanted = std::min(static_cast<uint32>(state->grudgePool * state->grudgePct / 100.0), cap);

    // Re-applied only on a real change: every step is a stat update the client is told about
    uint32 const step = std::max<uint32>(25, state->grudgeApplied / 20);
    if (wanted == state->grudgeApplied ||
        (wanted && wanted + step > state->grudgeApplied && state->grudgeApplied + step > wanted))
        return;

    if (state->grudgeApplied)
    {
        ApplyPermanentStat(player, PermanentStat::AttackPower, state->grudgeApplied, false);
        ApplyPermanentStat(player, PermanentStat::SpellPower, state->grudgeApplied, false);
    }
    state->grudgeApplied = wanted;
    if (wanted)
    {
        ApplyPermanentStat(player, PermanentStat::AttackPower, wanted, true);
        ApplyPermanentStat(player, PermanentStat::SpellPower, wanted, true);
        if (!player->HasAura(SPELL_PARAGON_GRUDGE))
            player->AddAura(SPELL_PARAGON_GRUDGE, player);
    }
    else
        player->RemoveAurasDueToSpell(SPELL_PARAGON_GRUDGE);
}

uint32 GetParagonThreatPct(Player* player)
{
    ParagonState const* state = GetState(player);
    return state && state->applied ? state->threatPct : 0;
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

    if (!state->pendingHeals.empty())
    {
        std::vector<ParagonState::PendingHeal> heals;
        heals.swap(state->pendingHeals);
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_PARAGON_LEECH);
        DealingProcHeal = true;
        for (ParagonState::PendingHeal const& heal : heals)
            if (Player* target = ObjectAccessor::GetPlayer(*player, heal.target);
                spellInfo && target && target->IsAlive() && player->IsInMap(target))
            {
                HealInfo healInfo(player, target, heal.amount, spellInfo, spellInfo->GetSchoolMask());
                player->HealBySpell(healInfo);
            }
        DealingProcHeal = false;
    }

    uint32 const now = GameTime::GetGameTimeMS().count();
    UpdateGrudge(player, state, now);
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

        EndBuff(player, buff);
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

        // Counted as the character will have them once in game, at the nodes' costs: nodes the board no longer has
        // are not
        uint32 level = 0;
        if (QueryResult result = CharacterDatabase.Query("SELECT node FROM character_paragon WHERE guid = {}", counter))
            do
            {
                if (auto const node = Board.find(result->Fetch()[0].Get<uint32>()); node != Board.end())
                    level += NodeCost(node->second);
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
    RemoveGlyphLayer(player, state);
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

// A glyph used from the bags: learnt into the collection, or absorbed as experience. The item's on-use spell is only
// there so the client lets it be used; nothing is cast.
class item_paragon_glyph : public ItemScript
{
public:
    item_paragon_glyph() : ItemScript("item_paragon_glyph") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        UseParagonGlyphItem(player, item->GetEntry());
        return true;
    }
};

void AddParagonScripts()
{
    new npc_stat_growth_paragon_keeper();
    new ParagonCharacterListScript();
    new item_paragon_glyph();
}
