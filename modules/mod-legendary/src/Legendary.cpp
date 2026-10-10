// Legendary items: per-copy rolls, applied when worn, and their powers. See Legendary.h and README.md.

#include "Legendary.h"

#include "Bag.h"
#include "CellImpl.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "DynamicObject.h"
#include "GlobalScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroundLoot.h"
#include "PersonalLootSystem.h"
#include "Item.h"
#include "LiveTuning.h"
#include "Log.h"
#include "Mail.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "PowerScaling.h"
#include "Random.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include "Tokenize.h"
#include "UnitScript.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>

// mod-playerbots (RaidFinder.cpp, ChallengeBoard.cpp), built into the same modules library: a Défi's tier, and the
// item level of the god's gear at it
uint8 GetChallengeTierOf(Map const* map);
uint32 GetChallengeGodItemLevel(uint8 tier);
// The Défi boss of an instance (0: not a Défi's), and the item level of the board's highest page (its pinnacles')
uint32 GetChallengeBossOf(Map const* map);
uint32 GetChallengeTopItemLevel();
namespace RaidFinder
{
// A raid mode's gear item level (0: not a raid the Défis know)
uint32 GetChallengeItemLevel(uint16 map, uint8 difficulty);
}

// mod-stat-growth (LootFit.cpp), built into the same modules library: the player's role (LootFit::Role: 0 tank,
// 1 strength fighter, 2 agility fighter, 3 caster, 4 healer) and the primary stat its gear takes
uint8 LootRoleOfPlayer(Player* player);
uint32 LootPrimaryStatOf(Player* player);

