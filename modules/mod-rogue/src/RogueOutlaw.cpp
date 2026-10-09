#include "AllSpellScript.h"
#include "CellImpl.h"
#include "Containers.h"
#include "Creature.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LiveTuning.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "RogueShared.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <list>
#include <string_view>
#include <vector>

// mod-stat-growth's EvolutionsAudio.h (our own sound engine, .agents/docs/systems/evolutions-audio.md): a sound of its
// bank played for one player, following an object. The Outlaw sounds are modules/mod-rogue/client-assets/audio.
namespace EvolutionsAudio
{
void PlayOn(Player* player, std::string_view key, WorldObject const* source);
}

// Hors-la-loi: the Rogue's third tree reworked into retail's Outlaw (.agents/plans/outlaw-rogue/outlaw-rogue.PLAN.md).
// Everything here is the spec's: a rogue whose specialization aura (92194) is on. Spell data in
// localTools/rogue/Spells.ps1, the tree's nodes in localTools/rogue/talentTree.json; a talent is its rank spell's aura.
//
// The damage of the kit is worked out here. An ability whose data carries a dummy effect (as Black Powder) has it
// dealt here; one with a damage effect of its own (weapon or school damage) has that hit's amount replaced by the same
// number (ModifySpellDamageTaken), so the data may take either shape. Every number is a `rogue.outlaw_*` knob.
//
// Blade Flurry repeats the rogue's single-target hits on the enemies around: auto attacks and melee / ranged spells
// through ModifyFinalDamage, the kit's own hits (dealt here) explicitly. A repeat is dealt as plain damage (no spell
// cast, no hook but OnDamage): it never repeats itself nor triggers any proc. Main Gauche's strike and the other extra
// damage are dealt one tick later (an event on the rogue), never inside the hit that caused them.
namespace
{
// The spec's abilities and auras (localTools/rogue/Spells.ps1)
enum OutlawSpells : uint32
{
    SPELL_OUTLAW_PASSIVE        = 92194,
    SPELL_PISTOL_SHOT           = 92342,
    SPELL_DISPATCH              = 92343,
    SPELL_BETWEEN_THE_EYES      = 92344,
    SPELL_ROLL_THE_BONES        = 92345,
    SPELL_BLADE_FLURRY          = 92346,
    SPELL_ADRENALINE_RUSH       = 92347,
    SPELL_BLADE_RUSH            = 92348,
    SPELL_KILLING_SPREE         = 92349,
    SPELL_KEEP_IT_ROLLING       = 92350,
    SPELL_DREADBLADES           = 92351,
    SPELL_OPPORTUNITY           = 92352,
    SPELL_AUDACITY              = 92353,
    SPELL_BROADSIDE             = 92354,
    SPELL_BURIED_TREASURE       = 92355,
    SPELL_GRAND_MELEE           = 92356,
    SPELL_RUTHLESS_PRECISION    = 92357,
    SPELL_SKULL_AND_CROSSBONES  = 92358,
    SPELL_TRUE_BEARING          = 92359,
    SPELL_GREENSKINS_WICKERS    = 92360,
    SPELL_BLADE_RUSH_ENERGY     = 92361,
    SPELL_PISTOL_SHOT_SLOW      = 92362,
    // Hidden helpers from the plan's spare ids; each one falls back when the data has not got it
    SPELL_BLADE_FLURRY_HIT      = 92363,    // the repeat's name in the log and the meters (else Blade Flurry's)
    SPELL_BETWEEN_THE_EYES_CRIT = 92364,    // Between the Eyes' critical strike buff (else none)
    SPELL_BETWEEN_THE_EYES_STUN = 92365,    // its stun when its own data has none (else Cheap Shot's)
    SPELL_MAIN_GAUCHE_HIT       = 92366,    // Main Gauche's strike's name (else the talent's first rank)
    SPELL_LOADED_DICE           = 92367,    // Dés pipés' pending extra roll, shown (else kept in the state only)