namespace Legendary
{
namespace
{
// The client's tooltip feed (FrameXML Legendary.lua):
//   server -> client  C <tab> seed <tab> legendary <tab> item level <tab> power x10 <tab> window low x10 <tab>
//                         window high x10 <tab> armour <tab> type=value,type=value... [<tab> bag:slot]
//   client -> server  W <tab> bag <tab> slot    (a copy the player carries, by where it is: the server's bag and slot)
//                     Q <tab> seed              (a copy by its id)
// The seed is the copy's item guid. The client knows its own copies by where they sit: the 3.3.5 client does not put
// an item's property seed in its links (only a random suffix item's), so a link alone does not say which copy it is.
constexpr std::string_view Prefix = "LEGENDARY";

// The powers' spells: localTools/legendary/Spells.ps1, every one of them the wearer's own (its name and icon in the
// combat log and Details). None of them feeds a power again. The Scarlet Cathedral's are 97000-97005, the other
// dungeons' 97700-97999: the Barbarian's own spells sit between (97100-97299).
bool IsOwnSpell(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;
    uint32 const id = spellInfo->Id;
    return (id >= 97000 && id <= 97005) || (id >= 97700 && id <= 97999);
}

// Health below which "low health" powers (Execute, Last Stand) act, and the Bulwark's threshold
constexpr float LowHealthPct = 35.0f;
constexpr float BulwarkHealthPct = 50.0f;
// An overhealing shield never holds more than this share of its target's health
constexpr float OverhealShieldCapPct = 20.0f;

// The slots' budgets at item level 277, the medians of the stock epics of that slot for each kind of wearer
// (2026-10-07): strength or agility (the higher of the two: a strength slot's own median is a tank's, its stamina
// raised), the attack power of agility gear, intellect and the spell power of caster gear, a secondary. The first
// budgets were one median for everyone - a caster's - and left strength and agility copies a quarter short, agility
// ones without their attack power. A trinket takes a neck's.
// What a legendary's stats have over a stock epic's of its item level (its sockets are a stock item's: the base
// items' templates, localTools/legendary/buildLegendaryItemSql.py)
constexpr float LegendaryPremium = 1.10f;
// The stamina of a stock epic over its primary stat (the item level 277 epics, slot by slot: 1.25 to 1.6, the plate
// slots' median about 1.3): a copy rolled stamina equal to its primary, a fifth under a stock epic's
constexpr float StaminaShare = 1.3f;

constexpr Budget HeadBudget = { 277.0f, 184, 212, 139, 186, 110, { 300, 564, 1253, 2239 } };
constexpr Budget NeckBudget = { 277.0f, 105, 120, 78, 110, 63, { 0, 0, 0, 0 } };
constexpr Budget ShoulderBudget = { 277.0f, 138, 165, 103, 150, 86, { 277, 521, 1157, 2067 } };
constexpr Budget CloakBudget = { 277.0f, 102, 120, 78, 110, 64, { 185, 185, 185, 185 } };
constexpr Budget ChestBudget = { 277.0f, 184, 212, 139, 195, 114, { 369, 694, 1542, 2756 } };
constexpr Budget WristBudget = { 277.0f, 102, 120, 78, 110, 63, { 162, 304, 675, 1206 } };
constexpr Budget GlovesBudget = { 277.0f, 138, 165, 103, 150, 86, { 231, 434, 964, 1723 } };
constexpr Budget WaistBudget = { 277.0f, 139, 181, 103, 140, 82, { 208, 391, 867, 1550 } };
constexpr Budget LegsBudget = { 277.0f, 183, 212, 139, 195, 114, { 323, 608, 1349, 2412 } };
constexpr Budget FeetBudget = { 277.0f, 120, 181, 103, 140, 82, { 254, 477, 1060, 1895 } };
constexpr Budget RingBudget = { 277.0f, 102, 120, 78, 110, 62, { 0, 0, 0, 0 } };

// The Mythic+ pool's dungeons (Dungeon Finder ids: mod-playerbots RaidFinder.cpp MythicDungeons)
constexpr uint32 ScarletCathedral = 164;
constexpr uint32 Mechanar = 192;
constexpr uint32 UtgardeKeep = 242;
constexpr uint32 ShatteredHalls = 189;
constexpr uint32 Deadmines = 6;
constexpr uint32 DrakTharon = 215;
constexpr uint32 ForgeOfSouls = 252;
constexpr uint32 HallsOfLightning = 212;
// The Hollow Voice's win: Archbishop Aldric's death (mod-stat-growth HollowVoice.cpp NPC_ALDRIC)
constexpr uint32 HollowVoiceBoss = 930100;
// L'Infini, the Défi board's god (mod-stat-growth InfiniteGod.cpp NPC_INFINI): its legendary drops at its gear's item
// level for the Défi's tier (GetChallengeGodItemLevel)
constexpr uint32 InfiniteGodBoss = 930000;
// Gardien-chef Vorhan, the Geôle's head warden (mod-stat-growth WardenVorhan.cpp): his Unique drops at his gear's item
// level (mod-playerbots ChallengeBoard.cpp, his page)
constexpr uint32 WardenVorhanBoss = 930200;
constexpr uint32 WardenVorhanItemLevel = 485;
// L'Étoile captive: how far around the wearer the star finds a target when they have none, and their allies to heal
constexpr float SupernovaTargetReach = 30.0f;
constexpr float SupernovaHealReach = 40.0f;
// Écho du Néant: what counts as a big cooldown, the least a cooldown must have left to be echoed (not a global
// cooldown's tail), and the rest between two echoes
constexpr uint32 EchoMinCooldownMs = 20000;
constexpr uint32 EchoMinLeftMs = 1500;
constexpr uint32 EchoRestMs = 1000;
// Sablier de Perpétuité: how long a sentence's mark outlasts the time to the next
constexpr int32 SentenceMarkGraceMs = 1500;

uint32 const Floor = Mythic::GetItemLevel(2);

// Every legendary: its base item (Item.dbc rows with no template of their own, given one in the module's world SQL and
// their look in localTools/patchSinisterStrike.ps1), its power, its window (bottom at +2, top at +60), its slot's
// budget, its dungeon and its numbers. Misc armour rows: every class wears them, the armour rolled for the looter's
// own type. Three per dungeon.
std::array<Definition, 27> const Definitions = { {
    // --- The Scarlet Cathedral ---
    // Marque de l'Inquisiteur, a cloak (24567): direct damage burns as Holy over 4 sec, 5-10% -> 25-35%
    { 1, 24567, KIND_BRAND, 5.0f, 10.0f, 25.0f, 35.0f, Floor, CloakBudget, ScarletCathedral,
      { .spell = 97000, .count = 4 } },
    // Serment de Whitemane, a ring (996): a killing blow leaves 1 health, 10-15% -> 40-50% of it back over 4 sec
    { 2, 996, KIND_OATH, 10.0f, 15.0f, 40.0f, 50.0f, Floor, RingBudget, ScarletCathedral,
      { .spell = 97001, .spell2 = 97002, .count = 4 } },
    // Consécration de Mograine, gloves (21428): holy ground every 10 sec, 5-10% -> 25-35% of AP or SP a second
    { 3, 21428, KIND_GROUND, 5.0f, 10.0f, 25.0f, 35.0f, Floor, GlovesBudget, ScarletCathedral,
      { .spell = 97003, .spell2 = 97004, .spell3 = 97005, .everyMs = 10000, .count = 6, .radius = 8.0f } },

    // --- The Mechanar ---
    // Bouclier de Capacitus, shoulders (21424): melee blows taken strike back as Arcane, 10-15% -> 40-50%
    { 4, 21424, KIND_THORNS, 10.0f, 15.0f, 40.0f, 50.0f, Floor, ShoulderBudget, Mechanar,
      { .spell = 97700 } },
    // Abaque de Pathaleon, a trinket (1258): 15% of direct hits haste 5-8% -> 15-20% for 8 sec, once per 30 sec
    { 5, 1258, KIND_SURGE, 5.0f, 8.0f, 15.0f, 20.0f, Floor, NeckBudget, Mechanar,
      { .spell = 97710, .chance = 15.0f, .cooldownMs = 30000 } },
    // Brassards de Sepethrea, bracers (21432): spells burn as Fire over 4 sec, 8-12% -> 30-40%
    { 6, 21432, KIND_BRAND, 8.0f, 12.0f, 30.0f, 40.0f, Floor, WristBudget, Mechanar,
      { .spell = 97720, .count = 4, .filter = FILTER_SPELL } },

    // --- Utgarde Keep ---
    // Ceinture d'Ingvar, a belt (21425): every 5th direct hit, a shadow axe, 50-70% -> 180-240% of AP or SP
    { 7, 21425, KIND_ECHO, 50.0f, 70.0f, 180.0f, 240.0f, Floor, WaistBudget, UtgardeKeep,
      { .spell = 97730, .count = 5 } },
    // Tombeau de Keleseth, a chest (21420): below 35% health, 10-15% -> 30-40% less damage taken
    { 8, 21420, KIND_LAST_STAND, 10.0f, 15.0f, 30.0f, 40.0f, Floor, ChestBudget, UtgardeKeep,
      { .spell = 97740 } },
    // Appel d'Annhylde, a neck (26541): a kill, 5-8% -> 15-20% more damage for 10 sec
    { 9, 26541, KIND_KILL_FRENZY, 5.0f, 8.0f, 15.0f, 20.0f, Floor, NeckBudget, UtgardeKeep,
      { .spell = 97750 } },

    // --- The Shattered Halls ---
    // Poignes de Kargath, gloves (21437): direct damage to 4 more enemies within 6 yd of the target, 10-15% -> 35-45%
    { 10, 21437, KIND_CLEAVE, 10.0f, 15.0f, 35.0f, 45.0f, Floor, GlovesBudget, ShatteredHalls,
      { .spell = 97760, .count = 4, .radius = 6.0f } },
    // Bandelettes de Nethekurse, bracers (21433): damage over time 8-12% -> 30-40% stronger
    { 11, 21433, KIND_DOT_FEAST, 8.0f, 12.0f, 30.0f, 40.0f, Floor, WristBudget, ShatteredHalls, {} },
    // Chevalière de Porung, a ring (5828): 2-3% -> 6-8% of direct damage heals
    { 12, 5828, KIND_LEECH, 2.0f, 3.0f, 6.0f, 8.0f, Floor, RingBudget, ShatteredHalls,
      { .spell = 97780 } },

    // --- The Deadmines ---
    // Plastron de VanCleef, a chest (21421): 8-12% -> 30-40% more damage to enemies below 35% health
    { 13, 21421, KIND_EXECUTE, 8.0f, 12.0f, 30.0f, 40.0f, Floor, ChestBudget, Deadmines, {} },
    // Ceinture à poudre de Gilnid, a belt (21429): kills blow up, 50-80% -> 200-260% of AP or SP within 8 yd
    { 14, 21429, KIND_KILL_NOVA, 50.0f, 80.0f, 200.0f, 260.0f, Floor, WaistBudget, Deadmines,
      { .spell = 97800, .radius = 8.0f } },
    // Moufles de Cookie, gloves (21444): every 5 sec, the most hurt ally healed, 50-80% -> 200-260% of AP or SP
    { 15, 21444, KIND_RENEW_ALLIES, 50.0f, 80.0f, 200.0f, 260.0f, Floor, GlovesBudget, Deadmines,
      { .spell = 97810, .everyMs = 5000, .radius = 40.0f } },

    // --- Drak'Tharon Keep ---
    // Griffes du roi Dred, boots (18161): weapon blows bleed over 6 sec, 5-10% -> 25-35%
    { 16, 18161, KIND_BRAND, 5.0f, 10.0f, 25.0f, 35.0f, Floor, FeetBudget, DrakTharon,
      { .spell = 97820, .count = 6, .filter = FILTER_WEAPON } },
    // Robe de Novos, a robe (21430): 15-25% -> 50-70% of overhealing shields its target
    { 17, 21430, KIND_OVERHEAL_SHIELD, 15.0f, 25.0f, 50.0f, 70.0f, Floor, ChestBudget, DrakTharon,
      { .spell = 97830 } },
    // Pendentif de Tharon'ja, a neck (27218): 8-12% -> 30-40% of a heal on the most hurt other ally within 40 yd
    { 18, 27218, KIND_HEAL_SPLASH, 8.0f, 12.0f, 30.0f, 40.0f, Floor, NeckBudget, DrakTharon,
      { .spell = 97840, .radius = 40.0f } },

    // --- The Forge of Souls ---
    // Jambières du Dévoreur, legs (21423): a well of souls every 10 sec, 8-13% -> 35-45% of AP or SP a second, as wide
    // as its look (6 yd)
    { 19, 21423, KIND_GROUND, 8.0f, 13.0f, 35.0f, 45.0f, Floor, LegsBudget, ForgeOfSouls,
      { .spell = 97850, .spell2 = 97851, .everyMs = 10000, .count = 6, .radius = 6.0f } },
    // Heaume de Bronjahm, a helm (21434): a kill gives back 2-3% -> 6-8% of the health over 4 sec
    { 20, 21434, KIND_KILL_HEAL, 2.0f, 3.0f, 6.0f, 8.0f, Floor, HeadBudget, ForgeOfSouls,
      { .spell = 97860, .count = 4 } },
    // L'Âme reflétée, a ring (6673): 10-15% -> 40-50% of direct damage to another enemy within 10 yd
    { 21, 6673, KIND_CHAIN, 10.0f, 15.0f, 40.0f, 50.0f, Floor, RingBudget, ForgeOfSouls,
      { .spell = 97870, .count = 1, .radius = 10.0f } },

    // --- The Halls of Lightning ---
    // Étincelle d'Ionar, a trinket (8688): a hit leaps to 3 enemies within 10 yd, 20-30% -> 80-100%, every 2 sec
    { 22, 8688, KIND_CHAIN, 20.0f, 30.0f, 80.0f, 100.0f, Floor, NeckBudget, HallsOfLightning,
      { .spell = 97880, .count = 3, .radius = 10.0f, .cooldownMs = 2000 } },
    // Poings de Loken, gloves (21450): a lightning nova every 6 sec, 30-50% -> 130-170% of AP or SP within 10 yd
    { 23, 21450, KIND_PULSE, 30.0f, 50.0f, 130.0f, 170.0f, Floor, GlovesBudget, HallsOfLightning,
      { .spell = 97891, .spell2 = 97890, .everyMs = 6000, .radius = 10.0f } },
    // Chevalière de Bjarngrim, a ring (6674): below 50% health, a shield of 10-15% -> 30-40% of it, once per min
    { 24, 6674, KIND_BULWARK, 10.0f, 15.0f, 30.0f, 40.0f, Floor, RingBudget, HallsOfLightning,
      { .spell = 97900, .spell2 = 97901 } },

    // --- The Hollow Voice: the Unique (quality 6, red), above the others ---
    // Écho du Néant, a ring (10555): an ability with a cooldown of 20 sec or more echoes, the others' cooldowns
    // losing 20-30% of what they have left. Only from the Hollow Voice (item level 477): one window
    { 25, 10555, KIND_COOLDOWN_ECHO, 20.0f, 30.0f, 20.0f, 30.0f, Mythic::MaxPinnacleItemLevel, RingBudget, 0,
      { .spell = 97910 }, HollowVoiceBoss },

    // --- L'Infini, the Défi board's god ---
    // L'Étoile captive, a trinket (16067): 15-20% of the damage and healing done feeds a star that collapses every
    // 20 sec in combat, its damage shared by the target and the enemies within 8 yd of it, its healing by the 5 most
    // hurt allies within 40 yd. From L'Infini's death, at its gear's item level (370 at Défi I): one window
    { 26, 16067, KIND_SUPERNOVA, 15.0f, 20.0f, 15.0f, 20.0f, Mythic::MaxLootItemLevel, NeckBudget, 0,
      { .spell = 97920, .spell2 = 97921, .spell3 = 97922, .everyMs = 20000, .count = 5, .radius = 8.0f },
      InfiniteGodBoss },

    // --- Gardien-chef Vorhan: the Unique ---
    // Sablier de Perpétuité, a trinket (17855, a free row made one): his Perpétuité, every 4 sec in combat a sentence
    // on the wearer's target, a share of what they dealt it since the last, heavier each time on the same target -
    // a fifth of 20-25% the first time, the whole from the fifth. Only from his death (item level 485): one window
    { 27, 17855, KIND_SENTENCE, 20.0f, 25.0f, 20.0f, 25.0f, WardenVorhanItemLevel, NeckBudget, 0,
      { .spell = 97930, .spell2 = 97931, .everyMs = 4000, .count = 5 }, WardenVorhanBoss },
} };

// A legendary drops for each player who completes a key of its source, rarely: this chance, raised by the step for
// every key of that source completed without one, never above the cap (bad luck protection, reset by a drop). Kept
// per character and per source in character_legendary_luck. At 2 / 0.5 / 10 (the cap raised from 3 on 2026-10-07:
// a legendary every 33 keys on average felt like none at all) the chance climbs to 10% over 16 dry keys.
LiveTuning::Knob const DropBasePct("legendary.drop_base_pct", 2.0f);
LiveTuning::Knob const DropStepPct("legendary.drop_step_pct", 0.5f);
LiveTuning::Knob const DropCapPct("legendary.drop_cap_pct", 10.0f);

std::shared_mutex StoreLock;
std::unordered_map<ObjectGuid::LowType, Copy> Store;
// Each copy's failed reinforcements since its last success (character_legendary_upgrade), under StoreLock
std::unordered_map<ObjectGuid::LowType, uint32> Fails;

// --- Reinforcing at the Forge ----------------------------------------------------------------------------------------
// An attempt's chance: (item levels left to the cap / ChanceReach) squared, between ChanceFloor and ChanceCeiling -
// 90% from 70 levels away, 51% at 50, 18% at 30, 8% at 20, 2% from 10. Each failure adds PityShare of that chance to
// the next attempt, and the attempt after GuaranteedAfter failures always holds. From 370 to 485, a copy takes about
// 90 of the material on average, near 40 of them for its last 10 levels.
constexpr float ChanceReach = 70.0f;
constexpr float ChanceFloor = 2.0f;
constexpr float ChanceCeiling = 90.0f;
LiveTuning::Knob const PitySharePct("legendary.upgrade_pity_pct", 12.5f);
LiveTuning::Knob const GuaranteedAfter("legendary.upgrade_guaranteed_after", 50.0f);
// The material's chance for each player of a high-end source (on top of its loot, never in place of any), and what
// counts as one: gear of this item level or more
LiveTuning::Knob const MaterialDropPct("legendary.material_drop_pct", 35.0f);
constexpr uint32 MaterialMinItemLevel = 250;
// The material's rolls are once per instance and source, as the drops' (FirstRoll): its sources apart from theirs
constexpr uint32 MaterialSource = 0x80000000;

float UpgradeChance(uint32 itemLevel, uint32 cap, uint32 fails)
{
    if (float(fails) >= float(GuaranteedAfter))
        return 100.0f;
    float const left = float(cap > itemLevel ? cap - itemLevel : 0) / ChanceReach;
    float const base = std::clamp(left * left * 100.0f, ChanceFloor, ChanceCeiling);
    return std::min(100.0f, base * (1.0f + float(PitySharePct) / 100.0f * float(fails)));
}

bool IsRating(uint32 type)
{
    return (type >= ITEM_MOD_DEFENSE_SKILL_RATING && type <= ITEM_MOD_EXPERTISE_RATING) ||
        type == ITEM_MOD_ARMOR_PENETRATION_RATING;
}

std::string EncodeStats(Copy const& copy)
{
    std::string text;
    for (auto const& [type, value] : copy.stats)
        text += Acore::StringFormat("{}{}={}", text.empty() ? "" : ",", type, value);
    return text;
}

std::vector<std::pair<uint32, int32>> DecodeStats(std::string_view text)
{
    std::vector<std::pair<uint32, int32>> stats;
    for (std::string_view pair : Acore::Tokenize(text, ',', false))
    {
        std::vector<std::string_view> parts = Acore::Tokenize(pair, '=', false);
        if (parts.size() != 2)
            continue;
        Optional<uint32> type = Acore::StringTo<uint32>(parts[0]);
        Optional<int32> value = Acore::StringTo<int32>(parts[1]);
        if (type && value)
            stats.emplace_back(*type, *value);
    }
    return stats;
}

std::optional<Copy> FindCopy(ObjectGuid::LowType guid)
{
    std::shared_lock lock(StoreLock);
    auto const found = Store.find(guid);
    if (found == Store.end())
        return std::nullopt;
    return found->second;
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

void SendCopy(Player* player, ObjectGuid::LowType seed, Copy const& copy, Item const* where = nullptr)
{
    Definition const* definition = GetDefinition(copy.legendary);
    if (!definition)
        return;
    auto const [low, high] = PowerWindow(*definition, copy.itemLevel);
    std::string body = Acore::StringFormat("C\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", seed, copy.legendary,
        copy.itemLevel, std::lround(copy.power * 10.0f), std::lround(low * 10.0f), std::lround(high * 10.0f),
        copy.armor, EncodeStats(copy));
    if (where)
        body += Acore::StringFormat("\t{}:{}", where->GetBagSlot(), where->GetSlot());
    Send(player, body);
}

// The copy's id in the item itself: links carry it, the client asks for the copy by it. Not saved with the item, so
// set again whenever a legendary is loaded (login, equipping, a mail taken).
void Mark(Item* item)
{
    uint32 const seed = item->GetGUID().GetCounter();
    if (item->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED) != seed)
        item->SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, seed);
}

template <typename Visit>
void ForEachItem(Player* player, Visit visit)
{
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < BANK_SLOT_BAG_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;
        visit(item);
        if (Bag* bag = item->ToBag())
            for (uint32 index = 0; index < bag->GetBagSize(); ++index)
                if (Item* inside = bag->GetItemByPos(uint8(index)))
                    visit(inside);
    }
}