    // Stock
    SPELL_SINISTER_STRIKE       = 1752,
    SPELL_EVISCERATE            = 2098,
    SPELL_SLICE_AND_DICE        = 5171,
    SPELL_AMBUSH                = 8676,
    SPELL_CHEAP_SHOT            = 1833,
    SPELL_GHOSTLY_STRIKE        = 14278,
};

// The Hors-la-loi tree's rank spells (localTools/rogue/talentTree.json, tree 3): one per rank, read with Rank(). The
// first ranks are the plan's node table; the later ranks came with the tree's second pass (2026-10-09). Riposte,
// Frapper et fuir, Endurance au combat and Œil de lynx are their ranks' auras' own data. Choices: node 310 Acier
// dansant / Coupes précises, node 328 Lames d'effroi / Rejouer (abilities: nothing to read).
enum OutlawTalents : uint32
{
    TALENT_QUICK_DRAW_1                 = 92263,
    TALENT_QUICK_DRAW_2                 = 92296,
    TALENT_FAN_THE_HAMMER_1             = 92264,
    TALENT_FAN_THE_HAMMER_2             = 92265,
    TALENT_AUDACITY_1                   = 92266,
    TALENT_AUDACITY_2                   = 92298,
    TALENT_HIDDEN_OPPORTUNITY_1         = 92267,
    TALENT_HIDDEN_OPPORTUNITY_2         = 92221,
    TALENT_TRIPLE_THREAT_1              = 92268,
    TALENT_TRIPLE_THREAT_2              = 92269,
    TALENT_PRECISE_CUTS                 = 92270,
    TALENT_DANCING_STEEL                = 92271,
    TALENT_DEFT_MANEUVERS_1             = 92272,
    TALENT_DEFT_MANEUVERS_2             = 92228,
    TALENT_IMPROVED_ADRENALINE_RUSH_1   = 92273,
    TALENT_IMPROVED_ADRENALINE_RUSH_2   = 92243,
    TALENT_LOADED_DICE                  = 92274,
    TALENT_SLEIGHT_OF_HAND_1            = 92275,
    TALENT_SLEIGHT_OF_HAND_2            = 92249,
    TALENT_COUNT_THE_ODDS_1             = 92276,
    TALENT_COUNT_THE_ODDS_2             = 92277,
    TALENT_COUNT_THE_ODDS_3             = 92246,
    TALENT_GREENSKINS_WICKERS_1         = 92278,
    TALENT_GREENSKINS_WICKERS_2         = 92223,
    TALENT_IMPROVED_BETWEEN_EYES_1      = 92279,
    TALENT_IMPROVED_BETWEEN_EYES_2      = 92229,
    TALENT_ACE_UP_YOUR_SLEEVE_1         = 92280,
    TALENT_ACE_UP_YOUR_SLEEVE_2         = 92245,
    TALENT_HEAVY_HITTER_1               = 92281,
    TALENT_HEAVY_HITTER_2               = 92282,
    TALENT_HEAVY_HITTER_3               = 92248,
    TALENT_MAIN_GAUCHE_1                = 92283,
    TALENT_MAIN_GAUCHE_2                = 92284,
    TALENT_MAIN_GAUCHE_3                = 92244,
    TALENT_SWIFT_SLASHER_1              = 92285,
    TALENT_SWIFT_SLASHER_2              = 92306,
    TALENT_IMPROVED_RUTHLESSNESS_1      = 92289,
    TALENT_IMPROVED_RUTHLESSNESS_2      = 92290,
    TALENT_IMPROVED_RUTHLESSNESS_3      = 92247,
    TALENT_BOARDING_SABRE_1             = 92291,
    TALENT_BOARDING_SABRE_2             = 92292,
    TALENT_BOARDING_SABRE_3             = 92295,
    TALENT_IMPROVED_BLADE_FLURRY_1      = 92225,
    TALENT_IMPROVED_BLADE_FLURRY_2      = 92226,
    TALENT_IMPROVED_BLADE_FLURRY_3      = 92227,
    TALENT_FATAL_FLOURISH_1             = 92307,
    TALENT_FATAL_FLOURISH_2             = 92308,
};

// The kits localTools/rogue/ascensionVisuals.json gives a fixed id of their own (from 77903), played here on a unit.
// First ids of the plan: the looks agent's final ids are reconciled by the integrator.
enum OutlawKits : uint32
{
    KIT_BLADE_FLURRY_HIT        = 77903,    // on each enemy a repeat lands on
    KIT_KILLING_SPREE_STRIKE    = 77904,    // on the enemy, at each strike (the kits have no animation)
    KIT_MAIN_GAUCHE             = 77905,    // on the enemy, the off-hand strike
};

// The six Roll the Bones buffs
constexpr std::array<uint32, 6> RollTheBonesBuffs = { {
    SPELL_BROADSIDE, SPELL_BURIED_TREASURE, SPELL_GRAND_MELEE, SPELL_RUTHLESS_PRECISION, SPELL_SKULL_AND_CROSSBONES,
    SPELL_TRUE_BEARING } };

// The cooldowns Lames sans repos shortens besides the utility ones (RogueShared.h)
constexpr std::array<uint32, 9> RestlessBladesSpells = { {
    SPELL_ADRENALINE_RUSH, SPELL_BETWEEN_THE_EYES, SPELL_BLADE_FLURRY, SPELL_ROLL_THE_BONES, SPELL_GHOSTLY_STRIKE,
    SPELL_BLADE_RUSH, SPELL_KILLING_SPREE, SPELL_KEEP_IT_ROLLING, SPELL_DREADBLADES } };

// Taken back from every rogue at login: the Crimson Duelist's kit and auras (the Combat rework before Hors-la-loi,
// taught by mod-stat-growth until 2026-10-09), and the WotLK Combat talents its tree was built on (no tree teaches
// them any more; the patcher no longer turns them into dummies)
constexpr std::array<uint32, 94> RetiredSpells = { {
    90010, 90011, 90012, 90013, 90014, 90015, 90016, 90017, 90018, 90100, 90101, 90102, 90103, 90104, 90105,
    13741, 13793, 13792, 13732, 13863, 13715, 13848, 13849, 13851, 13852, 14165, 14166, 13713, 13853, 13854, 13705,
    13832, 13843, 13844, 13845, 13742, 13872, 14251, 13706, 13804, 13805, 13806, 13807, 13754, 13867, 13743, 13875,
    13712, 13788, 13789, 18427, 18428, 18429, 61330, 61331, 13709, 13800, 13801, 13802, 13803, 13877, 13960, 13961,
    13962, 13963, 13964, 30919, 30920, 31124, 31126, 31122, 31123, 61329, 13750, 31130, 31131, 5952, 51679, 35541,
    35550, 35551, 35552, 35553, 51672, 51674, 32601, 51682, 58413, 51685, 51686, 51687, 51688, 51689, 51690 } };

// Frappe sinistre strikes again (one more combo point) and grants Opportunité; Tête de mort, more often; Triple menace
// a third time now and then. Sabre d'abordage: Sinister Strike's damage.
LiveTuning::KnobInt const OpportunityPct("rogue.outlaw_opportunity_pct", 35);
LiveTuning::KnobInt const SkullAndCrossbonesPct("rogue.outlaw_skull_and_crossbones_pct", 25);
LiveTuning::KnobInt const TripleThreatPctPerRank("rogue.outlaw_triple_threat_pct_per_rank", 10);
LiveTuning::KnobUInt const ExtraStrikeGapMs("rogue.outlaw_extra_strike_gap_ms", 200);
LiveTuning::KnobInt const BoardingSabrePctPerRank("rogue.outlaw_boarding_sabre_pct_per_rank", 10);
// Opportunité cachée: Ambush strikes again with this share of Sinister Strike's chance per rank
LiveTuning::KnobInt const HiddenOpportunityPctPerRank("rogue.outlaw_hidden_opportunity_pct_per_rank", 50);
// Opportunité's charges, more with Marteau en éventail
LiveTuning::KnobUInt const OpportunityMaxStacks("rogue.outlaw_opportunity_max_stacks", 1);
LiveTuning::KnobUInt const FanTheHammerMaxStacks("rogue.outlaw_fan_the_hammer_max_stacks", 2);
// Tir de pistolet: a share of a main-hand hit (normalized, attack power included); Opportunité makes it free and
// stronger, Dégainer vite stronger still (per rank), Mèches de Peau-Verte much stronger; Marteau en éventail's shots
// this far apart. Audace: an Opportunité shot's chance per rank to make Ambush usable out of stealth (retail's
// Audacity)
LiveTuning::KnobInt const PistolShotWeaponPct("rogue.outlaw_pistol_shot_weapon_pct", 130);
LiveTuning::KnobInt const OpportunityDamagePct("rogue.outlaw_opportunity_damage_pct", 50);
LiveTuning::KnobInt const QuickDrawDamagePctPerRank("rogue.outlaw_quick_draw_damage_pct_per_rank", 25);
LiveTuning::KnobInt const GreenskinsDamagePct("rogue.outlaw_greenskins_damage_pct", 300);
LiveTuning::KnobUInt const FanTheHammerGapMs("rogue.outlaw_fan_the_hammer_gap_ms", 150);
LiveTuning::KnobInt const AudacityPctPerRank("rogue.outlaw_audacity_pct_per_rank", 25);
// The finishers: WotLK's Eviscerate (the rogue's rank: its base and per-point damage, and this share of the attack
// power a point) times their factor
LiveTuning::Knob const FinisherApPerPoint("rogue.outlaw_finisher_ap_per_point", 0.07f);
LiveTuning::Knob const DispatchFactor("rogue.outlaw_dispatch_factor", 2.0f);
LiveTuning::KnobInt const HeavyHitterPctPerRank("rogue.outlaw_heavy_hitter_pct_per_rank", 7);
LiveTuning::Knob const BetweenTheEyesFactor("rogue.outlaw_between_eyes_factor", 1.6f);
LiveTuning::KnobUInt const BetweenTheEyesStunMsPerPoint("rogue.outlaw_between_eyes_stun_ms_per_point", 1000);
LiveTuning::KnobUInt const BetweenTheEyesCritMsPerPoint("rogue.outlaw_between_eyes_crit_ms_per_point", 3000);
// Per rank: Entre les deux yeux amélioré's critical strikes; Mèches de Peau-Verte's and Atout dans la manche's chance
// per combo point
LiveTuning::KnobInt const ImprovedBetweenTheEyesCritPctPerRank("rogue.outlaw_improved_between_eyes_crit_pct_per_rank",
    15);
LiveTuning::KnobInt const GreenskinsPctPerPointPerRank("rogue.outlaw_greenskins_pct_per_point_per_rank", 10);
LiveTuning::KnobInt const AceUpYourSleevePctPerPointPerRank("rogue.outlaw_ace_up_your_sleeve_pct_per_point_per_rank",
    2);
// Hors-la-loi's own: Lames sans repos (Cap assuré: more), Potentiel de combat (an off-hand hit's energy; Fioriture
// fatale: more often, per rank), Cruauté (a combo point back from a finisher; Cruauté accrue: per rank, the chance of a
// second one when it does)
LiveTuning::KnobUInt const RestlessBladesMsPerPoint("rogue.outlaw_restless_blades_ms_per_point", 1000);
LiveTuning::KnobUInt const TrueBearingMsPerPoint("rogue.outlaw_true_bearing_ms_per_point", 500);
LiveTuning::KnobInt const CombatPotencyPct("rogue.outlaw_combat_potency_pct", 20);
LiveTuning::KnobUInt const CombatPotencyEnergy("rogue.outlaw_combat_potency_energy", 8);
LiveTuning::KnobInt const FatalFlourishPctPerRank("rogue.outlaw_fatal_flourish_pct_per_rank", 10);
LiveTuning::KnobInt const RuthlessnessPctPerPoint("rogue.outlaw_ruthlessness_pct_per_point", 20);
LiveTuning::KnobInt const ImprovedRuthlessnessPctPerRank("rogue.outlaw_improved_ruthlessness_pct_per_rank", 15);
// Jeter les os: its buffs' time, the chance of a second one (Tour de passe-passe: more, per rank); Compter les chances'
// short buff; Rejouer's added time
LiveTuning::KnobUInt const RollTheBonesMs("rogue.outlaw_roll_the_bones_ms", 30000);
LiveTuning::KnobInt const TwoBuffsPct("rogue.outlaw_two_buffs_pct", 25);
LiveTuning::KnobInt const SleightOfHandPctPerRank("rogue.outlaw_sleight_of_hand_pct_per_rank", 8);
LiveTuning::KnobInt const CountTheOddsPctPerRank("rogue.outlaw_count_the_odds_pct_per_rank", 5);
LiveTuning::KnobUInt const CountTheOddsMs("rogue.outlaw_count_the_odds_ms", 5000);
LiveTuning::KnobUInt const KeepItRollingMs("rogue.outlaw_keep_it_rolling_ms", 30000);
// Déluge de lames: the repeat's share on up to that many other enemies that close to the rogue (Coupes précises: more
// for each one short of the maximum; Acier dansant: one more, and longer; Déluge amélioré: more, per rank); its first
// strike on the enemies around (Manœuvres habiles: a combo point for each enemy struck, up to its first rank's count,
// all five at its second)
LiveTuning::KnobInt const BladeFlurryPct("rogue.outlaw_blade_flurry_pct", 70);
LiveTuning::KnobUInt const BladeFlurryTargets("rogue.outlaw_blade_flurry_targets", 8);
LiveTuning::Knob const BladeFlurryRange("rogue.outlaw_blade_flurry_range", 8.0f);
LiveTuning::KnobInt const BladeFlurryStrikeWeaponPct("rogue.outlaw_blade_flurry_strike_weapon_pct", 50);
LiveTuning::KnobInt const PreciseCutsPct("rogue.outlaw_precise_cuts_pct", 2);
LiveTuning::KnobUInt const DancingSteelMs("rogue.outlaw_dancing_steel_ms", 3000);
LiveTuning::KnobInt const ImprovedBladeFlurryPctPerRank("rogue.outlaw_improved_blade_flurry_pct_per_rank", 10);
LiveTuning::KnobUInt const DeftManeuversFirstRankPoints("rogue.outlaw_deft_maneuvers_first_rank_points", 3);
// How long Blade Flurry lasts when its data has no aura to show it (else the aura's own time)
LiveTuning::KnobUInt const BladeFlurryMs("rogue.outlaw_blade_flurry_ms", 10000);
// Main gauche: a main-hand hit's chance per rank to strike again with the off hand, for this share of a main-hand hit.
// Entailleur rapide: Slice and Dice's attack speed this much more per rank
LiveTuning::KnobInt const MainGauchePctPerRank("rogue.outlaw_main_gauche_pct_per_rank", 10);
LiveTuning::KnobInt const MainGaucheWeaponPct("rogue.outlaw_main_gauche_weapon_pct", 50);
LiveTuning::KnobInt const SwiftSlasherPctPerRank("rogue.outlaw_swift_slasher_pct_per_rank", 10);
// Ruée des lames' strike; Série meurtrière's strikes (main and off hand each), their gap, and whether the rogue jumps
// around its target at each one (0: it strikes from where it stands)
LiveTuning::KnobInt const BladeRushWeaponPct("rogue.outlaw_blade_rush_weapon_pct", 200);
LiveTuning::KnobUInt const KillingSpreeStrikes("rogue.outlaw_killing_spree_strikes", 7);
LiveTuning::KnobUInt const KillingSpreeGapMs("rogue.outlaw_killing_spree_gap_ms", 285);
LiveTuning::KnobInt const KillingSpreeWeaponPct("rogue.outlaw_killing_spree_weapon_pct", 100);
LiveTuning::KnobUInt const KillingSpreeTeleport("rogue.outlaw_killing_spree_teleport", 1);

// The players this close who see the rogue hear its sounds
constexpr float SoundRange = 40.0f;
constexpr uint32 UpdateIntervalMs = 500;

// A rogue's Outlaw state. Kept on the player, so it dies with the session.
struct OutlawState : public DataMap::Base
{
    // A finisher's combo points, read before the spell runs (an instant one has spent them by its cast hook)
    uint32 finisherSpell = 0;
    uint8 finisherPoints = 0;
    // Between the Eyes' points, for its stun landing later than the cast (a missile)
    uint8 betweenEyesPoints = 0;
    // The Pistol Shot being cast: its multiplier, and what it found when it began
    float pistolMultiplier = 1.0f;
    bool pistolOpportunity = false;
    bool pistolGreenskins = false;
    uint32 opportunityCount = 0;    // Opportunité's stacks and charges before the shot (spent already or not)
    // Marteau en éventail's extra shots: the Opportunité shot's multiplier
    float fanMultiplier = 1.0f;
    // Blade Flurry's end, when its data has no aura
    uint32 flurryUntil = 0;
    // Dés pipés: the next Roll the Bones grants one more buff
    bool loadedDice = false;
    // Set while the kit deals its own damage (ModifySpellDamageTaken leaves it alone)
    bool dealing = false;
    // The last auto attack rolled (the hook that sees its damage does not say which hand)
    WeaponAttackType swingAttack = BASE_ATTACK;
    bool swingPending = false;
    uint32 updateTimer = 0;
    // The Slice and Dice haste amount Entailleur rapide set (0: none), and the bonus in it
    int32 swiftSlasherAmount = 0;
    int32 swiftSlasherBonus = 0;
};

constexpr char const* StateKey = "RogueOutlawState";

OutlawState* GetState(Player* player)
{
    return player->CustomData.GetDefault<OutlawState>(StateKey);
}

bool IsOutlaw(Player const* player)
{
    return player && player->getClass() == CLASS_ROGUE && player->HasAura(SPELL_OUTLAW_PASSIVE);
}

Player* OutlawPlayer(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return IsOutlaw(player) ? player : nullptr;
}

uint32 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

// A node's rank: its highest rank spell the rogue has, rank 1 first
uint8 Rank(Player* player, std::initializer_list<uint32> ranks)
{
    uint8 rank = 0;
    uint8 index = 0;
    for (uint32 spellId : ranks)
    {
        ++index;
        if (player->HasAura(spellId))
            rank = index;
    }
    return rank;
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo->GetFirstRankSpell()->Id;
}

bool SpellExists(uint32 spellId)
{
    return sSpellMgr->GetSpellInfo(spellId) != nullptr;
}

bool IsBoss(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss());
}