int32 Spread(int32 value, float spread)
{
    return std::max(1, int32(std::lround(float(value) * frand(1.0f - spread, 1.0f + spread))));
}

Copy Roll(Definition const& definition, Player* player, uint32 itemLevel, std::optional<float> powerOverride)
{
    Copy copy;
    copy.legendary = definition.id;
    copy.itemLevel = itemLevel;
    auto const [low, high] = PowerWindow(definition, itemLevel);
    copy.power = powerOverride ? *powerOverride : std::round(frand(low, high) * 10.0f) / 10.0f;

    Budget const& budget = definition.budget;
    float const level = float(itemLevel);
    // A legendary's stats are a stock epic's at its item level, a little more (LegendaryPremium); its armour is the
    // slot's
    float const statGrowth = ::Power::StatGrowth(budget.itemLevel, level) * LegendaryPremium;
    float const ratingGrowth = ::Power::StatGrowth(budget.itemLevel, level, true) * LegendaryPremium;
    float const armorGrowth = ::Power::StatGrowth(budget.itemLevel, level);
    int32 const armor = budget.armor[ArmorType(player)];
    copy.armor = armor ? Spread(int32(std::lround(float(armor) * armorGrowth)), 0.0f) : 0;

    uint32 const primary = FavouredPrimary(player);
    bool const caster = primary == ITEM_MOD_INTELLECT;
    int32 const main = caster ? budget.intellect : budget.physical;
    copy.stats.emplace_back(primary, Spread(int32(std::lround(main * statGrowth)), 0.05f));
    copy.stats.emplace_back(ITEM_MOD_STAMINA, Spread(int32(std::lround(main * StaminaShare * statGrowth)), 0.05f));
    if (caster)
        copy.stats.emplace_back(ITEM_MOD_SPELL_POWER,
            Spread(int32(std::lround(budget.spellPower * statGrowth)), 0.05f));
    else if (primary == ITEM_MOD_AGILITY)
        copy.stats.emplace_back(ITEM_MOD_ATTACK_POWER,
            Spread(int32(std::lround(budget.attackPower * statGrowth)), 0.05f));

    // Two secondaries drawn from the ones that suit the role: a tank's defences, a caster's or a healer's, a damage
    // dealer's
    constexpr uint8 TankRole = 0;
    std::vector<uint32> pool = caster ?
        std::vector<uint32>{ ITEM_MOD_CRIT_RATING, ITEM_MOD_HASTE_RATING, ITEM_MOD_HIT_RATING, ITEM_MOD_SPIRIT } :
        LootRoleOfPlayer(player) == TankRole ?
        std::vector<uint32>{ ITEM_MOD_DEFENSE_SKILL_RATING, ITEM_MOD_DODGE_RATING, ITEM_MOD_PARRY_RATING,
                             ITEM_MOD_EXPERTISE_RATING, ITEM_MOD_HIT_RATING } :
        std::vector<uint32>{ ITEM_MOD_CRIT_RATING, ITEM_MOD_HASTE_RATING, ITEM_MOD_HIT_RATING,
                             ITEM_MOD_EXPERTISE_RATING, ITEM_MOD_ARMOR_PENETRATION_RATING };
    for (uint32 draw = 0; draw < 2 && !pool.empty(); ++draw)
    {
        uint32 const index = urand(0, uint32(pool.size() - 1));
        float const growth = pool[index] == ITEM_MOD_SPIRIT ? statGrowth : ratingGrowth;
        copy.stats.emplace_back(pool[index], Spread(int32(std::lround(budget.secondary * growth)), 0.10f));
        pool.erase(pool.begin() + index);
    }
    return copy;
}

// A copy reinforced to a higher item level, its rolls kept: every stat grown as generated gear grows (ratings by the
// square root), its armour too, and its power at the same place in the new level's window (never lower: a GM's
// forced power above the window stays)
Copy Grow(Definition const& definition, Copy copy, uint32 itemLevel)
{
    float const from = float(copy.itemLevel);
    float const to = float(itemLevel);
    float const statGrowth = ::Power::StatGrowth(from, to);
    float const ratingGrowth = ::Power::StatGrowth(from, to, true);
    copy.armor = int32(std::lround(float(copy.armor) * statGrowth));
    for (auto& [type, value] : copy.stats)
        value = std::max(1, int32(std::lround(float(value) * (IsRating(type) ? ratingGrowth : statGrowth))));

    auto const [oldLow, oldHigh] = PowerWindow(definition, copy.itemLevel);
    auto const [low, high] = PowerWindow(definition, itemLevel);
    float const place = oldHigh > oldLow ? std::clamp((copy.power - oldLow) / (oldHigh - oldLow), 0.0f, 1.0f) : 1.0f;
    copy.power = std::max(copy.power, std::round((low + (high - low) * place) * 10.0f) / 10.0f);
    copy.itemLevel = itemLevel;
    return copy;
}

void Save(ObjectGuid::LowType guid, ObjectGuid::LowType owner, Copy const& copy)
{
    CharacterDatabase.Execute("REPLACE INTO character_legendary (item_guid, owner_guid, legendary, item_level, power, "
        "armor, stats) VALUES ({}, {}, {}, {}, {}, {}, '{}')", guid, owner, copy.legendary, copy.itemLevel, copy.power,
        copy.armor, EncodeStats(copy));
}

// --- The powers that are worn ----------------------------------------------------------------------------------------

constexpr char const* WornKey = "LegendaryWorn";

// A worn legendary's running state on its wearer: timers, a hit counter, a ground's pulses, healing waiting to land
struct PowerState
{
    uint32 timer = 0;
    uint32 counter = 0;
    uint32 pulsesLeft = 0;
    uint32 pulseIn = 0;
    uint32 readyAt = 0;             // getMSTime() from which a power with a cooldown may act again
    float pending = 0.0f;
    float healing = 0.0f;           // healing waiting to land (the star's)
    bool running = false;           // a timed power's cycle under way
    ObjectGuid target;              // the enemy under sentence (the hourglass's)
};

struct Worn : public DataMap::Base
{
    // Per equipment slot: the legendary worn there and its strength (0: none)
    std::array<std::pair<uint32, float>, EQUIPMENT_SLOT_END> slots = {};
    std::unordered_map<uint32, PowerState> states;

    // Every legendary worn, once, with its strength: two copies of one add up
    template <typename Visit>
    void ForEach(Kind kind, Visit visit)
    {
        std::array<std::pair<Definition const*, float>, EQUIPMENT_SLOT_END> seen = {};
        size_t count = 0;
        for (auto const& [legendary, value] : slots)
        {
            if (!legendary || value <= 0.0f)
                continue;
            Definition const* definition = GetDefinition(legendary);
            if (!definition || definition->kind != kind)
                continue;
            auto found = std::find_if(seen.begin(), seen.begin() + count,
                [definition](auto const& entry) { return entry.first == definition; });
            if (found != seen.begin() + count)
                found->second += value;
            else
                seen[count++] = { definition, value };
        }
        for (size_t index = 0; index < count; ++index)
            visit(*seen[index].first, seen[index].second, states[seen[index].first->id]);
    }

    bool Wears(Kind kind)
    {
        for (auto const& [legendary, value] : slots)
            if (legendary && value > 0.0f)
                if (Definition const* definition = GetDefinition(legendary))
                    if (definition->kind == kind)
                        return true;
        return false;
    }
};

Worn* GetWorn(Player* player)
{
    return player->CustomData.GetDefault<Worn>(WornKey);
}

// What a share of "attack or spell power" is taken of: the higher of the two, for the power's own school
float PowerOf(Player* player, uint32 spell)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell);
    SpellSchoolMask const school = spellInfo ? spellInfo->GetSchoolMask() : SPELL_SCHOOL_MASK_HOLY;
    return std::max({ player->GetTotalAttackPowerValue(BASE_ATTACK),
        float(player->SpellBaseDamageBonusDone(school)), float(player->SpellBaseHealingBonusDone(school)) });
}

int32 Share(float of, float percent)
{
    return std::max(1, int32(std::lround(of * percent / 100.0f)));
}

// The living enemies of the wearer within radius of a point
std::list<Unit*> EnemiesAround(Player* player, WorldObject* center, float radius)
{
    std::list<Unit*> enemies;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, enemies, check);
    Cell::VisitObjects(center, searcher, radius);
    enemies.remove_if([player](Unit* enemy) { return !enemy->IsAlive() || !player->IsValidAttackTarget(enemy); });
    return enemies;
}