// Whether a spell's data deals damage itself (else it is a dummy whose damage is dealt here)
bool HasOwnDamage(SpellInfo const* spellInfo)
{
    return spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) || spellInfo->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
        spellInfo->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL) ||
        spellInfo->HasEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) ||
        spellInfo->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE);
}

// Whether an aura's data makes the spells it touches free (a -100% cost modifier) - else the kit refunds them
bool MakesFree(uint32 auraId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(auraId);
    if (!spellInfo)
        return false;
    for (SpellEffectInfo const& effect : spellInfo->Effects)
        if (effect.ApplyAuraName == SPELL_AURA_ADD_PCT_MODIFIER && effect.MiscValue == SPELLMOD_COST)
            return true;
    return false;
}

// Whether the spell reached its target (a dodged finisher spends nothing and deals nothing); one without a unit target
// always does
bool Landed(Spell* spell, Unit* target)
{
    if (!target)
        return true;
    for (TargetInfo const& info : *spell->GetUniqueTargetInfo())
        if (info.targetGUID == target->GetGUID())
            return info.missCondition == SPELL_MISS_NONE;
    return true;
}

void Energize(Player* player, uint32 spellId, uint32 amount)
{
    if (amount)
        player->EnergizeBySpell(player, spellId, amount, POWER_ENERGY);
}

// Combo points the kit adds after a spell. A finisher clears the rogue's points when it lands: one still flying (a
// missile) would clear these too, so they wait for it.
void AddPointsAfter(Player* player, Spell* spell, Unit* target, uint8 count)
{
    if (!count)
        return;
    SpellInfo const* spellInfo = spell ? spell->GetSpellInfo() : nullptr;
    if (!spell || !target || spell->getState() != SPELL_STATE_DELAYED || spellInfo->Speed <= 0.0f)
    {
        player->AddComboPoints(target, int8(count));
        return;
    }
    uint32 const flightMs = uint32(player->GetDistance(target) / spellInfo->Speed * IN_MILLISECONDS) + 150;
    player->m_Events.AddEventAtOffset([player, count]()
    {
        if (player->IsInWorld() && player->IsAlive())
            player->AddComboPoints(int8(count));
    }, Milliseconds(flightMs));
}

// Grants an aura on the rogue for that long (a fresh one, or the one up given that time)
Aura* GrantFor(Player* player, uint32 spellId, uint32 durationMs)
{
    Aura* aura = player->GetAura(spellId);
    if (!aura)
        aura = player->AddAura(spellId, player);
    if (aura && durationMs)
    {
        aura->SetMaxDuration(int32(durationMs));
        aura->SetDuration(int32(durationMs));
    }
    return aura;
}

// One more use of an aura, up to maxUses: a charge when its data gives it charges (Opportunité: a stack would multiply
// its cost modifier), else a stack
void AddUse(Player* player, uint32 spellId, uint32 maxUses)
{
    Aura* aura = player->GetAura(spellId);
    if (!aura)
    {
        player->AddAura(spellId, player);
        return;
    }
    if (aura->GetCharges())
    {
        if (aura->GetCharges() < maxUses)
            aura->SetCharges(aura->GetCharges() + 1);
    }
    else if (aura->GetStackAmount() < maxUses)
        aura->SetStackAmount(aura->GetStackAmount() + 1);
    aura->RefreshDuration();
}

// An aura's count of uses left: its stacks, or its charges
uint32 UsesOf(Player* player, uint32 spellId)
{
    Aura const* aura = player->GetAura(spellId);
    return aura ? uint32(aura->GetStackAmount()) + aura->GetCharges() : 0;
}