// The wearer's group in the map, living, within radius of a point; alone, the wearer
std::vector<Player*> AlliesAround(Player* player, WorldObject* center, float radius)
{
    std::vector<Player*> allies;
    auto consider = [&allies, player, center, radius](Player* ally)
    {
        if (ally && ally->IsAlive() && ally->IsInMap(player) && ally->IsWithinDistInMap(center, radius))
            allies.push_back(ally);
    };
    if (Group* group = player->GetGroup())
    {
        for (GroupReference* reference = group->GetFirstMember(); reference; reference = reference->next())
            consider(reference->GetSource());
    }
    else
        consider(player);
    return allies;
}

// The most hurt of them (by share of health), not at full health; nullptr if none
Player* MostHurt(std::vector<Player*> const& allies, Unit const* except = nullptr)
{
    Player* chosen = nullptr;
    for (Player* ally : allies)
        if (ally != except && !ally->IsFullHealth() &&
            (!chosen || ally->GetHealthPct() < chosen->GetHealthPct()))
            chosen = ally;
    return chosen;
}

// A power with a cooldown: whether it may act again (readyAt: getMSTime() from which it may; the clock wraps)
bool Ready(uint32 readyAt, uint32 now)
{
    return int32(now - readyAt) >= 0;
}

bool Matches(Filter filter, SpellInfo const* spellInfo)
{
    if (filter == FILTER_ANY)
        return true;
    bool const weapon = !spellInfo || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE ||
        spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED;
    return filter == FILTER_WEAPON ? weapon : !weapon;
}

bool IsDamageOverTime(SpellInfo const* spellInfo)
{
    return spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE) ||
        spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT) || spellInfo->HasAura(SPELL_AURA_PERIODIC_LEECH));
}

void Cast(Player* player, Unit* target, uint32 spell, int32 amount)
{
    player->CastCustomSpell(target, spell, &amount, nullptr, nullptr, true);
}

// A heal-over-time tick reaches ModifyHealReceived right after ModifyPeriodicDamageAurasTick (SpellAuraEffects.cpp);
// a heal power only takes direct heals, so the tick is marked on its way through
thread_local bool HealTickPending = false;

class LegendaryPlayerScript : public PlayerScript
{
public:
    LegendaryPlayerScript() : PlayerScript("LegendaryPlayerScript", {
        PLAYERHOOK_ON_AFTER_APPLY_ITEM_BONUSES,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_SPELL_CAST
    }) { }

    // Écho du Néant: an ability with a cooldown of 20 sec or more, cast by the player themselves (not triggered, not
    // an item's), echoes: every other ability still cooling down loses the rolled share of what it has left. The
    // client is told each change (its cooldown sweeps jump). Item cooldowns are left alone.
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        SpellInfo const* spellInfo = spell ? spell->GetSpellInfo() : nullptr;
        if (!spellInfo || spell->IsTriggered() || spell->m_CastItem || IsOwnSpell(spellInfo) ||
            std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime) < EchoMinCooldownMs)
            return;
        GetWorn(player)->ForEach(KIND_COOLDOWN_ECHO, [player, spellInfo](Definition const& definition, float percent,
            PowerState& state)
        {
            uint32 const now = getMSTime();
            if (!Ready(state.readyAt, now))
                return;
            state.readyAt = now + EchoRestMs;
            std::vector<std::pair<uint32, int32>> cuts;
            for (auto const& [id, cooldown] : player->GetSpellCooldownMap())
            {
                if (id == spellInfo->Id || cooldown.itemid || int32(cooldown.end - now) <= int32(EchoMinLeftMs))
                    continue;
                cuts.emplace_back(id, -int32(std::lround(float(cooldown.end - now) * percent / 100.0f)));
            }
            for (auto const& [id, cut] : cuts)
                player->ModifySpellCooldown(id, cut);
            if (!cuts.empty() && definition.tuning.spell)
                player->CastSpell(player, definition.tuning.spell, true);
        });
    }

    // A worn copy's own armour and stats, through the core's own item stat code, and its power
    void OnPlayerAfterApplyItemBonuses(Player* player, Item* item, uint8 slot, bool apply) override
    {
        std::optional<Copy> copy = GetCopy(item);
        if (!copy)
            return;
        Mark(item);
        if (copy->armor)
            player->HandleStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(copy->armor), apply);
        for (auto const& [type, value] : copy->stats)
            player->ApplyItemStatMod(type, value, apply);

        Definition const* definition = GetDefinition(copy->legendary);
        if (!definition || slot >= EQUIPMENT_SLOT_END)
            return;
        GetWorn(player)->slots[slot] = apply ? std::make_pair(definition->id, copy->power) :
            std::make_pair(0u, 0.0f);
        // A buff the power keeps up goes with it; a spent debuff stays (taking it off and on is no way around it)
        if (!apply && (definition->kind == KIND_LAST_STAND || definition->kind == KIND_KILL_FRENZY ||
            definition->kind == KIND_SURGE))
            player->RemoveAurasDueToSpell(definition->tuning.spell);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        Worn* worn = GetWorn(player);
        bool const alive = player->IsAlive();

        // Grounds (Consécration de Mograine, Jambières du Dévoreur): every everyMs in combat (at once when a fight
        // starts), the ground where the wearer stands - a persistent area, laid and left - pulsing every second
        worn->ForEach(KIND_GROUND, [player, diff, alive](Definition const& definition, float percent, PowerState& state)
        {
            Tuning const& tuning = definition.tuning;
            if (!alive)
            {
                state.pulsesLeft = 0;
                return;
            }
            if (state.pulsesLeft)
            {
                state.pulseIn = state.pulseIn > diff ? state.pulseIn - diff : 0;
                if (!state.pulseIn)
                {
                    state.pulseIn = 1000;
                    --state.pulsesLeft;
                    // Around the ground, where it was laid; gone (the wearer left the map), nothing more
                    if (DynamicObject* ground = player->GetDynObject(tuning.spell))
                        GroundPulse(player, ground, definition, percent);
                    else
                        state.pulsesLeft = 0;
                }
            }
            if (!player->IsInCombat())
            {
                state.timer = 0;
                return;
            }
            state.timer = state.timer > diff ? state.timer - diff : 0;
            if (state.timer)
                return;
            state.timer = tuning.everyMs;
            state.pulsesLeft = tuning.count;
            state.pulseIn = 0;
            player->CastSpell(player, tuning.spell, true);
        });

        // Novas (Poings de Loken): every everyMs in combat, the nova's look on the wearer and its blow on every
        // enemy around
        worn->ForEach(KIND_PULSE, [player, diff, alive](Definition const& definition, float percent, PowerState& state)
        {
            if (!alive || !player->IsInCombat())
            {
                state.timer = 0;
                return;
            }
            state.timer = state.timer > diff ? state.timer - diff : 0;
            if (state.timer)
                return;
            Tuning const& tuning = definition.tuning;
            state.timer = tuning.everyMs;
            if (tuning.spell2)
                player->CastSpell(player, tuning.spell2, true);
            int32 const amount = Share(PowerOf(player, tuning.spell), percent);
            for (Unit* enemy : EnemiesAround(player, player, tuning.radius))
                Cast(player, enemy, tuning.spell, amount);
        });

        // Allies looked after (Moufles de Cookie): every everyMs in combat, the most hurt one around healed
        worn->ForEach(KIND_RENEW_ALLIES, [player, diff, alive](Definition const& definition, float percent,
            PowerState& state)
        {
            if (!alive || !player->IsInCombat())
            {
                state.timer = 0;
                return;
            }
            state.timer = state.timer > diff ? state.timer - diff : 0;
            if (state.timer)
                return;
            Tuning const& tuning = definition.tuning;
            state.timer = tuning.everyMs;
            if (Player* ally = MostHurt(AlliesAround(player, player, tuning.radius)))
                Cast(player, ally, tuning.spell, Share(PowerOf(player, tuning.spell), percent));
        });

        // Leech (Chevalière de Porung): what the blows have earned, healed once a second - one line a second in the
        // combat log, not one per hit
        worn->ForEach(KIND_LEECH, [player, diff, alive](Definition const& definition, float /*percent*/,
            PowerState& state)
        {
            state.timer = state.timer > diff ? state.timer - diff : 0;
            if (state.timer)
                return;
            state.timer = 1000;
            if (alive && state.pending >= 1.0f && !player->IsFullHealth())
                Cast(player, player, definition.tuning.spell, int32(std::lround(state.pending)));
            state.pending = 0.0f;
        });

        // L'Étoile captive: fed by the wearer's blows and heals, it collapses every everyMs in combat (its buff counts
        // the time down); out of combat it goes out, what it held lost
        worn->ForEach(KIND_SUPERNOVA, [player, diff, alive](Definition const& definition, float /*percent*/,
            PowerState& state)
        {
            Tuning const& tuning = definition.tuning;
            if (!alive || !player->IsInCombat())
            {
                if (state.running)
                    player->RemoveAurasDueToSpell(tuning.spell3);
                state = PowerState();
                return;
            }
            if (!state.running)
            {
                state.running = true;
                state.timer = tuning.everyMs;
                player->CastSpell(player, tuning.spell3, true);
                return;
            }
            state.timer = state.timer > diff ? state.timer - diff : 0;
            if (state.timer)
                return;
            state.timer = tuning.everyMs;
            Collapse(player, definition, state);
            player->CastSpell(player, tuning.spell3, true);
        });

        // Sablier de Perpétuité: every everyMs in combat, a sentence on the target under it; out of combat the count
        // starts over
        worn->ForEach(KIND_SENTENCE, [player, diff, alive](Definition const& definition, float percent,
            PowerState& state)
        {
            if (!alive || !player->IsInCombat())
            {
                state = PowerState();
                return;
            }
            if (!state.running)
            {
                state.running = true;
                state.timer = definition.tuning.everyMs;
                return;
            }
            state.timer = state.timer > diff ? state.timer - diff : 0;
            if (state.timer)
                return;
            state.timer = definition.tuning.everyMs;
            Sentence(player, definition, percent, state);
        });

        // The last stand's look (Tombeau de Keleseth): on while the wearer is below the threshold
        worn->ForEach(KIND_LAST_STAND, [player, alive](Definition const& definition, float /*percent*/,
            PowerState& /*state*/)
        {
            uint32 const spell = definition.tuning.spell;
            bool const low = alive && player->HealthBelowPct(int32(LowHealthPct));
            if (low && !player->HasAura(spell))
                player->CastSpell(player, spell, true);
            else if (!low && player->HasAura(spell))
                player->RemoveAurasDueToSpell(spell);
        });
    }

    // The star collapsing: its damage shared by the wearer's target (else the enemy nearest them) and the enemies
    // around it, its healing by the most hurt allies around the wearer. Damage with no enemy in reach waits for the
    // next collapse; healing with nobody hurt is gone.
    static void Collapse(Player* player, Definition const& definition, PowerState& state)
    {
        Tuning const& tuning = definition.tuning;
        if (state.pending >= 1.0f)
        {
            Unit* target = player->GetSelectedUnit();
            if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target) ||
                !player->IsWithinDistInMap(target, SupernovaTargetReach))
            {
                target = nullptr;
                for (Unit* enemy : EnemiesAround(player, player, SupernovaTargetReach))
                    if (!target || player->GetExactDist(enemy) < player->GetExactDist(target))
                        target = enemy;
            }
            if (target)
            {
                std::list<Unit*> enemies = EnemiesAround(player, target, tuning.radius);
                if (std::find(enemies.begin(), enemies.end(), target) == enemies.end())
                    enemies.push_back(target);
                int32 const share = std::max(1, int32(std::lround(state.pending / float(enemies.size()))));
                for (Unit* enemy : enemies)
                    Cast(player, enemy, tuning.spell, share);
                state.pending = 0.0f;
            }
        }
        if (state.healing >= 1.0f)
        {
            std::vector<Player*> allies = AlliesAround(player, player, SupernovaHealReach);
            std::erase_if(allies, [](Player* ally) { return ally->IsFullHealth(); });
            std::sort(allies.begin(), allies.end(), [](Player* a, Player* b)
            {
                return a->GetHealthPct() < b->GetHealthPct();
            });
            if (allies.size() > tuning.count)
                allies.resize(tuning.count);
            if (!allies.empty())
            {
                int32 const share = std::max(1, int32(std::lround(state.healing / float(allies.size()))));
                for (Player* ally : allies)
                    Cast(player, ally, tuning.spell2, share);
            }
        }
        state.healing = 0.0f;
    }

    // A sentence: the n-th on the same target strikes for n / count of the rolled share of what the wearer dealt it
    // since the last (the whole share from the count-th on), and its mark on the target counts them. A target dead or
    // gone takes the count with it.
    static void Sentence(Player* player, Definition const& definition, float percent, PowerState& state)
    {
        Tuning const& tuning = definition.tuning;
        Unit* target = state.target.IsEmpty() ? nullptr : ObjectAccessor::GetUnit(*player, state.target);
        if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
        {
            state.target.Clear();
            state.counter = 0;
            state.pending = 0.0f;
            return;
        }
        state.counter = std::min(state.counter + 1, tuning.count);
        float const share = percent * float(state.counter) / float(tuning.count);
        if (state.pending >= 1.0f)
            Cast(player, target, tuning.spell, Share(state.pending, share));
        state.pending = 0.0f;
        Aura* mark = target->GetAura(tuning.spell2, player->GetGUID());
        if (!mark)
            mark = player->AddAura(tuning.spell2, target);
        if (mark)
        {
            int32 const lasts = int32(tuning.everyMs) + SentenceMarkGraceMs;
            mark->SetStackAmount(uint8(state.counter));
            mark->SetMaxDuration(lasts);
            mark->SetDuration(lasts);
        }
    }

    // A ground's pulse: what it deals to each enemy and heals each ally on it, the rolled share of the wearer's
    // attack or spell power, whichever is higher
    static void GroundPulse(Player* player, WorldObject* ground, Definition const& definition, float percent)
    {
        Tuning const& tuning = definition.tuning;
        int32 const amount = Share(PowerOf(player, tuning.spell2), percent);
        for (Unit* enemy : EnemiesAround(player, ground, tuning.radius))
            Cast(player, enemy, tuning.spell2, amount);
        if (tuning.spell3)
            for (Player* ally : AlliesAround(player, ground, tuning.radius))
                Cast(player, ally, tuning.spell3, amount);
    }

    // A kill (the killing blow, the wearer's or their pet's)
    void OnPlayerCreatureKill(Player* player, Creature* killed) override
    {
        OnKill(player, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* owner, Creature* killed) override
    {
        OnKill(owner, killed);
    }

    static void OnKill(Player* player, Creature* killed)
    {
        if (!player || !killed || !player->IsAlive())
            return;
        Worn* worn = GetWorn(player);
        // Appel d'Annhylde: the frenzy's buff, renewed; ModifyFinalDamage reads it
        worn->ForEach(KIND_KILL_FRENZY, [player](Definition const& definition, float, PowerState&)
        {
            player->CastSpell(player, definition.tuning.spell, true);
        });
        // Ceinture à poudre de Gilnid: the body blows up
        worn->ForEach(KIND_KILL_NOVA, [player, killed](Definition const& definition, float percent, PowerState&)
        {
            Tuning const& tuning = definition.tuning;
            int32 const amount = Share(PowerOf(player, tuning.spell), percent);
            for (Unit* enemy : EnemiesAround(player, killed, tuning.radius))
                if (enemy != killed)
                    Cast(player, enemy, tuning.spell, amount);
        });
        // Heaume de Bronjahm: the soul taken heals
        worn->ForEach(KIND_KILL_HEAL, [player](Definition const& definition, float percent, PowerState&)
        {
            Tuning const& tuning = definition.tuning;
            Cast(player, player, tuning.spell, std::max(1, int32(std::lround(float(player->GetMaxHealth()) *
                percent / 100.0f / float(std::max(1u, tuning.count))))));
        });
    }

    // Every copy the character carries: marked, and its rolls sent for the tooltips. The base items' records first,
    // as the server has them now: the client keeps the ones it saw (its item cache) and showed a socket the base
    // item lost.
    void OnPlayerLogin(Player* player) override
    {
        if (player->GetSession() && !player->GetSession()->IsBot())
            for (Definition const& definition : Definitions)
            {
                WorldPacket query(CMSG_ITEM_QUERY_SINGLE, 4);
                query << definition.baseItem;
                player->GetSession()->HandleItemQuerySingleOpcode(query);
            }
        ForEachItem(player, [player](Item* item)
        {
            if (std::optional<Copy> copy = GetCopy(item))
            {
                Mark(item);
                SendCopy(player, item->GetGUID().GetCounter(), *copy, item);
            }
        });
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
                                       std::string& message) override
    {
        if (language != LANG_ADDON || message.size() <= Prefix.size() || !message.starts_with(Prefix) ||
            message[Prefix.size()] != '\t')
            return;
        std::vector<std::string_view> fields = Acore::Tokenize(std::string_view(message).substr(Prefix.size() + 1),
            '\t', true);
        if (fields.size() == 2 && fields[0] == "Q")
        {
            if (Optional<uint32> seed = Acore::StringTo<uint32>(fields[1]))
                if (std::optional<Copy> copy = FindCopy(*seed))
                    SendCopy(player, *seed, *copy);
        }
        else if (fields.size() == 3 && fields[0] == "W")
        {
            Optional<uint32> bag = Acore::StringTo<uint32>(fields[1]);
            Optional<uint32> slot = Acore::StringTo<uint32>(fields[2]);
            Item* item = bag && slot && *bag <= 255 && *slot <= 255 ?
                player->GetItemByPos(uint8(*bag), uint8(*slot)) : nullptr;
            if (std::optional<Copy> copy = GetCopy(item))
                SendCopy(player, item->GetGUID().GetCounter(), *copy, item);
        }
    }
};