// Spends one use of an aura (unless the cast spent it already through its data: charges)
void SpendOne(Player* player, uint32 spellId, uint32 usesBefore)
{
    Aura* aura = player->GetAura(spellId);
    if (!aura || UsesOf(player, spellId) != usesBefore)
        return;
    if (aura->GetCharges())
        aura->DropCharge();
    else if (aura->GetStackAmount() > 1)
        aura->SetStackAmount(aura->GetStackAmount() - 1);
    else
        player->RemoveAura(aura);
}

std::list<Unit*> EnemiesAround(Player* player, float range)
{
    std::list<Unit*> found;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(player, found, check);
    Cell::VisitObjects(player, searcher, range);
    std::list<Unit*> enemies;
    for (Unit* unit : found)
        if (unit->IsAlive() && player->IsValidAttackTarget(unit) && !unit->IsTotem())
            enemies.push_back(unit);
    return enemies;
}

// The sound of a cast, for every player close by who sees the rogue (bots have no client: the engine skips them)
void PlayCastSound(Player* rogue, std::string_view key)
{
    std::list<Player*> players;
    Acore::AnyPlayerInObjectRangeCheck check(rogue, SoundRange, false);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(rogue, players, check);
    Cell::VisitObjects(rogue, searcher, SoundRange);
    for (Player* listener : players)
        if (listener == rogue || listener->HaveAtClient(rogue))
            EvolutionsAudio::PlayOn(listener, key, rogue);
}

std::string_view SoundOf(SpellInfo const* spellInfo)
{
    switch (spellInfo->Id)
    {
        case SPELL_PISTOL_SHOT:         return "Outlaw.PistolShot";
        case SPELL_DISPATCH:            return "Outlaw.Dispatch";
        case SPELL_BETWEEN_THE_EYES:    return "Outlaw.BetweenTheEyes";
        case SPELL_ROLL_THE_BONES:      return "Outlaw.RollTheBones";
        case SPELL_BLADE_FLURRY:        return "Outlaw.BladeFlurry";
        case SPELL_ADRENALINE_RUSH:     return "Outlaw.AdrenalineRush";
        case SPELL_KILLING_SPREE:       return "Outlaw.KillingSpree";
        case SPELL_BLADE_RUSH:          return "Outlaw.BladeRush";
        default:
            break;
    }
    switch (FirstRank(spellInfo))
    {
        case SPELL_SINISTER_STRIKE:     return "Outlaw.SinisterStrike";
        case SPELL_GHOSTLY_STRIKE:      return "Outlaw.GhostlyStrike";
        default:
            return {};
    }
}

// --- Damage -------------------------------------------------------------------------------------------------------

// A main-hand (or off-hand) hit: the weapon's damage with the attack power, normalized as abilities are
float WeaponHit(Player* player, WeaponAttackType attackType)
{
    if (attackType == OFF_ATTACK && !player->haveOffhandWeapon())
        return 0.0f;
    return float(player->CalculateDamage(attackType, true, true));
}

// The finishers' base: WotLK's Eviscerate of the rogue's best rank (base and per-point damage) and FinisherApPerPoint
// of the attack power a point
float EviscerateDamage(Player* player, uint8 comboPoints)
{
    SpellInfo const* rank = sSpellMgr->GetSpellInfo(SPELL_EVISCERATE);
    for (SpellInfo const* next = rank; next; next = next->GetNextRankSpell())
        if (player->HasSpell(next->Id))
            rank = next;

    float base = 0.0f;
    float perPoint = 0.0f;
    if (rank)
    {
        SpellEffectInfo const& effect = rank->Effects[EFFECT_0];
        base = float(effect.BasePoints) + float(std::max(effect.DieSides, 1) + 1) / 2.0f;
        perPoint = effect.PointsPerComboPoint;
    }
    float const ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
    return base + comboPoints * (perPoint + ap * FinisherApPerPoint);
}

void BladeFlurryRepeat(Player* player, Unit* victim, uint32 worth);

struct StrikeResult
{
    uint32 worth = 0;   // damage dealt and absorbed
    bool crit = false;
};

// Damage the kit works out, dealt as that ability: the rogue's damage bonuses and the target's, a critical strike on
// the rogue's melee chance (critBonusPct more when it is one), armour for physical ones. Blade Flurry repeats it.
StrikeResult Strike(Player* player, Unit* target, uint32 spellId, float amount, int32 critBonusPct = 0,
    bool repeat = true)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || amount < 1.0f || !target || !target->IsAlive())
        return {};

    OutlawState* state = GetState(player);
    state->dealing = true;
    uint32 damage = player->SpellDamageBonusDone(target, spellInfo, uint32(amount), SPELL_DIRECT_DAMAGE, EFFECT_0);
    damage = target->SpellDamageBonusTaken(player, spellInfo, damage, SPELL_DIRECT_DAMAGE);
    float const critChance = player->GetUnitCriticalChance(BASE_ATTACK, target);
    bool const crit = roll_chance_f(critChance);
    SpellNonMeleeDamage log(player, target, spellInfo, spellInfo->GetSchoolMask());
    player->CalculateSpellDamageTaken(&log, int32(damage), spellInfo, BASE_ATTACK, crit, critChance);
    state->dealing = false;

    if (crit && critBonusPct > 0)
        log.damage = uint32(log.damage * (1.0f + critBonusPct / 100.0f));
    Unit::DealDamageMods(target, log.damage, &log.absorb);
    StrikeResult const result = { log.damage + log.absorb, crit };
    player->SendSpellNonMeleeDamageLog(&log);
    player->DealSpellDamage(&log, true);

    if (repeat)
        BladeFlurryRepeat(player, target, result.worth);
    return result;
}

// --- Déluge de lames ----------------------------------------------------------------------------------------------

bool IsFlurrying(Player* player)
{
    if (player->HasAura(SPELL_BLADE_FLURRY))
        return true;
    return NowMs() < GetState(player)->flurryUntil;
}

uint32 BladeFlurryMaxTargets(Player* player)
{
    return BladeFlurryTargets + (player->HasAura(TALENT_DANCING_STEEL) ? 1 : 0);
}

// A single-target hit of the rogue, again on up to BladeFlurryMaxTargets other enemies close by, for BladeFlurryPct of
// it (Coupes précises: more for each enemy short of the maximum). Dealt a tick later as plain damage: nothing repeats
// it again, nothing procs from it.
void BladeFlurryRepeat(Player* player, Unit* victim, uint32 worth)
{
    if (!worth || !victim || !IsFlurrying(player))
        return;

    ObjectGuid const victimGuid = victim->GetGUID();
    player->m_Events.AddEventAtOffset([player, victimGuid, worth]()
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        uint32 const spellId = SpellExists(SPELL_BLADE_FLURRY_HIT) ? SPELL_BLADE_FLURRY_HIT : SPELL_BLADE_FLURRY;
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        uint32 const maxTargets = BladeFlurryMaxTargets(player);
        std::vector<Unit*> others;
        for (Unit* enemy : EnemiesAround(player, BladeFlurryRange))
            if (enemy->GetGUID() != victimGuid && others.size() < maxTargets)
                others.push_back(enemy);
        if (others.empty())
            return;

        float share = BladeFlurryPct / 100.0f;
        if (player->HasAura(TALENT_PRECISE_CUTS))
            share *= 1.0f + PreciseCutsPct * float(maxTargets - others.size()) / 100.0f;
        if (uint8 const improved = Rank(player, { TALENT_IMPROVED_BLADE_FLURRY_1, TALENT_IMPROVED_BLADE_FLURRY_2,
                TALENT_IMPROVED_BLADE_FLURRY_3 }))
            share *= 1.0f + ImprovedBladeFlurryPctPerRank * improved / 100.0f;
        uint32 const amount = std::max<uint32>(1, uint32(worth * share));
        for (Unit* enemy : others)
        {
            enemy->SendPlaySpellVisual(KIT_BLADE_FLURRY_HIT);
            SpellNonMeleeDamage log(player, enemy, spellInfo, spellInfo->GetSchoolMask());
            log.damage = amount;
            Unit::DealDamageMods(enemy, log.damage, &log.absorb);
            player->SendSpellNonMeleeDamageLog(&log);
            player->DealSpellDamage(&log, false);
        }
    }, 0ms);
}