// The powers on blows and heals. Only direct damage feeds the blow powers (ModifyFinalDamage: swings and spell hits,
// never periodic damage), and none of the powers' own spells feeds any of them: no loop.
class LegendaryUnitScript : public UnitScript
{
public:
    LegendaryUnitScript() : UnitScript("LegendaryUnitScript", true, {
        UNITHOOK_MODIFY_FINAL_DAMAGE,
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_HEAL_RECEIVED
    }) { }

    // Whatever the blow - a hit, damage over time, a fall:
    // - Serment de Whitemane: a blow that would kill the wearer leaves them at 1 health, and the oath heals them for
    //   its rolled share of their health over 4 sec; then it rests for 3 min (its debuff shows how long);
    // - Chevalière de Bjarngrim: a blow that takes them below half health raises a shield of its rolled share of
    //   their health; then it rests for a minute.
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player || !player->IsAlive() || !damage)
            return;
        Worn* worn = GetWorn(player);
        if (damage >= player->GetHealth())
        {
            worn->ForEach(KIND_OATH, [player, &damage](Definition const& definition, float percent, PowerState&)
            {
                Tuning const& tuning = definition.tuning;
                if (damage < player->GetHealth() || player->HasAura(tuning.spell2))
                    return;
                damage = player->GetHealth() - 1;
                Cast(player, player, tuning.spell, std::max(1, int32(std::lround(float(player->GetMaxHealth()) *
                    percent / 100.0f / float(std::max(1u, tuning.count))))));
                player->CastSpell(player, tuning.spell2, true);
            });
            return;
        }
        float const threshold = float(player->GetMaxHealth()) * BulwarkHealthPct / 100.0f;
        if (float(player->GetHealth() - damage) >= threshold)
            return;
        worn->ForEach(KIND_BULWARK, [player](Definition const& definition, float percent, PowerState&)
        {
            Tuning const& tuning = definition.tuning;
            if (player->HasAura(tuning.spell2))
                return;
            Cast(player, player, tuning.spell, Share(float(player->GetMaxHealth()), percent));
            player->CastSpell(player, tuning.spell2, true);
        });
    }

    void ModifyFinalDamage(Unit* attacker, Unit* victim, uint32& damage, uint32& /*absorb*/,
                           SpellInfo const* spellInfo) override
    {
        if (!victim || victim == attacker || !damage || IsOwnSpell(spellInfo))
            return;
        if (Player* wearer = victim->ToPlayer())
            Taken(wearer, attacker, damage, spellInfo);
        if (Player* player = attacker ? attacker->ToPlayer() : nullptr)
            if (victim->IsAlive())
                Dealt(player, victim, damage, spellInfo);
    }

    // Damage over time: the wearer's grows (Bandelettes de Nethekurse, a frenzy), the wearer's last stand eases what
    // they take. The hook also carries heal-over-time ticks: those only get marked for ModifyHealReceived.
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (spellInfo && spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL))
        {
            HealTickPending = true;
            return;
        }
        if (!IsDamageOverTime(spellInfo) || IsOwnSpell(spellInfo) || !damage || target == attacker)
            return;
        if (Player* player = attacker ? attacker->ToPlayer() : nullptr)
        {
            Worn* worn = GetWorn(player);
            float bonus = 0.0f;
            worn->ForEach(KIND_DOT_FEAST, [&bonus](Definition const&, float percent, PowerState&)
            {
                bonus += percent;
            });
            worn->ForEach(KIND_KILL_FRENZY, [player, &bonus](Definition const& definition, float percent, PowerState&)
            {
                if (player->HasAura(definition.tuning.spell))
                    bonus += percent;
            });
            if (bonus > 0.0f)
                damage = uint32(std::lround(float(damage) * (1.0f + bonus / 100.0f)));
            uint32 const dealt = damage;
            worn->ForEach(KIND_SUPERNOVA, [dealt](Definition const&, float percent, PowerState& state)
            {
                state.pending += float(dealt) * percent / 100.0f;
            });
            if (target)
                FeedSentence(player, worn, target, float(dealt));
        }
        if (Player* wearer = target ? target->ToPlayer() : nullptr)
            damage = LastStand(wearer, damage);
    }

    // Direct heals by a wearer (a heal-over-time tick, marked on its way, is not one):
    // - Robe de Novos: the rolled share of the overhealing shields the target, added to what is left of the shield;
    // - Pendentif de Tharon'ja: the rolled share of the heal also heals the most hurt other ally around.
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        bool const tick = HealTickPending;
        HealTickPending = false;
        Player* player = healer ? healer->ToPlayer() : nullptr;
        if (!player || !target || !heal || !spellInfo || IsOwnSpell(spellInfo) || !target->IsAlive())
            return;
        Worn* worn = GetWorn(player);
        // L'Étoile captive: what the heal really heals (no overhealing), a tick's too
        uint32 const healed = std::min(heal, target->GetMaxHealth() - target->GetHealth());
        worn->ForEach(KIND_SUPERNOVA, [healed](Definition const&, float percent, PowerState& state)
        {
            state.healing += float(healed) * percent / 100.0f;
        });
        if (tick)
            return;
        worn->ForEach(KIND_OVERHEAL_SHIELD, [player, target, heal](Definition const& definition, float percent,
            PowerState&)
        {
            uint32 const missing = target->GetMaxHealth() - target->GetHealth();
            if (heal <= missing)
                return;
            uint32 const spell = definition.tuning.spell;
            float shield = float(heal - missing) * percent / 100.0f;
            if (AuraEffect const* existing = target->GetAuraEffect(spell, EFFECT_0, player->GetGUID()))
                shield += float(existing->GetAmount());
            shield = std::min(shield, float(target->GetMaxHealth()) * OverhealShieldCapPct / 100.0f);
            if (shield >= 1.0f)
                Cast(player, target, spell, int32(std::lround(shield)));
        });
        worn->ForEach(KIND_HEAL_SPLASH, [player, target, heal](Definition const& definition, float percent,
            PowerState&)
        {
            Tuning const& tuning = definition.tuning;
            if (Player* ally = MostHurt(AlliesAround(player, player, tuning.radius), target))
                Cast(player, ally, tuning.spell, Share(float(heal), percent));
        });
    }