// Its first strike: every enemy close by (its target first), up to the repeat's targets and the target; Acier dansant
// makes it last longer, Manœuvres habiles gives a combo point for each enemy struck (up to its first rank's count, or
// all five at its second)
void BladeFlurryStart(Player* player, Unit* target)
{
    OutlawState* state = GetState(player);
    uint32 durationMs = BladeFlurryMs;
    if (Aura* aura = player->GetAura(SPELL_BLADE_FLURRY))
        durationMs = uint32(std::max(aura->GetMaxDuration(), 0));
    if (player->HasAura(TALENT_DANCING_STEEL))
    {
        durationMs += DancingSteelMs;
        if (Aura* aura = player->GetAura(SPELL_BLADE_FLURRY))
        {
            aura->SetMaxDuration(int32(durationMs));
            aura->SetDuration(int32(durationMs));
        }
    }
    state->flurryUntil = SpellExists(SPELL_BLADE_FLURRY) &&
        sSpellMgr->GetSpellInfo(SPELL_BLADE_FLURRY)->HasEffect(SPELL_EFFECT_APPLY_AURA) ? 0 : NowMs() + durationMs;

    std::list<Unit*> enemies = EnemiesAround(player, BladeFlurryRange);
    if (target && player->IsValidAttackTarget(target))
    {
        enemies.remove(target);
        enemies.push_front(target);
    }
    uint32 const maxStruck = BladeFlurryMaxTargets(player) + 1;
    uint32 struck = 0;
    float const hit = WeaponHit(player, BASE_ATTACK) * BladeFlurryStrikeWeaponPct / 100.0f;
    for (Unit* enemy : enemies)
    {
        if (struck >= maxStruck)
            break;
        Strike(player, enemy, SPELL_BLADE_FLURRY, hit, 0, false);
        ++struck;
    }
    uint8 const deft = Rank(player, { TALENT_DEFT_MANEUVERS_1, TALENT_DEFT_MANEUVERS_2 });
    if (struck && deft)
    {
        uint32 const most = deft >= 2 ? 5 : std::min<uint32>(DeftManeuversFirstRankPoints, 5);
        player->AddComboPoints(target, int8(std::min<uint32>(struck, most)));
    }
}

// --- Procs on hits ------------------------------------------------------------------------------------------------

// Potentiel de combat: an off-hand hit gives energy now and then (Fioriture fatale: more often)
void CombatPotency(Player* player)
{
    int32 const chance = CombatPotencyPct + FatalFlourishPctPerRank *
        Rank(player, { TALENT_FATAL_FLOURISH_1, TALENT_FATAL_FLOURISH_2 });
    if (roll_chance_i(chance))
        Energize(player, SPELL_OUTLAW_PASSIVE, CombatPotencyEnergy);
}

// Main gauche: a main-hand hit now and then strikes again with the off hand, a tick later
void MainGauche(Player* player, Unit* victim)
{
    uint8 const rank = Rank(player, { TALENT_MAIN_GAUCHE_1, TALENT_MAIN_GAUCHE_2, TALENT_MAIN_GAUCHE_3 });
    if (!rank || !player->haveOffhandWeapon() || !roll_chance_i(MainGauchePctPerRank * rank))
        return;

    ObjectGuid const victimGuid = victim->GetGUID();
    player->m_Events.AddEventAtOffset([player, victimGuid]()
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        Unit* enemy = ObjectAccessor::GetUnit(*player, victimGuid);
        if (!enemy || !enemy->IsAlive() || !player->IsValidAttackTarget(enemy))
            return;
        uint32 const spellId = SpellExists(SPELL_MAIN_GAUCHE_HIT) ? SPELL_MAIN_GAUCHE_HIT : TALENT_MAIN_GAUCHE_1;
        enemy->SendPlaySpellVisual(KIT_MAIN_GAUCHE);
        Strike(player, enemy, spellId, WeaponHit(player, BASE_ATTACK) * MainGaucheWeaponPct / 100.0f);
        CombatPotency(player);
    }, 0ms);
}

// The rogue's hits Blade Flurry repeats and Main Gauche follows: one enemy's, a weapon's (melee or ranged)
bool IsSingleTargetHit(SpellInfo const* spellInfo)
{
    return (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED) &&
        !spellInfo->IsAffectingArea() && spellInfo->Id != SPELL_BLADE_FLURRY;
}

// --- Jeter les os -------------------------------------------------------------------------------------------------

// Grants that buff for that long, or lengthens the one up by it (Compter les chances)
void AddRollTheBonesTime(Player* player, uint32 spellId, uint32 durationMs)
{
    Aura* aura = player->GetAura(spellId);
    if (!aura)
    {
        GrantFor(player, spellId, durationMs);
        return;
    }
    int32 const duration = aura->GetDuration() + int32(durationMs);
    aura->SetMaxDuration(std::max(aura->GetMaxDuration(), duration));
    aura->SetDuration(duration);
}

void RollTheBones(Player* player)
{
    for (uint32 spellId : RollTheBonesBuffs)
        player->RemoveAurasDueToSpell(spellId);

    OutlawState* state = GetState(player);
    uint32 count = 1;
    int32 chance = TwoBuffsPct;
    chance += SleightOfHandPctPerRank * Rank(player, { TALENT_SLEIGHT_OF_HAND_1, TALENT_SLEIGHT_OF_HAND_2 });
    if (roll_chance_i(chance))
        ++count;
    bool const loaded = SpellExists(SPELL_LOADED_DICE) ? player->HasAura(SPELL_LOADED_DICE) : state->loadedDice;
    if (loaded)
    {
        ++count;
        state->loadedDice = false;
        player->RemoveAurasDueToSpell(SPELL_LOADED_DICE);
    }

    std::vector<uint32> buffs(RollTheBonesBuffs.begin(), RollTheBonesBuffs.end());
    Acore::Containers::RandomShuffle(buffs);
    for (uint32 index = 0; index < count && index < buffs.size(); ++index)
        GrantFor(player, buffs[index], RollTheBonesMs);
}

// Rejouer: the buffs up last that much longer
void KeepItRolling(Player* player)
{
    for (uint32 spellId : RollTheBonesBuffs)
        if (Aura* aura = player->GetAura(spellId))
        {
            aura->SetMaxDuration(aura->GetMaxDuration() + int32(KeepItRollingMs));
            aura->SetDuration(aura->GetDuration() + int32(KeepItRollingMs));
        }
}

// Compter les chances: a random Roll the Bones buff for a few seconds
void CountTheOdds(Player* player)
{
    uint8 const rank = Rank(player, { TALENT_COUNT_THE_ODDS_1, TALENT_COUNT_THE_ODDS_2, TALENT_COUNT_THE_ODDS_3 });
    if (rank && roll_chance_i(CountTheOddsPctPerRank * rank))
        AddRollTheBonesTime(player, Acore::Containers::SelectRandomContainerElement(RollTheBonesBuffs),
            CountTheOddsMs);
}

// --- The kit ------------------------------------------------------------------------------------------------------

// Frappe sinistre (and Ambush with Opportunité cachée, at its share of the chance): the chance to strike again, a
// moment later (a triggered cast: free, its own combo point, and it does not roll again), with Opportunité; Triple
// menace, a third time
void ExtraStrikes(Player* player, Unit* target, uint32 spellId, int32 chanceSharePct = 100)
{
    int32 chance = OpportunityPct;
    if (player->HasAura(SPELL_SKULL_AND_CROSSBONES))
        chance += SkullAndCrossbonesPct;
    if (!roll_chance_i(chance * chanceSharePct / 100))
        return;

    uint32 const maxStacks = Rank(player, { TALENT_FAN_THE_HAMMER_1, TALENT_FAN_THE_HAMMER_2 }) ?
        FanTheHammerMaxStacks : OpportunityMaxStacks;
    AddUse(player, SPELL_OPPORTUNITY, std::max<uint32>(1, maxStacks));

    uint32 strikes = 1;
    if (uint8 const triple = Rank(player, { TALENT_TRIPLE_THREAT_1, TALENT_TRIPLE_THREAT_2 }))
        if (roll_chance_i(TripleThreatPctPerRank * triple))
            ++strikes;

    ObjectGuid const targetGuid = target->GetGUID();
    for (uint32 strike = 1; strike <= strikes; ++strike)
        player->m_Events.AddEventAtOffset([player, targetGuid, spellId]()
        {
            Unit* again = ObjectAccessor::GetUnit(*player, targetGuid);
            if (player->IsInWorld() && player->IsAlive() && again && again->IsAlive())
                player->CastSpell(again, spellId, TRIGGERED_FULL_MASK);
        }, Milliseconds(ExtraStrikeGapMs * strike));
}

// An Opportunité Pistol Shot's strength (Dégainer vite: more, per rank)
float OpportunityMultiplier(Player* player)
{
    float multiplier = 1.0f + OpportunityDamagePct / 100.0f;
    if (uint8 const quick = Rank(player, { TALENT_QUICK_DRAW_1, TALENT_QUICK_DRAW_2 }))
        multiplier *= 1.0f + QuickDrawDamagePctPerRank * quick / 100.0f;
    return multiplier;
}