private:
    // Sablier de Perpétuité: the wearer's target is the one under sentence - a blow on the enemy they have selected
    // moves the sentence onto it, the count starting over (with none selected, the first enemy struck) - and what it
    // takes from them is kept for its next sentence
    static void FeedSentence(Player* player, Worn* worn, Unit* victim, float dealt)
    {
        worn->ForEach(KIND_SENTENCE, [player, victim, dealt](Definition const&, float, PowerState& state)
        {
            ObjectGuid const guid = victim->GetGUID();
            if (guid != state.target && (state.target.IsEmpty() || player->GetTarget() == guid))
            {
                state.target = guid;
                state.counter = 0;
                state.pending = 0.0f;
            }
            if (guid == state.target)
                state.pending += dealt;
        });
    }

    // What a wearer's blow becomes: amplified first (Plastron de VanCleef, a frenzy), then what it sets off
    static void Dealt(Player* player, Unit* victim, uint32& damage, SpellInfo const* spellInfo)
    {
        Worn* worn = GetWorn(player);
        float bonus = 0.0f;
        if (victim->HealthBelowPct(int32(LowHealthPct)))
            worn->ForEach(KIND_EXECUTE, [&bonus](Definition const&, float percent, PowerState&) { bonus += percent; });
        worn->ForEach(KIND_KILL_FRENZY, [player, &bonus](Definition const& definition, float percent, PowerState&)
        {
            if (player->HasAura(definition.tuning.spell))
                bonus += percent;
        });
        if (bonus > 0.0f)
            damage = uint32(std::lround(float(damage) * (1.0f + bonus / 100.0f)));
        float const dealt = float(damage);

        // Brands (Marque de l'Inquisiteur, Brassards de Sepethrea, Griffes du roi Dred): the share burns over the
        // spell's duration; the core's Ignite rolls what is left of the burn into the new one
        worn->ForEach(KIND_BRAND, [player, victim, dealt, spellInfo](Definition const& definition, float percent,
            PowerState&)
        {
            Tuning const& tuning = definition.tuning;
            if (!Matches(tuning.filter, spellInfo))
                return;
            int32 const perTick = int32(std::lround(dealt * percent / 100.0f / float(std::max(1u, tuning.count))));
            if (perTick > 0)
                victim->CastDelayedSpellWithPeriodicAmount(player, tuning.spell, SPELL_AURA_PERIODIC_DAMAGE, perTick);
        });
        // Ceinture d'Ingvar: every count-th hit, a blow of its own
        worn->ForEach(KIND_ECHO, [player, victim](Definition const& definition, float percent, PowerState& state)
        {
            Tuning const& tuning = definition.tuning;
            if (++state.counter < tuning.count)
                return;
            state.counter = 0;
            Cast(player, victim, tuning.spell, Share(PowerOf(player, tuning.spell), percent));
        });
        // Poignes de Kargath: the share on enemies around the target
        worn->ForEach(KIND_CLEAVE, [player, victim, dealt](Definition const& definition, float percent, PowerState&)
        {
            Tuning const& tuning = definition.tuning;
            uint32 hit = 0;
            for (Unit* enemy : EnemiesAround(player, victim, tuning.radius))
                if (enemy != victim && hit < tuning.count)
                {
                    ++hit;
                    Cast(player, enemy, tuning.spell, Share(dealt, percent));
                }
        });
        // Chains (Étincelle d'Ionar, L'Âme reflétée): the share leaps to other enemies, nearest first
        worn->ForEach(KIND_CHAIN, [player, victim, dealt](Definition const& definition, float percent,
            PowerState& state)
        {
            Tuning const& tuning = definition.tuning;
            uint32 const now = getMSTime();
            if (tuning.cooldownMs && !Ready(state.readyAt, now))
                return;
            std::list<Unit*> enemies = EnemiesAround(player, victim, tuning.radius);
            enemies.remove(victim);
            if (enemies.empty())
                return;
            enemies.sort([victim](Unit* a, Unit* b) { return victim->GetExactDist(a) < victim->GetExactDist(b); });
            state.readyAt = now + tuning.cooldownMs;
            uint32 hit = 0;
            for (Unit* enemy : enemies)
            {
                if (hit++ >= tuning.count)
                    break;
                Cast(player, enemy, tuning.spell, Share(dealt, percent));
            }
        });
        // L'Étoile captive: the share feeds the star (OnPlayerUpdate collapses it)
        worn->ForEach(KIND_SUPERNOVA, [dealt](Definition const&, float percent, PowerState& state)
        {
            state.pending += dealt * percent / 100.0f;
        });
        // Sablier de Perpétuité: what the target under sentence takes waits for its next sentence
        FeedSentence(player, worn, victim, dealt);
        // Chevalière de Porung: the share waits to be healed (OnPlayerUpdate, once a second)
        worn->ForEach(KIND_LEECH, [dealt](Definition const&, float percent, PowerState& state)
        {
            state.pending += dealt * percent / 100.0f;
        });
        // Abaque de Pathaleon: a chance of the surge, then its rest
        worn->ForEach(KIND_SURGE, [player](Definition const& definition, float percent, PowerState& state)
        {
            Tuning const& tuning = definition.tuning;
            uint32 const now = getMSTime();
            if (!Ready(state.readyAt, now) || !roll_chance_f(tuning.chance))
                return;
            state.readyAt = now + tuning.cooldownMs;
            int32 const haste = std::max(1, int32(std::lround(percent)));
            player->CastCustomSpell(player, tuning.spell, &haste, &haste, nullptr, true);
        });
    }

    // What a wearer's taken blow becomes: eased by a last stand, struck back by thorns (melee swings: no spell)
    static void Taken(Player* wearer, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
    {
        damage = LastStand(wearer, damage);
        if (spellInfo || !attacker || !attacker->IsAlive() || !damage)
            return;
        uint32 const taken = damage;
        GetWorn(wearer)->ForEach(KIND_THORNS, [wearer, attacker, taken](Definition const& definition, float percent,
            PowerState&)
        {
            if (wearer->IsValidAttackTarget(attacker))
                Cast(wearer, attacker, definition.tuning.spell, Share(float(taken), percent));
        });
    }

    // Tombeau de Keleseth: below the threshold, the rolled share less
    static uint32 LastStand(Player* wearer, uint32 damage)
    {
        if (!wearer->HealthBelowPct(int32(LowHealthPct)))
            return damage;
        float reduction = 0.0f;
        GetWorn(wearer)->ForEach(KIND_LAST_STAND, [&reduction](Definition const&, float percent, PowerState&)
        {
            reduction += percent;
        });
        if (reduction <= 0.0f)
            return damage;
        return uint32(std::lround(float(damage) * std::max(0.0f, 1.0f - reduction / 100.0f)));
    }
};

class LegendaryWorldScript : public WorldScript
{
public:
    LegendaryWorldScript() : WorldScript("LegendaryWorldScript", { WORLDHOOK_ON_STARTUP }) { }

    // Every copy in memory before anyone logs in: a login applies a worn copy's stats as the inventory loads, before
    // any script of the player could read them. A copy whose item is gone is dropped.
    void OnStartup() override
    {
        uint32 const startTime = getMSTime();
        CharacterDatabase.DirectExecute("DELETE l FROM character_legendary l LEFT JOIN item_instance i "
            "ON i.guid = l.item_guid WHERE i.guid IS NULL");
        std::unique_lock lock(StoreLock);
        Store.clear();
        if (QueryResult result = CharacterDatabase.Query(
            "SELECT item_guid, legendary, item_level, power, armor, stats FROM character_legendary"))
        {
            do
            {
                Field* fields = result->Fetch();
                Copy copy;
                copy.legendary = fields[1].Get<uint32>();
                // A legendary no longer defined (Vorhan's set pieces, generated items now: SetPieces.cpp)
                if (!GetDefinition(copy.legendary))
                    continue;
                copy.itemLevel = fields[2].Get<uint32>();
                copy.power = fields[3].Get<float>();
                copy.armor = fields[4].Get<int32>();
                copy.stats = DecodeStats(fields[5].Get<std::string>());
                Store[fields[0].Get<uint32>()] = std::move(copy);
            } while (result->NextRow());
        }
        Fails.clear();
        CharacterDatabase.DirectExecute("DELETE u FROM character_legendary_upgrade u LEFT JOIN item_instance i "
            "ON i.guid = u.item_guid WHERE i.guid IS NULL");
        if (QueryResult result = CharacterDatabase.Query("SELECT item_guid, fails FROM character_legendary_upgrade"))
        {
            do
            {
                Field* fields = result->Fetch();
                Fails[fields[0].Get<uint32>()] = fields[1].Get<uint32>();
            } while (result->NextRow());
        }
        LOG_INFO("server.loading", ">> Loaded {} legendary copies in {} ms", Store.size(),
            GetMSTimeDiffToNow(startTime));
    }
};

// A deleted item takes its copy with it; a copy has its own item level
class LegendaryGlobalScript : public GlobalScript
{
public:
    LegendaryGlobalScript() : GlobalScript("LegendaryGlobalScript", {
        GLOBALHOOK_ON_ITEM_DEL_FROM_DB,
        GLOBALHOOK_ON_ITEM_LEVEL
    }) { }

    // A copy counts at its own item level, not its base item's (the average item level: the Défis' and the Dungeon
    // Finder's requirements)
    void OnItemLevel(Item const* item, uint32& itemLevel) override
    {
        if (std::optional<Copy> copy = GetCopy(item))
            itemLevel = copy->itemLevel;
    }

    void OnItemDelFromDB(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid) override
    {
        transaction->Append("DELETE FROM character_legendary WHERE item_guid = {}", itemGuid);
        transaction->Append("DELETE FROM character_legendary_upgrade WHERE item_guid = {}", itemGuid);
        std::unique_lock lock(StoreLock);
        Store.erase(itemGuid);
        Fails.erase(itemGuid);
    }
};

// Whether a power is one for the role: healing ones for a healer, a tank's survival for a tank, the rest for
// damage dealers (and tanks: their threat). Kinds that serve everyone (Supernova) suit every role.
bool PowerSuitsRole(Kind kind, uint8 role)
{
    constexpr uint8 Tank = 0;
    constexpr uint8 Healer = 4;
    bool const healing = kind == KIND_RENEW_ALLIES || kind == KIND_OVERHEAL_SHIELD || kind == KIND_HEAL_SPLASH;
    bool const survival = kind == KIND_THORNS || kind == KIND_LAST_STAND || kind == KIND_BULWARK ||
        kind == KIND_OATH || kind == KIND_LEECH || kind == KIND_KILL_HEAL;
    if (kind == KIND_SUPERNOVA || kind == KIND_COOLDOWN_ECHO)
        return true;
    if (role == Healer)
        return healing;
    if (role == Tank)
        return !healing;
    return !healing && !survival;
}

// One of the pool, among those whose power suits the player's role when there are any (a tank was given a healer's
// power, a healer a damage dealer's)
Definition const* PickForRole(std::vector<Definition const*> const& pool, Player* player)
{
    uint8 const role = LootRoleOfPlayer(player);
    std::vector<Definition const*> suited;
    for (Definition const* definition : pool)
        if (PowerSuitsRole(definition->kind, role))
            suited.push_back(definition);
    std::vector<Definition const*> const& from = suited.empty() ? pool : suited;
    return from[urand(0, uint32(from.size() - 1))];
}

// Every real player in a map rolls once against their luck at a source (a dungeon id or a boss entry); a drop picks
// one of the pool (PickForRole) and lands on the floor (GroundLoot) at the item level given.
void RollDrops(Map* map, uint32 source, std::vector<Definition const*> const& pool, uint32 itemLevel)
{
    map->DoForAllPlayers([&pool, itemLevel, source](Player* player)
    {
        if (!player->GetSession() || player->GetSession()->IsBot())
            return;
        ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
        uint32 misses = 0;
        if (QueryResult result = CharacterDatabase.Query("SELECT misses FROM character_legendary_luck "
            "WHERE guid = {} AND source = {}", guid, source))
            misses = result->Fetch()[0].Get<uint32>();
        float const chance = std::min(float(DropCapPct), float(DropBasePct) + float(DropStepPct) * float(misses));
        bool const dropped = frand(0.0f, 100.0f) < chance;
        CharacterDatabase.Execute("REPLACE INTO character_legendary_luck (guid, source, misses) "
            "VALUES ({}, {}, {})", guid, source, dropped ? 0 : misses + 1);
        LOG_INFO("module", "Legendary: {} at source {} at {}%: {}", player->GetName(), source, chance,
            dropped ? "dropped" : "nothing");
        if (!dropped)
            return;

        Definition const* definition = PickForRole(pool, player);
        uint32 const id = definition->id;
        ItemTemplate const* base = sObjectMgr->GetItemTemplate(definition->baseItem);
        ObjectGuid const owner = player->GetGUID();
        bool const thrown = base && GroundLoot::Throw(player, base, [owner, id, itemLevel](Item* item)
        {
            if (Player* looter = ObjectAccessor::FindConnectedPlayer(owner))
                MakeCopy(looter, item, id, itemLevel);
        });
        if (!thrown)
            GiveLegendary(player, id, itemLevel);
    });
}

// Once per instance and source
std::mutex RolledLock;
std::set<std::pair<uint32, uint32>> Rolled;

bool FirstRoll(Map* map, uint32 source)
{
    std::lock_guard lock(RolledLock);
    return Rolled.insert({ map->GetInstanceId(), source }).second;
}

// The legendaries of a dungeon, when a Mythic+ key of it is completed, at the key's item level. The luck is kept
// per character and source (character_legendary_luck: the dungeon's id).
class LegendaryDropScript : public GlobalScript
{
public:
    LegendaryDropScript() : GlobalScript("LegendaryDropScript", { GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE }) { }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType /*type*/, uint32 /*creditEntry*/, Unit* source,
        Difficulty /*difficulty*/, std::list<DungeonEncounter const*> const* /*encounters*/, uint32 dungeonCompleted,
        bool /*updated*/) override
    {
        int32 const level = map->GetMythicLevel();
        if (!dungeonCompleted || level <= 0)
            return;
        // The reinforcing material, for every key at 250 and up, whatever its dungeon's legendaries
        uint32 const itemLevel = Mythic::GetItemLevel(level);
        if (itemLevel >= MaterialMinItemLevel && FirstRoll(map, MaterialSource | dungeonCompleted))
        {
            if (Creature* corpse = source ? source->ToCreature() : nullptr)
                GroundLoot::Open(corpse);
            map->DoForAllPlayers([itemLevel](Player* player) { RollUpgradeMaterial(player, itemLevel); });
        }
        std::vector<Definition const*> pool;
        for (Definition const& definition : Definitions)
            if (definition.sourceDungeon == dungeonCompleted)
                pool.push_back(&definition);
        if (pool.empty() || !FirstRoll(map, dungeonCompleted))
            return;
        // The last boss's loot opened first: the death hooks run in no set order, and a legendary rolled before the
        // ground loot had opened its corpse went straight to the bags - no beam, no sound
        if (Creature* corpse = source ? source->ToCreature() : nullptr)
            GroundLoot::Open(corpse);
        RollDrops(map, dungeonCompleted, pool, itemLevel);
    }
};

// The legendaries of a boss, when it dies (the Hollow Voice: Archbishop Aldric), at its loot's item level - the
// floor of their window. The luck is kept by the boss's entry.
class LegendaryBossDropScript : public UnitScript
{
public:
    LegendaryBossDropScript() : UnitScript("LegendaryBossDropScript", true, { UNITHOOK_ON_UNIT_DEATH }) { }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* boss = unit ? unit->ToCreature() : nullptr;
        if (!boss || !boss->GetMap())
            return;
        RollRaidBossMaterial(boss);
        uint32 const entry = boss->GetEntry();
        std::vector<Definition const*> pool;
        for (Definition const& definition : Definitions)
            if (definition.sourceBoss == entry)
                pool.push_back(&definition);
        if (pool.empty() || !FirstRoll(boss->GetMap(), entry))
            return;
        // Its loot opened first (as above): the Hollow Voice's Unique went to the bags unseen
        GroundLoot::Open(boss);
        uint32 const itemLevel = entry == InfiniteGodBoss ?
            GetChallengeGodItemLevel(GetChallengeTierOf(boss->GetMap())) : pool.front()->floorItemLevel;
        RollDrops(boss->GetMap(), entry, pool, itemLevel);
    }

private:
    // A stock raid's boss whose gear is item level 250 or more (its raid mode's, as the Défis know it: Icecrown
    // Citadel, the Ruby Sanctum, Trial of the Grand Crusader): the material's chance for each player there. Not in a
    // Défi's instance: the board rolls its own (ChallengeBoard.cpp).
    static void RollRaidBossMaterial(Creature* boss)
    {
        Map* map = boss->GetMap();
        if (!map->IsRaid() || (!boss->IsDungeonBoss() && !boss->isWorldBoss()) || GetChallengeBossOf(map))
            return;
        uint32 const itemLevel = RaidFinder::GetChallengeItemLevel(uint16(map->GetId()), uint8(map->GetDifficulty()));
        if (itemLevel < MaterialMinItemLevel || !FirstRoll(map, MaterialSource | boss->GetEntry()))
            return;
        map->DoForAllPlayers([itemLevel](Player* player) { RollUpgradeMaterial(player, itemLevel); });
    }
};

using namespace Acore::ChatCommands;

class LegendaryCommandScript : public CommandScript
{
public:
    LegendaryCommandScript() : CommandScript("LegendaryCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable legendaryTable =
        {
            { "add", HandleAdd, SEC_GAMEMASTER, Console::No },
            { "all", HandleAll, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "legendary", legendaryTable },
        };
        return commandTable;
    }