// Tir de pistolet. The first shot spends an Opportunité (free, stronger; Dégainer vite: a combo point and stronger
// still; Audace: Ambush out of stealth now and then; Marteau en éventail: more shots) and Mèches de Peau-Verte.
void PistolShot(Player* player, Spell* spell, Unit* target, bool hit)
{
    OutlawState* state = GetState(player);
    SpellInfo const* spellInfo = spell->GetSpellInfo();

    if (!spell->IsTriggered())
    {
        if (state->pistolOpportunity)
        {
            SpendOne(player, SPELL_OPPORTUNITY, state->opportunityCount);
            if (!MakesFree(SPELL_OPPORTUNITY) && spell->GetPowerCost() > 0)
                player->ModifyPower(POWER_ENERGY, spell->GetPowerCost());
            if (Rank(player, { TALENT_QUICK_DRAW_1, TALENT_QUICK_DRAW_2 }) && target)
                player->AddComboPoints(target, 1);
            uint8 const audacity = Rank(player, { TALENT_AUDACITY_1, TALENT_AUDACITY_2 });
            if (audacity && roll_chance_i(AudacityPctPerRank * audacity))
                player->AddAura(SPELL_AUDACITY, player);

            // The extra shots carry the Opportunité shot's strength, not Mèches de Peau-Verte's
            state->fanMultiplier = OpportunityMultiplier(player);
            uint8 const fan = Rank(player, { TALENT_FAN_THE_HAMMER_1, TALENT_FAN_THE_HAMMER_2 });
            if (fan && target)
            {
                ObjectGuid const targetGuid = target->GetGUID();
                for (uint8 shot = 1; shot <= fan; ++shot)
                    player->m_Events.AddEventAtOffset([player, targetGuid]()
                    {
                        Unit* again = ObjectAccessor::GetUnit(*player, targetGuid);
                        if (player->IsInWorld() && player->IsAlive() && again && again->IsAlive())
                            player->CastSpell(again, SPELL_PISTOL_SHOT, TRIGGERED_FULL_MASK);
                    }, Milliseconds(FanTheHammerGapMs * shot));
            }
        }
        if (state->pistolGreenskins)
            player->RemoveAurasDueToSpell(SPELL_GREENSKINS_WICKERS);
    }

    if (!target || !hit)
        return;
    if (!HasOwnDamage(spellInfo))
        Strike(player, target, SPELL_PISTOL_SHOT,
            WeaponHit(player, BASE_ATTACK) * PistolShotWeaponPct / 100.0f * state->pistolMultiplier);
    if (!spellInfo->HasEffect(SPELL_EFFECT_ADD_COMBO_POINTS))
        player->AddComboPoints(target, 1);
    if (!spellInfo->HasAura(SPELL_AURA_MOD_DECREASE_SPEED) && SpellExists(SPELL_PISTOL_SHOT_SLOW) && target->IsAlive())
        player->AddAura(SPELL_PISTOL_SHOT_SLOW, target);
}

// Achever: Eviscerate's damage times DispatchFactor (Frappeur lourd: more)
float DispatchDamage(Player* player, uint8 points)
{
    float damage = EviscerateDamage(player, points) * DispatchFactor;
    if (uint8 const heavy = Rank(player, { TALENT_HEAVY_HITTER_1, TALENT_HEAVY_HITTER_2, TALENT_HEAVY_HITTER_3 }))
        damage *= 1.0f + HeavyHitterPctPerRank * heavy / 100.0f;
    return damage;
}

float BetweenTheEyesDamage(Player* player, uint8 points)
{
    return EviscerateDamage(player, points) * BetweenTheEyesFactor;
}

// Entre les deux yeux: its damage (Entre les deux yeux amélioré: its critical strikes harder), the stun (not on
// bosses), the critical strike buff, Mèches de Peau-Verte and Atout dans la manche
void BetweenTheEyes(Player* player, Spell* spell, Unit* target, uint8 points)
{
    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (!HasOwnDamage(spellInfo))
        Strike(player, target, SPELL_BETWEEN_THE_EYES, BetweenTheEyesDamage(player, points),
            ImprovedBetweenTheEyesCritPctPerRank *
            Rank(player, { TALENT_IMPROVED_BETWEEN_EYES_1, TALENT_IMPROVED_BETWEEN_EYES_2 }));

    // Its own stun, when its data has none (with one, OnAuraApply sets its length)
    if (!spellInfo->HasAura(SPELL_AURA_MOD_STUN) && !IsBoss(target) && target->IsAlive())
    {
        uint32 const stunId = SpellExists(SPELL_BETWEEN_THE_EYES_STUN) ? SPELL_BETWEEN_THE_EYES_STUN : SPELL_CHEAP_SHOT;
        if (Aura* stun = player->AddAura(stunId, target))
        {
            int32 const duration = int32(BetweenTheEyesStunMsPerPoint * points);
            stun->SetMaxDuration(duration);
            stun->SetDuration(duration);
        }
    }

    if (SpellExists(SPELL_BETWEEN_THE_EYES_CRIT))
        GrantFor(player, SPELL_BETWEEN_THE_EYES_CRIT, BetweenTheEyesCritMsPerPoint * points);
    uint8 const greenskins = Rank(player, { TALENT_GREENSKINS_WICKERS_1, TALENT_GREENSKINS_WICKERS_2 });
    if (greenskins && roll_chance_i(GreenskinsPctPerPointPerRank * greenskins * points))
        player->AddAura(SPELL_GREENSKINS_WICKERS, player);
    uint8 const ace = Rank(player, { TALENT_ACE_UP_YOUR_SLEEVE_1, TALENT_ACE_UP_YOUR_SLEEVE_2 });
    if (ace && roll_chance_i(AceUpYourSleevePctPerPointPerRank * ace * points))
        AddPointsAfter(player, spell, target, 5);
}

// Ruée des lames: to the target (when its data has no charge), then the strike as it arrives
void BladeRush(Player* player, Spell* spell, Unit* target)
{
    // Its energy over time, unless its own data gave it already
    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (SpellExists(SPELL_BLADE_RUSH_ENERGY) && !player->HasAura(SPELL_BLADE_RUSH_ENERGY) &&
        !spellInfo->HasAura(SPELL_AURA_PERIODIC_ENERGIZE))
        player->AddAura(SPELL_BLADE_RUSH_ENERGY, player);

    if (!target)
        return;
    float const distance = player->GetExactDist2d(target) - player->GetMeleeRange(target);
    uint32 arrivalMs = 0;
    bool const charges = spellInfo->HasEffect(SPELL_EFFECT_CHARGE) || spellInfo->HasEffect(SPELL_EFFECT_CHARGE_DEST);
    if (distance > 0.0f && !charges && !player->HasUnitState(UNIT_STATE_ROOT) && !player->GetVehicle())
    {
        float x, y, z;
        target->GetNearPoint(player, x, y, z, player->GetCombatReach(), 0.0f, target->GetAngle(player));
        player->GetMotionMaster()->MoveCharge(x, y, z, SPEED_CHARGE);
    }
    if (distance > 0.0f)
        arrivalMs = uint32(distance / SPEED_CHARGE * IN_MILLISECONDS);

    if (HasOwnDamage(spellInfo))
        return;
    ObjectGuid const targetGuid = target->GetGUID();
    player->m_Events.AddEventAtOffset([player, targetGuid]()
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        Unit* enemy = ObjectAccessor::GetUnit(*player, targetGuid);
        if (enemy && enemy->IsAlive() && player->IsValidAttackTarget(enemy))
            Strike(player, enemy, SPELL_BLADE_RUSH, WeaponHit(player, BASE_ATTACK) * BladeRushWeaponPct / 100.0f);
    }, Milliseconds(arrivalMs + 100));
}

// Série meurtrière: strikes on the target one after the other, main and off hand, the rogue jumping round it for each
void KillingSpree(Player* player, Unit* target)
{
    ObjectGuid const targetGuid = target->GetGUID();
    uint32 const strikes = std::max<uint32>(1, KillingSpreeStrikes);
    for (uint32 strike = 0; strike < strikes; ++strike)
        player->m_Events.AddEventAtOffset([player, targetGuid]()
        {
            if (!player->IsInWorld() || !player->IsAlive())
                return;
            Unit* enemy = ObjectAccessor::GetUnit(*player, targetGuid);
            if (!enemy || !enemy->IsAlive() || !player->IsValidAttackTarget(enemy))
                return;

            // A point round the target at random, the rogue facing it; never far up or down (stairs, ledges)
            if (KillingSpreeTeleport && !player->HasUnitState(UNIT_STATE_ROOT) && !player->GetVehicle() &&
                !player->IsBeingTeleported() && player->IsWithinDistInMap(enemy, 15.0f))
            {
                float x, y, z;
                enemy->GetNearPoint(enemy, x, y, z, 0.0f, enemy->GetCombatReach() + 0.5f,
                    frand(0.0f, 2.0f * float(M_PI)));
                if (std::fabs(z - enemy->GetPositionZ()) < 3.0f && enemy->IsWithinLOS(x, y, z))
                {
                    float const facing = Position::NormalizeOrientation(
                        std::atan2(enemy->GetPositionY() - y, enemy->GetPositionX() - x));
                    player->NearTeleportTo(x, y, z, facing, true);
                    // As the core's Killing Spree: the position at once, not at the client's answer
                    if (player->IsBeingTeleportedNear())
                        player->UpdatePosition(player->GetTeleportDest(), true);
                }
            }

            enemy->SendPlaySpellVisual(KIT_KILLING_SPREE_STRIKE);
            float const share = KillingSpreeWeaponPct / 100.0f;
            Strike(player, enemy, SPELL_KILLING_SPREE, WeaponHit(player, BASE_ATTACK) * share);
            if (enemy->IsAlive())
                Strike(player, enemy, SPELL_KILLING_SPREE, WeaponHit(player, OFF_ATTACK) * share);
        }, Milliseconds(KillingSpreeGapMs * strike));
}