    // .legendary add <legendary> [item level] [power %]: a rolled copy for the selected player (or yourself)
    static bool HandleAdd(ChatHandler* handler, uint32 legendary, Optional<uint32> itemLevel, Optional<float> power)
    {
        Definition const* definition = GetDefinition(legendary);
        if (!definition)
        {
            handler->PSendSysMessage("Légendaire inconnu : {}.", legendary);
            return false;
        }
        Player* target = handler->getSelectedPlayerOrSelf();
        uint32 const level = itemLevel.value_or(definition->floorItemLevel);
        Item* item = GiveLegendary(target, legendary, level, power ? std::optional<float>(*power) : std::nullopt);
        if (!item)
        {
            handler->SendSysMessage("Sacs pleins.");
            return false;
        }
        std::optional<Copy> copy = GetCopy(item);
        auto const [low, high] = PowerWindow(*definition, level);
        handler->PSendSysMessage("{} : niveau d'objet {}, pouvoir {:.1f}% (fenêtre {:.1f}-{:.1f}%), {}.",
            target->GetName(), level, copy ? copy->power : 0.0f, low, high, copy ? EncodeStats(*copy) : "");
        return true;
    }

    // .legendary all [item level] [power %]: a rolled copy of every legendary for the selected player (or yourself),
    // a test kit; stops when the bags are full and says how far it got
    static bool HandleAll(ChatHandler* handler, Optional<uint32> itemLevel, Optional<float> power)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        uint32 given = 0;
        for (Definition const& definition : Definitions)
        {
            uint32 const level = itemLevel.value_or(definition.floorItemLevel);
            if (!GiveLegendary(target, definition.id, level, power ? std::optional<float>(*power) : std::nullopt))
            {
                handler->PSendSysMessage("Sacs pleins : {} légendaires sur {} donnés (le suivant : {}).", given,
                    Definitions.size(), definition.id);
                return given > 0;
            }
            ++given;
        }
        handler->PSendSysMessage("{} : les {} légendaires donnés.", target->GetName(), given);
        return true;
    }
};
}

Definition const* GetDefinition(uint32 id)
{
    for (Definition const& definition : Definitions)
        if (definition.id == id)
            return &definition;
    return nullptr;
}

Definition const* GetDefinitionByItem(uint32 baseItem)
{
    for (Definition const& definition : Definitions)
        if (definition.baseItem == baseItem)
            return &definition;
    return nullptr;
}

std::optional<Copy> GetCopy(Item const* item)
{
    if (!item || !GetDefinitionByItem(item->GetEntry()))
        return std::nullopt;
    std::optional<Copy> copy = FindCopy(item->GetGUID().GetCounter());
    // A reused guid of another legendary's base item: not this copy
    Definition const* definition = copy ? GetDefinition(copy->legendary) : nullptr;
    if (copy && (!definition || definition->baseItem != item->GetEntry()))
        return std::nullopt;
    return copy;
}

// A copy rolled for an item: kept in memory and in the database, the item marked, the client told
static void Keep(Player* player, Item* item, Copy const& copy)
{
    ObjectGuid::LowType const guid = item->GetGUID().GetCounter();
    {
        std::unique_lock lock(StoreLock);
        Store[guid] = copy;
    }
    Save(guid, player->GetGUID().GetCounter(), copy);
    Mark(item);
    SendCopy(player, guid, copy, item->IsInWorld() ? item : nullptr);
}

std::pair<float, float> PowerWindow(Definition const& definition, uint32 itemLevel)
{
    float const span = float(TopItemLevel) - float(definition.floorItemLevel);
    float const t = span > 0.0f ?
        std::clamp((float(itemLevel) - float(definition.floorItemLevel)) / span, 0.0f, 1.0f) : 1.0f;
    return { definition.bottomLow + (definition.topLow - definition.bottomLow) * t,
             definition.bottomHigh + (definition.topHigh - definition.bottomHigh) * t };
}

Item* GiveLegendary(Player* player, uint32 legendary, uint32 itemLevel, std::optional<float> powerOverride)
{
    Definition const* definition = GetDefinition(legendary);
    if (!player || !definition)
        return nullptr;
    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, definition->baseItem, 1) != EQUIP_ERR_OK)
        return nullptr;

    // Rolled before the item exists: its primary stat reads the gear worn now
    Copy copy = Roll(*definition, player, itemLevel, powerOverride);
    Item* item = player->StoreNewItem(destination, definition->baseItem, true);
    if (!item)
        return nullptr;
    Keep(player, item, copy);
    // Its gear bonuses (health, fortune...) as a dropped item's, at the copy's item level (kept just above, read by
    // OnItemLevel). Thrown on the floor, the mythic item's own path rolls them (MythicDungeonSystem.cpp
    // StoreSelectedMythicItem): a legendary given straight to the bags had none.
    TryRollPersonalLoot(player, item);
    player->SendNewItem(item, 1, true, false);
    return item;
}

void MakeCopy(Player* player, Item* item, uint32 legendary, uint32 itemLevel)
{
    Definition const* definition = GetDefinition(legendary);
    if (!player || !item || !definition || item->GetEntry() != definition->baseItem)
        return;
    Keep(player, item, Roll(*definition, player, itemLevel, std::nullopt));
    TryRollPersonalLoot(player, item);
}

uint32 ArmorType(Player* player)
{
    if (player->HasSkill(SKILL_PLATE_MAIL))
        return 3;
    if (player->HasSkill(SKILL_MAIL))
        return 2;
    if (player->HasSkill(SKILL_LEATHER))
        return 1;
    return 0;
}

// The primary stat a copy takes: the player's role's (mod-stat-growth LootFit). Read from the gear worn, a tank given a
// caster's trinket or two was steered further towards intellect with every drop.
uint32 FavouredPrimary(Player* player)
{
    return LootPrimaryStatOf(player);
}

uint32 UpgradeCap()
{
    return GetChallengeTopItemLevel();
}

std::optional<Upgrade> GetUpgrade(Item const* item)
{
    std::optional<Copy> copy = GetCopy(item);
    if (!copy)
        return std::nullopt;
    Upgrade upgrade;
    upgrade.itemLevel = copy->itemLevel;
    upgrade.cap = UpgradeCap();
    {
        std::shared_lock lock(StoreLock);
        auto const found = Fails.find(item->GetGUID().GetCounter());
        upgrade.fails = found != Fails.end() ? found->second : 0;
    }
    if (copy->itemLevel < upgrade.cap)
    {
        upgrade.nextItemLevel = std::min(upgrade.cap, copy->itemLevel + UpgradeStep);
        upgrade.chance = UpgradeChance(copy->itemLevel, upgrade.cap, upgrade.fails);
    }
    return upgrade;
}

ReinforceResult Reinforce(Player* player, Item* item)
{
    std::optional<Copy> copy = GetCopy(item);
    Definition const* definition = copy ? GetDefinition(copy->legendary) : nullptr;
    std::optional<Upgrade> upgrade = GetUpgrade(item);
    if (!definition || !upgrade)
        return ReinforceResult::NotLegendary;
    if (!upgrade->nextItemLevel)
        return ReinforceResult::AtCap;
    if (!player->HasItemCount(UpgradeMaterial, 1))
        return ReinforceResult::NoMaterial;

    player->DestroyItemCount(UpgradeMaterial, 1, true);
    ObjectGuid::LowType const guid = item->GetGUID().GetCounter();
    bool const success = roll_chance_f(upgrade->chance);
    LOG_INFO("module", "Legendary: {} reinforced item {} (legendary {}, item level {}, {:.1f}% after {} failures): {}",
        player->GetName(), guid, copy->legendary, copy->itemLevel, upgrade->chance, upgrade->fails,
        success ? "success" : "failure");
    if (!success)
    {
        {
            std::unique_lock lock(StoreLock);
            Fails[guid] = upgrade->fails + 1;
        }
        CharacterDatabase.Execute("REPLACE INTO character_legendary_upgrade (item_guid, fails) VALUES ({}, {})", guid,
            upgrade->fails + 1);
        player->SaveToDB(false, false);
        return ReinforceResult::Failure;
    }

    // Worn: its stats taken off at the old level and put back at the new one (OnPlayerAfterApplyItemBonuses)
    Copy const grown = Grow(*definition, *copy, upgrade->nextItemLevel);
    bool const equipped = item->IsEquipped();
    uint8 const slot = item->GetSlot();
    if (equipped)
        player->_ApplyItemMods(item, slot, false);
    {
        std::unique_lock lock(StoreLock);
        Store[guid] = grown;
        Fails.erase(guid);
    }
    if (equipped)
        player->_ApplyItemMods(item, slot, true);
    Save(guid, player->GetGUID().GetCounter(), grown);
    CharacterDatabase.Execute("DELETE FROM character_legendary_upgrade WHERE item_guid = {}", guid);
    SendCopy(player, guid, grown, item);
    // Saved now: the material is gone, the copy must not come back unreinforced after a crash
    player->SaveToDB(false, false);
    return ReinforceResult::Success;
}

void RollUpgradeMaterial(Player* player, uint32 itemLevel)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot() || itemLevel < MaterialMinItemLevel)
        return;
    // Every roll in the log, the misses too: what a player's runs gave them can be read back
    if (!roll_chance_f(float(MaterialDropPct)))
    {
        LOG_INFO("module", "Legendary: {} rolled for the reinforcing material (item level {}, {:.0f}%): nothing",
            player->GetName(), itemLevel, float(MaterialDropPct));
        return;
    }
    bool const french = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
    // On the floor with the boss's loot (GroundLoot.cpp: a dungeon's or a Défi's boss just killed), as a legendary
    bool const thrown = GroundLoot::ThrowItem(player, UpgradeMaterial, 1, ITEM_QUALITY_LEGENDARY);
    ItemPosCountVec destination;
    if (!thrown && player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, UpgradeMaterial, 1) == EQUIP_ERR_OK)
    {
        if (Item* item = player->StoreNewItem(destination, UpgradeMaterial, true))
            player->SendNewItem(item, 1, true, false, true);
    }
    else if (!thrown)
    {
        CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
        MailDraft draft(french ? "Cœur d'étoile captive" : "Captive Star Heart",
            french ? "Vos sacs étaient pleins." : "Your bags were full.");
        if (Item* item = Item::CreateItem(UpgradeMaterial, 1, player))
        {
            item->SaveToDB(transaction);
            draft.AddItem(item);
        }
        draft.SendMailTo(transaction, MailReceiver(player, player->GetGUID().GetCounter()),
            MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
        CharacterDatabase.CommitTransaction(transaction);
    }
    LOG_INFO("module", "Legendary: {} found the reinforcing material (item level {}){}", player->GetName(),
        itemLevel, thrown ? ", on the floor" : "");
}
}

void AddLegendarySetPieceScripts();

void AddLegendaryScripts()
{
    new Legendary::LegendaryPlayerScript();
    new Legendary::LegendaryUnitScript();
    new Legendary::LegendaryWorldScript();
    new Legendary::LegendaryGlobalScript();
    new Legendary::LegendaryDropScript();
    new Legendary::LegendaryBossDropScript();
    new Legendary::LegendaryCommandScript();
    AddLegendarySetPieceScripts();
}