// Lames sans repos (built in; Cap assuré: more), Cruauté (Cruauté accrue: now and then a second point)
void OnFinisher(Player* player, Spell* spell, Unit* target, uint8 points)
{
    uint32 perPoint = RestlessBladesMsPerPoint;
    if (player->HasAura(SPELL_TRUE_BEARING))
        perPoint += TrueBearingMsPerPoint;
    int32 const reduction = int32(perPoint * points);
    for (uint32 spellId : RestlessBladesSpells)
        player->ModifySpellCooldown(spellId, -reduction);
    ReduceRogueUtilityCooldowns(player, reduction);

    if (!roll_chance_i(std::min<int32>(points * RuthlessnessPctPerPoint, 100)))
        return;
    uint8 const improved = Rank(player, { TALENT_IMPROVED_RUTHLESSNESS_1, TALENT_IMPROVED_RUTHLESSNESS_2,
        TALENT_IMPROVED_RUTHLESSNESS_3 });
    bool const second = improved && roll_chance_i(ImprovedRuthlessnessPctPerRank * improved);
    AddPointsAfter(player, spell, target, second ? 2 : 1);
}

// --- Spell casts --------------------------------------------------------------------------------------------------

class RogueOutlawSpellScript : public AllSpellScript
{
public:
    RogueOutlawSpellScript() : AllSpellScript("RogueOutlawSpellScript", { ALLSPELLHOOK_ON_CAST,
        ALLSPELLHOOK_ON_SCALE_AURA_UNIT_ADD }) { }

    // Between the Eyes' own stun never reaches a boss: its effect is taken off the target before it lands (a stun
    // removed afterwards would still interrupt the boss's cast)
    void OnScaleAuraUnitAdd(Spell* spell, Unit* target, uint32 /*effectMask*/, bool /*checkIfValid*/,
        bool /*implicit*/, uint8 /*auraScaleMask*/, TargetInfo& targetInfo) override
    {
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (spellInfo->Id != SPELL_BETWEEN_THE_EYES || !IsBoss(target) || !OutlawPlayer(spell->GetCaster()))
            return;
        for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
            if (spellInfo->Effects[index].ApplyAuraName == SPELL_AURA_MOD_STUN)
                targetInfo.effectMask = uint8(targetInfo.effectMask & ~(1 << index));
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = OutlawPlayer(caster);
        if (!player || !spellInfo)
            return;

        OutlawState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        bool const triggered = spell->IsTriggered();
        uint32 const firstRank = FirstRank(spellInfo);
        bool const hit = Landed(spell, target);

        uint8 points = 0;
        if (spellInfo->NeedsComboPoints())
        {
            points = player->GetComboPoints();
            if (state->finisherSpell == spellInfo->Id)
            {
                points = std::max(points, state->finisherPoints);
                state->finisherSpell = 0;
            }
        }

        std::string_view const sound = SoundOf(spellInfo);
        if (!sound.empty())
            PlayCastSound(player, sound);

        switch (spellInfo->Id)
        {
            case SPELL_PISTOL_SHOT:
                PistolShot(player, spell, target, hit);
                break;
            case SPELL_DISPATCH:
                if (target && hit && points && !HasOwnDamage(spellInfo) &&
                    Strike(player, target, SPELL_DISPATCH, DispatchDamage(player, points)).worth)
                    MainGauche(player, target);
                break;
            case SPELL_BETWEEN_THE_EYES:
                if (target && hit && points)
                    BetweenTheEyes(player, spell, target, points);
                break;
            case SPELL_ROLL_THE_BONES:
                RollTheBones(player);
                break;
            case SPELL_KEEP_IT_ROLLING:
                KeepItRolling(player);
                break;
            case SPELL_BLADE_FLURRY:
                BladeFlurryStart(player, target);
                break;
            case SPELL_ADRENALINE_RUSH:
            {
                // Poussée améliorée: the combo points, and the Energy too at its second rank
                uint8 const improved = Rank(player, { TALENT_IMPROVED_ADRENALINE_RUSH_1,
                    TALENT_IMPROVED_ADRENALINE_RUSH_2 });
                if (improved)
                    player->AddComboPoints(target, 5);
                if (improved >= 2)
                    player->SetPower(POWER_ENERGY, player->GetMaxPower(POWER_ENERGY));
                if (player->HasAura(TALENT_LOADED_DICE))
                {
                    state->loadedDice = true;
                    if (SpellExists(SPELL_LOADED_DICE))
                        player->AddAura(SPELL_LOADED_DICE, player);
                }
                break;
            }
            case SPELL_BLADE_RUSH:
                BladeRush(player, spell, target);
                break;
            case SPELL_KILLING_SPREE:
                if (target)
                    KillingSpree(player, target);
                break;
            default:
                break;
        }

        if (!triggered)
        {
            bool const sinister = firstRank == SPELL_SINISTER_STRIKE;
            bool const ambush = firstRank == SPELL_AMBUSH;

            // Audace is spent by the Ambush it allowed
            if (ambush)
                player->RemoveAurasDueToSpell(SPELL_AUDACITY);

            uint8 const hidden = ambush ?
                Rank(player, { TALENT_HIDDEN_OPPORTUNITY_1, TALENT_HIDDEN_OPPORTUNITY_2 }) : 0;
            if (target && hit && sinister)
                ExtraStrikes(player, target, spellInfo->Id);
            else if (target && hit && hidden)
                ExtraStrikes(player, target, spellInfo->Id, HiddenOpportunityPctPerRank * hidden);

            // Bordée: a builder's combo point more; Lames d'effroi: all of them
            if (target && hit && (sinister || ambush || spellInfo->Id == SPELL_PISTOL_SHOT))
            {
                if (player->HasAura(SPELL_DREADBLADES))
                    player->AddComboPoints(target, 5);
                else if (player->HasAura(SPELL_BROADSIDE))
                    player->AddComboPoints(target, 1);
            }

            if (hit && (sinister || ambush || spellInfo->Id == SPELL_DISPATCH))
                CountTheOdds(player);
        }

        // Every finisher that landed, with the combo points it spent
        if (spellInfo->NeedsComboPoints() && points && hit)
            OnFinisher(player, spell, target, points);
    }
};

// --- Damage and auras ---------------------------------------------------------------------------------------------

class RogueOutlawUnitScript : public UnitScript
{
public:
    RogueOutlawUnitScript() : UnitScript("RogueOutlawUnitScript", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_FINAL_DAMAGE,
        UNITHOOK_ON_BEFORE_ROLL_MELEE_OUTCOME_AGAINST,
        UNITHOOK_ON_AURA_APPLY
    }) { }

    // Sabre d'abordage on Sinister Strike; and the kit's abilities whose data deals damage itself: their hit is the
    // kit's number (the rogue's and the target's bonuses on it), the critical strike still rolled by the core
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!player || player->getClass() != CLASS_ROGUE || !spellInfo || !target || damage <= 0)
            return;

        bool const sinister = FirstRank(spellInfo) == SPELL_SINISTER_STRIKE;
        bool const kit = spellInfo->Id == SPELL_PISTOL_SHOT || spellInfo->Id == SPELL_DISPATCH ||
            spellInfo->Id == SPELL_BETWEEN_THE_EYES || spellInfo->Id == SPELL_BLADE_RUSH;
        if ((!sinister && !kit) || !IsOutlaw(player))
            return;

        if (sinister)
        {
            if (uint8 const sabre = Rank(player, { TALENT_BOARDING_SABRE_1, TALENT_BOARDING_SABRE_2,
                    TALENT_BOARDING_SABRE_3 }))
                damage = int32(damage * (1.0f + BoardingSabrePctPerRank * sabre / 100.0f));
            return;
        }

        OutlawState* state = GetState(player);
        if (state->dealing || !HasOwnDamage(spellInfo))
            return;

        float amount = 0.0f;
        switch (spellInfo->Id)
        {
            case SPELL_PISTOL_SHOT:
                amount = WeaponHit(player, BASE_ATTACK) * PistolShotWeaponPct / 100.0f * state->pistolMultiplier;
                break;
            case SPELL_DISPATCH:
                amount = DispatchDamage(player, state->finisherPoints);
                break;
            case SPELL_BETWEEN_THE_EYES:
                amount = BetweenTheEyesDamage(player, state->betweenEyesPoints);
                break;
            case SPELL_BLADE_RUSH:
                amount = WeaponHit(player, BASE_ATTACK) * BladeRushWeaponPct / 100.0f;
                break;
            default:
                break;
        }
        if (amount < 1.0f)
            return;
        uint32 value = player->SpellDamageBonusDone(target, spellInfo, uint32(amount), SPELL_DIRECT_DAMAGE, EFFECT_0);
        damage = int32(target->SpellDamageBonusTaken(player, spellInfo, value, SPELL_DIRECT_DAMAGE));
    }

    // Which hand the coming auto attack is (ModifyFinalDamage does not say)
    void OnBeforeRollMeleeOutcomeAgainst(Unit const* attacker, Unit const* /*victim*/, WeaponAttackType attType,
        int32& /*attackerMaxSkillValueForLevel*/, int32& /*victimMaxSkillValueForLevel*/,
        int32& /*attackerWeaponSkill*/, int32& /*victimDefenseSkill*/, int32& /*crit_chance*/,
        int32& /*miss_chance*/, int32& /*dodge_chance*/, int32& /*parry_chance*/, int32& /*block_chance*/) override
    {
        Player* player = OutlawPlayer(const_cast<Unit*>(attacker));
        if (!player)
            return;
        OutlawState* state = GetState(player);
        state->swingAttack = attType;
        state->swingPending = true;
    }

    // A hit of the rogue as it lands: Blade Flurry repeats it, Main Gauche follows a main-hand one, Combat Potency an
    // off-hand one
    void ModifyFinalDamage(Unit* attacker, Unit* victim, uint32& damage, uint32& absorb,
        SpellInfo const* spellInfo) override
    {
        Player* player = OutlawPlayer(attacker);
        if (!player || !victim || victim == player)
            return;

        uint32 const worth = damage + absorb;
        if (!spellInfo)
        {
            // An auto attack: once, whatever the number of its weapon's damage schools
            OutlawState* state = GetState(player);
            if (!state->swingPending)
                return;
            state->swingPending = false;
            BladeFlurryRepeat(player, victim, worth);
            if (state->swingAttack == OFF_ATTACK)
                CombatPotency(player);
            else if (state->swingAttack == BASE_ATTACK)
                MainGauche(player, victim);
            return;
        }

        if (!IsSingleTargetHit(spellInfo))
            return;
        BladeFlurryRepeat(player, victim, worth);
        if (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE)
            MainGauche(player, victim);
    }

    // Between the Eyes' own stun (its data's): a second per combo point, the knob's (bosses never get it:
    // OnScaleAuraUnitAdd)
    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (!unit || !aura || aura->GetId() != SPELL_BETWEEN_THE_EYES)
            return;
        Player* player = OutlawPlayer(aura->GetCaster());
        if (!player || unit == player || !aura->GetSpellInfo()->HasAura(SPELL_AURA_MOD_STUN))
            return;

        int32 const duration =
            int32(BetweenTheEyesStunMsPerPoint * std::max<uint8>(1, GetState(player)->betweenEyesPoints));
        aura->SetMaxDuration(duration);
        aura->SetDuration(duration);
    }
};

// --- Casts beginning, login, every update -------------------------------------------------------------------------

// Entailleur rapide: Slice and Dice's attack speed the talent's more (per rank), held on whatever Slice and Dice is up
void UpdateSwiftSlasher(Player* player)
{
    OutlawState* state = GetState(player);
    AuraEffect* sliceAndDice = nullptr;
    for (AuraEffect* effect : player->GetAuraEffectsByType(SPELL_AURA_MOD_MELEE_HASTE))
        if (effect->GetCasterGUID() == player->GetGUID() &&
            FirstRank(effect->GetSpellInfo()) == SPELL_SLICE_AND_DICE)
        {
            sliceAndDice = effect;
            break;
        }

    if (!sliceAndDice)
    {
        state->swiftSlasherAmount = 0;
        state->swiftSlasherBonus = 0;
        return;
    }

    // The core recalculates the amount when Slice and Dice is cast again: the bonus is then gone
    bool const applied = state->swiftSlasherAmount && sliceAndDice->GetAmount() == state->swiftSlasherAmount;
    int32 const base = sliceAndDice->GetAmount() - (applied ? state->swiftSlasherBonus : 0);
    int32 const wanted = SwiftSlasherPctPerRank * Rank(player, { TALENT_SWIFT_SLASHER_1, TALENT_SWIFT_SLASHER_2 });
    if (applied && wanted == state->swiftSlasherBonus)
        return;
    if (!applied && !wanted)
    {
        state->swiftSlasherAmount = 0;
        state->swiftSlasherBonus = 0;
        return;
    }

    sliceAndDice->ChangeAmount(base + wanted);
    state->swiftSlasherAmount = wanted ? base + wanted : 0;
    state->swiftSlasherBonus = wanted;
}

class RogueOutlawPlayerScript : public PlayerScript
{
public:
    RogueOutlawPlayerScript() : PlayerScript("RogueOutlawPlayerScript", { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_SPELL_CAST }) { }

    // The Crimson Duelist is gone: its kit, its auras and the WotLK Combat talents leave every rogue still having them
    void OnPlayerLogin(Player* player) override
    {
        if (player->getClass() != CLASS_ROGUE)
            return;
        for (uint32 spellId : RetiredSpells)
        {
            if (player->HasSpell(spellId))
                player->removeSpell(spellId, SPEC_MASK_ALL, false);
            player->RemoveAurasDueToSpell(spellId);
        }
    }

    // What a cast finds as it begins (before its cost, its combo points and its damage): a finisher's combo points,
    // Pistol Shot's Opportunité and Mèches de Peau-Verte
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        SpellInfo const* spellInfo = spell ? spell->GetSpellInfo() : nullptr;
        if (!spellInfo || !IsOutlaw(player))
            return;

        OutlawState* state = GetState(player);
        if (spellInfo->NeedsComboPoints())
        {
            state->finisherSpell = spellInfo->Id;
            state->finisherPoints = player->GetComboPoints();
            if (spellInfo->Id == SPELL_BETWEEN_THE_EYES)
                state->betweenEyesPoints = state->finisherPoints;
        }

        if (spellInfo->Id != SPELL_PISTOL_SHOT)
            return;
        if (spell->IsTriggered())
        {
            // Marteau en éventail's shots
            state->pistolMultiplier = state->fanMultiplier;
            state->pistolOpportunity = false;
            state->pistolGreenskins = false;
            return;
        }

        state->pistolOpportunity = player->HasAura(SPELL_OPPORTUNITY);
        state->opportunityCount = UsesOf(player, SPELL_OPPORTUNITY);
        state->pistolGreenskins = player->HasAura(SPELL_GREENSKINS_WICKERS);
        float multiplier = 1.0f;
        if (state->pistolOpportunity)
            multiplier *= OpportunityMultiplier(player);
        if (state->pistolGreenskins)
            multiplier *= 1.0f + GreenskinsDamagePct / 100.0f;
        state->pistolMultiplier = multiplier;
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_ROGUE)
            return;
        OutlawState* state = GetState(player);
        if (state->updateTimer > diff)
        {
            state->updateTimer -= diff;
            return;
        }
        state->updateTimer = UpdateIntervalMs;

        if (IsOutlaw(player))
            UpdateSwiftSlasher(player);
        else if (state->swiftSlasherAmount)
        {
            // Out of the spec with the bonus on: take it off
            for (AuraEffect* effect : player->GetAuraEffectsByType(SPELL_AURA_MOD_MELEE_HASTE))
                if (effect->GetAmount() == state->swiftSlasherAmount &&
                    FirstRank(effect->GetSpellInfo()) == SPELL_SLICE_AND_DICE)
                {
                    effect->ChangeAmount(effect->GetAmount() - state->swiftSlasherBonus);
                    break;
                }
            state->swiftSlasherAmount = 0;
            state->swiftSlasherBonus = 0;
        }
    }
};
}

void AddRogueOutlawScripts()
{
    new RogueOutlawSpellScript();
    new RogueOutlawUnitScript();
    new RogueOutlawPlayerScript();
}
