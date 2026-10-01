#include "AllSpellScript.h"
#include "CellImpl.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LiveTuning.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Optional.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "UnitScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <vector>

// The Warlock's talents and abilities on the retail-style trees (localTools/warlock/talentTree.json) that spell data
// cannot carry. A talent is its rank spell's aura on the Warlock (learned by mod-custom-classes' TalentTree.cpp), read
// here with HasAura; a specialization is its passive (Affliction, Démonologie, Destruction). The abilities, auras and
// hits named below are in localTools/warlock/Spells.ps1, the stock spells changed in place in StockSpells.ps1; the
// WotLK talents the trees reuse keep their own scripts in the core (spell_warlock.cpp).
//
// The Warlock keeps mana and its demons. Soul Shards (Fragments d'âme) are an aura of up to 5 stacks, with tenths of a
// shard kept here beside it; every specialization fills and spends them, and out of combat they come back to 3.
// Affliction: Curse of Agony's ticks give shards, Malefic Rapture spends one to strike every enemy with the Warlock's
// damage over time effects (once for each of them), Seed of Corruption spends one and spreads Corruption, Soul Rot,
// Phantom Singularity or Vile Taint, Summon Darkglare. Demonology: Shadow Bolt and Demonbolt give shards, Hand of
// Gul'dan spends up to 3 for as many Wild Imps, Call Dreadstalkers 2, Grimoire: Felguard 1; the Wild Imps give
// Demonic Cores (an instant Demonbolt), Implosion and Power Siphon spend them, the Demonic Tyrant extends and empowers
// the demons. Destruction: Immolate's ticks, Incinerate, Conflagrate (2 charges, kept here and shown as stacks) and the
// infernal give tenths of a shard, Chaos Bolt (always a critical strike), Rain of Fire and Shadowburn spend them; Havoc
// copies the single-target spells onto a second enemy.
//
// The summoned demons (Wild Imps, Dreadstalkers, the Tyrant, the Darkglare, the Grimoire's Felguard, the infernal) are
// allied guardians of the Warlock's (modules/mod-warlock SQL), held here: this module sends them at the Warlock's
// target, has them cast what they cast with amounts worked out from the Warlock's spell power, and unsummons them when
// their time is up.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_DESPERATE_PACT_1         = 95602,    // Pacte désespéré
    TALENT_DESPERATE_PACT_2         = 95603,
    TALENT_SOUL_LEECH               = 95604,    // Ponction d'âme
    TALENT_SOUL_CONDUIT_1           = 95606,    // Conduit d'âme
    TALENT_SOUL_CONDUIT_2           = 95607,
    TALENT_WRATHFUL_MINION_1        = 95610,    // Serviteur courroucé
    TALENT_WRATHFUL_MINION_2        = 95611,
    TALENT_DARK_ACCORD_1            = 95612,    // Accord des ténèbres
    TALENT_DARK_ACCORD_2            = 95613,
    TALENT_WRITHE_1                 = 95618,    // Agonie tourmentée
    TALENT_WRITHE_2                 = 95619,
    TALENT_SOW_THE_SEEDS            = 95620,    // Semer les graines
    TALENT_HARVEST_1                = 95625,    // Récolte sinistre
    TALENT_HARVEST_2                = 95626,
    TALENT_HAUNTED_SOUL             = 95627,    // Âme hantée
    TALENT_MALEVOLENT_GAZE          = 95628,    // Regard maléfique
    TALENT_DREAD_TOUCH_1            = 95629,    // Contact de l'effroi
    TALENT_DREAD_TOUCH_2            = 95630,
    TALENT_TORMENTED_CRESCENDO      = 95631,    // Crescendo tourmenté
    TALENT_DOOM_BLOSSOM             = 95632,    // Floraison funeste
    TALENT_DEMONIC_CALLING_1        = 95633,    // Appel démoniaque
    TALENT_DEMONIC_CALLING_2        = 95634,
    TALENT_IMP_HORDE_1              = 95635,    // Horde de diablotins
    TALENT_IMP_HORDE_2              = 95636,
    TALENT_FEL_FIRE_1               = 95637,    // Feu gangrené
    TALENT_FEL_FIRE_2               = 95638,
    TALENT_DREADLASH                = 95641,    // Fouet de l'effroi
    TALENT_INNER_DEMONS             = 95642,    // Démons intérieurs
    TALENT_SACRIFICED_SOULS_1       = 95643,    // Âmes sacrifiées
    TALENT_SACRIFICED_SOULS_2       = 95644,
    TALENT_REIGN_1                  = 95645,    // Règne de la tyrannie
    TALENT_REIGN_2                  = 95646,
    TALENT_SOULBOUND_TYRANT         = 95647,    // Tyran lié à l'âme
    TALENT_FEL_RAGE_1               = 95650,    // Gangregarde enragé
    TALENT_FEL_RAGE_2               = 95651,
    TALENT_GULDAN_SUPREME           = 95654,    // Gul'dan suprême
    TALENT_DOOM                     = 95655,    // Trépas
    TALENT_ERADICATION_1            = 95656,    // Éradication
    TALENT_ERADICATION_2            = 95657,
    TALENT_ROARING_BLAZE            = 95658,    // Brasier rugissant
    TALENT_INFERNO                  = 95659,    // Inferno
    TALENT_INTERNAL_COMBUSTION      = 95660,    // Combustion interne
    TALENT_CHAOS_CONFLAGRATION_1    = 95661,    // Conflagration du chaos
    TALENT_CHAOS_CONFLAGRATION_2    = 95662,
    TALENT_LORD_OF_FLAMES           = 95667,    // Seigneur des flammes
    TALENT_EMBERS_1                 = 95670,    // Braises vives
    TALENT_EMBERS_2                 = 95671,
    TALENT_AVATAR                   = 95672,    // Avatar de destruction
    TALENT_RAIN_OF_CHAOS            = 95673,    // Pluie de chaos
};

// The abilities, auras and hits of localTools/warlock/Spells.ps1
enum Spells : uint32
{
    SPELL_SOUL_SHARDS               = 95700,
    SPELL_DEMONIC_CORE              = 95701,
    SPELL_CONFLAGRATION_CHARGES     = 95702,
    SPELL_CRESCENDO_BUFF            = 95703,
    SPELL_SOUL_LEECH_SHIELD         = 95704,
    SPELL_SACRIFICE_BUFF            = 95705,
    SPELL_ERADICATION_MARK          = 95708,
    SPELL_ROARING_BLAZE_MARK        = 95709,
    SPELL_DREAD_TOUCH_MARK          = 95710,
    SPELL_DOOM_MARK                 = 95711,
    SPELL_DEMONIC_CALLING_BUFF      = 95712,
    SPELL_CHAOS_CONFLAGRATION_BUFF  = 95713,
    SPELL_DARK_PACT_SHIELD          = 95714,
    SPELL_DARK_PACT                 = 95721,
    SPELL_BURNING_RUSH              = 95722,
    SPELL_GRIMOIRE_OF_SACRIFICE     = 95723,
    SPELL_SACRIFICE_HIT             = 95724,
    SPELL_HARVEST_HIT               = 95729,
    SPELL_MALEFIC_RAPTURE           = 95730,
    SPELL_SOUL_ROT                  = 95731,
    SPELL_PHANTOM_SINGULARITY       = 95732,
    SPELL_VILE_TAINT                = 95733,
    SPELL_SUMMON_DARKGLARE          = 95734,
    SPELL_MALEFIC_RAPTURE_HIT       = 95735,
    SPELL_PHANTOM_SINGULARITY_HIT   = 95736,
    SPELL_VILE_TAINT_DOT            = 95737,
    SPELL_EYE_BEAM                  = 95738,
    SPELL_DOOM_BLOSSOM_HIT          = 95739,
    SPELL_HAND_OF_GULDAN            = 95740,
    SPELL_DEMONBOLT                 = 95741,
    SPELL_CALL_DREADSTALKERS        = 95742,
    SPELL_IMPLOSION                 = 95743,
    SPELL_POWER_SIPHON              = 95744,
    SPELL_BILESCOURGE_BOMBERS       = 95745,
    SPELL_DEMONIC_STRENGTH          = 95746,
    SPELL_GRIMOIRE_FELGUARD         = 95747,
    SPELL_SUMMON_DEMONIC_TYRANT     = 95748,
    SPELL_HAND_OF_GULDAN_HIT        = 95750,
    SPELL_FEL_FIREBOLT              = 95751,
    SPELL_DREADBITE                 = 95752,
    SPELL_IMPLOSION_HIT             = 95753,
    SPELL_BILESCOURGE_HIT           = 95754,
    SPELL_FELSTORM_HIT              = 95755,
    SPELL_TYRANT_DEMONFIRE          = 95756,
    SPELL_LEGION_STRIKE             = 95757,
    SPELL_DOOM_HIT                  = 95758,
    SPELL_DEMONIC_POWER             = 95759,
    SPELL_CONFLAGRATION             = 95760,
    SPELL_RAIN_OF_FIRE              = 95761,
    SPELL_HAVOC                     = 95762,
    SPELL_CHANNEL_DEMONFIRE         = 95763,
    SPELL_CATACLYSM                 = 95764,
    SPELL_SUMMON_INFERNAL           = 95765,
    SPELL_RAIN_OF_FIRE_HIT          = 95766,
    SPELL_DEMONFIRE_BOLT            = 95767,
    SPELL_HAVOC_RELAY               = 95768,
    SPELL_CATACLYSM_HIT             = 95769,
    SPELL_INFERNAL_IMMOLATION       = 95770,
    SPELL_INFERNAL_IMPACT           = 95771,
    SPELL_INTERNAL_COMBUSTION_HIT   = 95772,

    // The specializations' passives
    SPELL_SPEC_AFFLICTION           = 95880,
    SPELL_SPEC_DEMONOLOGY           = 95881,
    SPELL_SPEC_DESTRUCTION          = 95882,

    // Stock (first ranks)
    SPELL_CORRUPTION_R1             = 172,
    SPELL_IMMOLATE_R1               = 348,
    SPELL_SHADOW_BOLT_R1            = 686,
    SPELL_CURSE_OF_AGONY_R1         = 980,
    SPELL_SEARING_PAIN_R1           = 5676,
    SPELL_SOUL_FIRE_R1              = 6353,
    SPELL_SHADOWBURN_R1             = 17877,
    SPELL_SEED_OF_CORRUPTION_R1     = 27243,
    SPELL_INCINERATE_R1             = 29722,
    SPELL_UNSTABLE_AFFLICTION_R1    = 30108,
    SPELL_HAUNT_R1                  = 48181,
    SPELL_CHAOS_BOLT_R1             = 50796,
};

// The demons this module summons (modules/mod-warlock SQL), and the Felguard pet
enum Demons : uint32
{
    NPC_WILD_IMP                    = 95600,
    NPC_DREADSTALKER                = 95601,
    NPC_DEMONIC_TYRANT              = 95602,
    NPC_DARKGLARE                   = 95603,
    NPC_GRIMOIRE_FELGUARD           = 95604,
    NPC_INFERNAL                    = 95605,
    NPC_FELGUARD                    = 17252,
};

enum DemonKind : uint8
{
    DEMON_WILD_IMP,
    DEMON_DREADSTALKER,
    DEMON_TYRANT,
    DEMON_DARKGLARE,
    DEMON_GRIMOIRE_FELGUARD,
    DEMON_INFERNAL,
};

// Family flags: Corruption, Immolate and Curse of Agony (word 0), Unstable Affliction, Haunt and Seed of Corruption's
// blast (word 1)
constexpr uint32 FlagCorruption = 0x2;
constexpr uint32 FlagImmolate = 0x4;
constexpr uint32 FlagAgony = 0x400;
constexpr uint32 FlagUnstable = 0x100;
constexpr uint32 FlagHaunt = 0x40000;
constexpr uint32 FlagSeedBlast = 0x8000;

// An allied guardian of the Warlock's (category ally, type guardian), as mod-hunter's wild beasts: the pet category
// would take the pet's slot and send the Warlock's own demon away
constexpr uint32 DemonSummonProperties = 61;
// The creature is summoned for longer than this; the module unsummons it when its time is up (the Tyrant extends it)
constexpr uint32 DemonMaxLifeMs = 120000;
// Their health, a share of the Warlock's: Wild Imp, Dreadstalker, Demonic Tyrant, Darkglare, Felguard, infernal
constexpr std::array<float, 6> DemonHealthShares = { 0.15f, 0.35f, 0.5f, 0.4f, 0.5f, 0.6f };

// --- Tuning (README.md) ----------------------------------------------------------------------------------------------
// Soul Shards
constexpr uint8 ShardMax = 5;
LiveTuning::KnobInt const ShardRest("warlock.shard_rest", 3);  // out of combat they come back to this
LiveTuning::KnobUInt const ShardRestDelayMs("warlock.shard_rest_delay_ms", 5000);
LiveTuning::KnobUInt const ShardRestStepMs("warlock.shard_rest_step_ms", 2000);
// a Curse of Agony tick, at one curse; / sqrt(curses ticking)
LiveTuning::KnobInt const AgonyShardChance("warlock.agony_shard_chance", 22);
LiveTuning::KnobInt const WritheChancePerRank("warlock.writhe_chance_per_rank", 5);  // Agonie tourmentée
LiveTuning::KnobInt const SoulConduitPerRank("warlock.soul_conduit_per_rank", 5);
LiveTuning::KnobInt const CrescendoChance("warlock.crescendo_chance", 25);
LiveTuning::KnobUInt const ImmolateFragments("warlock.immolate_fragments", 2);  // tenths of a shard: an Immolate tick
// an Incinerate hit, +1 on a critical strike
LiveTuning::KnobUInt const IncinerateFragments("warlock.incinerate_fragments", 2);
LiveTuning::KnobUInt const ConflagrationFragments("warlock.conflagration_fragments", 5);
LiveTuning::KnobUInt const InfernalFragments("warlock.infernal_fragments", 1);       // an infernal's Immolation
// Affliction
LiveTuning::Knob const MaleficRange("warlock.malefic_range", 40.0f);
LiveTuning::KnobInt const SoulRotSpread("warlock.soul_rot_spread", 3);
LiveTuning::Knob const SpreadRange("warlock.spread_range", 10.0f);
LiveTuning::KnobUInt const PhantomPeriodMs("warlock.phantom_period_ms", 2000);
LiveTuning::Knob const PhantomRadius("warlock.phantom_radius", 8.0f);
LiveTuning::Knob const PhantomLeech("warlock.phantom_leech", 0.15f);
LiveTuning::Knob const VileTaintRadius("warlock.vile_taint_radius", 8.0f);
LiveTuning::KnobUInt const DarkglareMs("warlock.darkglare_ms", 20000);
LiveTuning::KnobInt const DarkglareExtendMs("warlock.darkglare_extend_ms", 8000);
LiveTuning::KnobInt const MalevolentGazeExtendMs("warlock.malevolent_gaze_extend_ms", 4000);
LiveTuning::Knob const DarkglareRange("warlock.darkglare_range", 40.0f);
// share of spell power a beam, before the damage over time effects
LiveTuning::Knob const EyeBeamPower("warlock.eye_beam_power", 0.3f);
LiveTuning::Knob const EyeBeamPerDot("warlock.eye_beam_per_dot", 0.25f);
LiveTuning::KnobInt const HarvestChancePerRank("warlock.harvest_chance_per_rank", 5);
LiveTuning::Knob const DreadTouchPerRank("warlock.dread_touch_per_rank", 0.075f);
LiveTuning::Knob const DoomBlossomRadius("warlock.doom_blossom_radius", 8.0f);
// Demonology
LiveTuning::KnobInt const GuldanMaxShards("warlock.guldan_max_shards", 3);
LiveTuning::Knob const GuldanRadius("warlock.guldan_radius", 8.0f);
LiveTuning::KnobUInt const WildImpMs("warlock.wild_imp_ms", 15000);
LiveTuning::KnobUInt const ImpHordeMsPerRank("warlock.imp_horde_ms_per_rank", 2000);
LiveTuning::KnobUInt const FireboltPeriodMs("warlock.firebolt_period_ms", 2000);
LiveTuning::Knob const FireboltPower("warlock.firebolt_power", 0.05f);
LiveTuning::Knob const FelFirePerRank("warlock.fel_fire_per_rank", 0.1f);
LiveTuning::KnobInt const ImpCoreChance("warlock.imp_core_chance", 15);
LiveTuning::KnobInt const CoreMax("warlock.core_max", 4);
LiveTuning::KnobInt const DemonicCallingPerRank("warlock.demonic_calling_per_rank", 10);
LiveTuning::KnobUInt const DreadstalkerMs("warlock.dreadstalker_ms", 12000);
LiveTuning::Knob const DreadbitePower("warlock.dreadbite_power", 0.9f);
LiveTuning::Knob const DreadlashRadius("warlock.dreadlash_radius", 8.0f);
LiveTuning::Knob const StalkerSwingPower("warlock.stalker_swing_power", 0.3f);  // a 2 s swing
LiveTuning::Knob const ImplosionRadius("warlock.implosion_radius", 8.0f);
LiveTuning::KnobInt const PowerSiphonImps("warlock.power_siphon_imps", 2);
LiveTuning::KnobUInt const BombersMs("warlock.bombers_ms", 6000);
LiveTuning::KnobUInt const BombersPeriodMs("warlock.bombers_period_ms", 1000);
LiveTuning::Knob const BombersRadius("warlock.bombers_radius", 8.0f);
LiveTuning::KnobUInt const FelstormMs("warlock.felstorm_ms", 5000);
LiveTuning::KnobUInt const FelstormPeriodMs("warlock.felstorm_period_ms", 1000);
LiveTuning::Knob const FelstormRadius("warlock.felstorm_radius", 8.0f);
LiveTuning::Knob const FelstormPower("warlock.felstorm_power", 0.35f);
LiveTuning::KnobUInt const GrimoireFelguardMs("warlock.grimoire_felguard_ms", 17000);
LiveTuning::KnobUInt const LegionStrikePeriodMs("warlock.legion_strike_period_ms", 3000);
LiveTuning::Knob const LegionStrikePower("warlock.legion_strike_power", 0.75f);
LiveTuning::Knob const GrimoireSwingPower("warlock.grimoire_swing_power", 0.35f);
LiveTuning::KnobUInt const TyrantMs("warlock.tyrant_ms", 15000);
LiveTuning::KnobUInt const TyrantExtendMs("warlock.tyrant_extend_ms", 15000);
LiveTuning::KnobUInt const TyrantPeriodMs("warlock.tyrant_period_ms", 2000);
LiveTuning::Knob const TyrantPower("warlock.tyrant_power", 0.4f);
LiveTuning::Knob const TyrantEmpowerment("warlock.tyrant_empowerment", 1.15f);
LiveTuning::Knob const ReignPerRank("warlock.reign_per_rank", 0.05f);
LiveTuning::KnobInt const SoulboundTyrantShards("warlock.soulbound_tyrant_shards", 3);
LiveTuning::KnobUInt const InnerDemonsMs("warlock.inner_demons_ms", 12000);
LiveTuning::Knob const SacrificedSoulsPerRank("warlock.sacrificed_souls_per_rank", 0.02f);
LiveTuning::Knob const WrathfulMinionPerRank("warlock.wrathful_minion_per_rank", 0.05f);
LiveTuning::Knob const FelRagePerRank("warlock.fel_rage_per_rank", 0.1f);
LiveTuning::Knob const DoomRadius("warlock.doom_radius", 8.0f);
LiveTuning::Knob const DemonRange("warlock.demon_range", 40.0f);
// Destruction
LiveTuning::KnobUInt const ConflagrationCharges("warlock.conflagration_charges", 2);
LiveTuning::Knob const HavocRange("warlock.havoc_range", 40.0f);
LiveTuning::KnobUInt const RainOfFireMs("warlock.rain_of_fire_ms", 8000);
LiveTuning::KnobUInt const RainOfFirePeriodMs("warlock.rain_of_fire_period_ms", 1000);
LiveTuning::Knob const RainOfFireRadius("warlock.rain_of_fire_radius", 8.0f);
LiveTuning::Knob const InfernoFactor("warlock.inferno_factor", 1.2f);
LiveTuning::KnobInt const InfernoFragmentChance("warlock.inferno_fragment_chance", 20);
LiveTuning::KnobInt const RainOfChaosChance("warlock.rain_of_chaos_chance", 15);
LiveTuning::KnobUInt const DemonfirePeriodMs("warlock.demonfire_period_ms", 250);
LiveTuning::Knob const DemonfireRange("warlock.demonfire_range", 40.0f);
LiveTuning::Knob const CataclysmRadius("warlock.cataclysm_radius", 8.0f);
LiveTuning::KnobUInt const InfernalMs("warlock.infernal_ms", 30000);
LiveTuning::KnobUInt const LordOfFlamesMs("warlock.lord_of_flames_ms", 10000);
LiveTuning::Knob const LordOfFlamesFactor("warlock.lord_of_flames_factor", 1.5f);
// Avatar de destruction, Pluie de chaos
LiveTuning::KnobUInt const SummonedInfernalMs("warlock.summoned_infernal_ms", 8000);
LiveTuning::Knob const InfernalRadius("warlock.infernal_radius", 8.0f);
LiveTuning::KnobUInt const InfernalPeriodMs("warlock.infernal_period_ms", 1500);
LiveTuning::Knob const InfernalImmolationPower("warlock.infernal_immolation_power", 0.24f);
LiveTuning::Knob const InfernalSwingPower("warlock.infernal_swing_power", 0.4f);
LiveTuning::Knob const RoaringBlazeFactor("warlock.roaring_blaze_factor", 1.25f);
LiveTuning::Knob const EradicationPerRank("warlock.eradication_per_rank", 0.05f);
LiveTuning::KnobInt const ChaosConflagrationPerRank("warlock.chaos_conflagration_per_rank", 25);
LiveTuning::KnobInt const ShadowburnExecuteCrit("warlock.shadowburn_execute_crit", 50);
LiveTuning::KnobUInt const ShadowburnRefundMs("warlock.shadowburn_refund_ms", 5000);
LiveTuning::KnobUInt const InternalCombustionMs("warlock.internal_combustion_ms", 5000);
// Class tree
LiveTuning::Knob const DarkPactSacrifice("warlock.dark_pact_sacrifice", 0.2f);
LiveTuning::Knob const DarkPactShield("warlock.dark_pact_shield", 2.0f);
LiveTuning::Knob const DesperatePactPerRank("warlock.desperate_pact_per_rank", 0.25f);
LiveTuning::KnobInt const BurningRushLossPct("warlock.burning_rush_loss_pct", 4);
LiveTuning::KnobInt const BurningRushFloorPct("warlock.burning_rush_floor_pct", 10);
LiveTuning::Knob const SoulLeechShare("warlock.soul_leech_share", 0.03f);
LiveTuning::Knob const SoulLeechCap("warlock.soul_leech_cap", 0.10f);
LiveTuning::KnobInt const SacrificeChance("warlock.sacrifice_chance", 35);
LiveTuning::KnobUInt const SacrificeCooldownMs("warlock.sacrifice_cooldown_ms", 1500);
// Packs (the combat bench, Fire mage as the reference): each area hit whole up to AreaFullTargets enemies and
// sqrt(AreaFullTargets / enemies) of it past them, as the Shaman's 8 yd areas
LiveTuning::KnobInt const AreaFullTargets("warlock.area_full_targets", 8);
LiveTuning::KnobInt const AreaMaxTargets("warlock.area_max_targets", 20);
constexpr uint32 HoldCheckMs = 1000;

struct Charges
{
    bool initialized = false;
    uint8 count = 0;
    int32 rechargeMs = 0;
    bool clearCooldown = false;     // the cooldown its cast just started is taken back on the next update
};

// A demon this module summoned
struct Demon
{
    ObjectGuid guid;
    DemonKind kind = DEMON_WILD_IMP;
    uint32 untilMs = 0;
    uint32 nextMs = 0;
    float power = 1.0f;             // the Tyrant's Demonfire: Règne de la tyrannie, fixed at its summon
};

// An effect held on a spot of the ground (Rain of Fire, Bilescourge Bombers)
struct GroundEffect
{
    uint32 untilMs = 0;
    uint32 nextMs = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool Active() const { return untilMs != 0; }
    Position Spot() const { return Position(x, y, z); }
    void Start(Position const& spot, uint32 now, uint32 durationMs, uint32 firstTickMs)
    {
        x = spot.GetPositionX();
        y = spot.GetPositionY();
        z = spot.GetPositionZ();
        untilMs = now + durationMs;
        nextMs = now + firstTickMs;
    }
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct WarlockState : public DataMap::Base
{
    uint8 fragments = 0;            // tenths of a shard beside the whole ones the aura shows
    bool shardsSeeded = false;
    uint32 restMs = 0;
    Charges conflagration;
    std::vector<Demon> demons;
    GroundEffect rainOfFire;
    GroundEffect bombers;
    ObjectGuid lastTarget;
    ObjectGuid phantomTarget;
    uint32 phantomNextMs = 0;
    ObjectGuid havocTarget;
    ObjectGuid shadowburnTarget;
    uint32 shadowburnUntilMs = 0;
    uint32 felstormUntilMs = 0;
    uint32 felstormNextMs = 0;
    uint32 demonfireNextMs = 0;
    uint32 tyrantUntilMs = 0;
    uint32 innerDemonsMs = 0;
    uint32 sacrificeReadyMs = 0;
    float leechPool = 0.0f;
    uint32 holdTimer = 0;
    // The shares of the hits this module hands out in a burst (an area's falloff, Hand of Gul'dan's shards), set
    // before the burst: those hits land with their cast
    std::map<uint32, float> hitFactors;
    // The Curses of Agony that ticked lately, for their shard chance
    std::vector<std::pair<ObjectGuid, uint32>> agonyTicks;
};

constexpr char const* StateKey = "WarlockTalentState";

WarlockState* GetState(Player* player)
{
    return player->CustomData.GetDefault<WarlockState>(StateKey);
}

Player* Warlock(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_WARLOCK ? player : nullptr;
}

// The Warlock who owns a demon: its pet, or one of the demons it summoned
Player* DemonOwner(Unit* unit)
{
    if (!unit || unit->IsPlayer())
        return nullptr;
    return Warlock(unit->GetOwner());
}

uint32 Now()
{
    return uint32(GameTime::GetGameTimeMS().count());
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsAffliction(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_AFFLICTION);
}

bool IsDemonology(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_DEMONOLOGY);
}

bool IsDestruction(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_DESTRUCTION);
}

// A two-rank talent's rank: 2, 1 or 0
uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

uint8 Stacks(Unit const* unit, uint32 spellId)
{
    Aura* aura = unit->GetAura(spellId);
    return aura ? aura->GetStackAmount() : 0;
}

// An aura of stacks on the Warlock at a count (none removes it); `effectAmount` keeps effect 0 at that amount whatever
// the stacks (the core multiplies it by them)
void ShowStacks(Player* player, uint32 spellId, uint8 stacks, Optional<int32> effectAmount = std::nullopt)
{
    if (!stacks)
    {
        player->RemoveAurasDueToSpell(spellId);
        return;
    }
    Aura* aura = player->GetAura(spellId);
    if (!aura)
        aura = player->AddAura(spellId, player);
    if (!aura)
        return;
    aura->SetStackAmount(stacks);
    aura->RefreshDuration();
    if (effectAmount)
        if (AuraEffect* effect = aura->GetEffect(EFFECT_0))
            effect->ChangeAmount(*effectAmount);
}

int32 Amount(float value)
{
    return int32(std::clamp(value, 1.0f, float(std::numeric_limits<int32>::max() / 2)));
}

float SpellPower(Player* player)
{
    return float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW));
}

// Each hit's share on a pack of `enemies`: whole up to AreaFullTargets, then sqrt(AreaFullTargets / enemies)
float AreaFalloff(std::size_t enemies)
{
    return enemies <= AreaFullTargets ? 1.0f : std::sqrt(float(AreaFullTargets) / float(enemies));
}

bool IsEnemy(Player* player, Unit* unit)
{
    return unit && unit->IsAlive() && unit->IsInMap(player) && player->IsValidAttackTarget(unit) &&
        !unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
}

// Enemies of the player within range of center, center left out
std::list<Unit*> EnemiesNear(Player* player, WorldObject* center, float range)
{
    std::list<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, targets, check);
    Cell::VisitObjects(center, searcher, range);
    targets.remove_if([player, center](Unit* unit)
    {
        return unit == center || !IsEnemy(player, unit);
    });
    return targets;
}

// Enemies within `radius` of a spot (the player's search reaches it), AreaMaxTargets at most
std::vector<Unit*> EnemiesAtSpot(Player* player, Position const& spot, float radius)
{
    std::vector<Unit*> enemies;
    float const reach = player->GetExactDist(&spot) + radius;
    for (Unit* enemy : EnemiesNear(player, player, reach))
    {
        if (enemy->GetExactDist(&spot) > radius + enemy->GetCombatReach())
            continue;
        enemies.push_back(enemy);
        if (enemies.size() >= AreaMaxTargets)
            break;
    }
    return enemies;
}

// A unit and the enemies around it, AreaMaxTargets at most
std::vector<Unit*> EnemiesAround(Player* player, Unit* center, float radius)
{
    std::vector<Unit*> enemies;
    if (IsEnemy(player, center))
        enemies.push_back(center);
    for (Unit* enemy : EnemiesNear(player, center, radius))
    {
        if (enemies.size() >= AreaMaxTargets)
            break;
        enemies.push_back(enemy);
    }
    return enemies;
}

// The highest rank of a chain the player knows, 0 without one
uint32 KnownRank(Player* player, uint32 firstRank)
{
    uint32 known = 0;
    for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpell(rank))
            known = rank;
    return known;
}

// Every rank of a spell the player knows, its cooldown taken back
void ClearChainCooldown(Player* player, uint32 spellId)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->RemoveSpellCooldown(rank, true);
}

bool HasImmolate(Player* player, Unit* target)
{
    return target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARLOCK, FlagImmolate, 0, 0,
        player->GetGUID());
}

bool HasUnstableAffliction(Player* player, Unit* target)
{
    return target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARLOCK, 0, FlagUnstable, 0,
        player->GetGUID());
}

// The Warlock's effects of damage over time on a target, each counted once: its periodic damage auras, Haunt and
// Phantom Singularity
uint8 DotCount(Player* player, Unit* target)
{
    std::vector<uint32> seen;
    for (AuraEffect const* effect : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE))
    {
        if (effect->GetCasterGUID() != player->GetGUID() ||
            effect->GetSpellInfo()->SpellFamilyName != SPELLFAMILY_WARLOCK)
            continue;
        uint32 const id = effect->GetId();
        if (std::find(seen.begin(), seen.end(), id) == seen.end())
            seen.push_back(id);
    }
    uint8 count = uint8(std::min<std::size_t>(seen.size(), 10));
    if (target->GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_WARLOCK, 0, FlagHaunt, 0, player->GetGUID()))
        ++count;
    if (target->HasAura(SPELL_PHANTOM_SINGULARITY, player->GetGUID()))
        ++count;
    return count;
}

// The enemy the Warlock fights: its last spell's target, its selection, or the nearest enemy in combat (any enemy
// near while the Warlock fights: a pack's next one may not have been struck yet)
Unit* CurrentTarget(Player* player, WarlockState* state)
{
    Unit* target = ObjectAccessor::GetUnit(*player, state->lastTarget);
    if (IsEnemy(player, target) && player->IsWithinDistInMap(target, DemonRange))
        return target;
    target = player->GetSelectedUnit();
    if (IsEnemy(player, target) && player->IsWithinDistInMap(target, DemonRange))
    {
        state->lastTarget = target->GetGUID();
        return target;
    }
    Unit* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (Unit* enemy : EnemiesNear(player, player, DemonRange))
    {
        if (!enemy->IsInCombat() && !player->IsInCombat())
            continue;
        float const distance = player->GetExactDist(enemy);
        if (distance < nearestDistance)
        {
            nearest = enemy;
            nearestDistance = distance;
        }
    }
    if (nearest)
        state->lastTarget = nearest->GetGUID();
    return nearest;
}

float HitFactor(WarlockState* state, uint32 spellId)
{
    auto const itr = state->hitFactors.find(spellId);
    return itr == state->hitFactors.end() ? 1.0f : itr->second;
}

// Casts a hit the Warlock hands out (its coefficient in the SQL) at each enemy, with the burst's share set first
void Burst(Player* player, WarlockState* state, uint32 spellId, std::vector<Unit*> const& enemies, float share = 1.0f)
{
    if (enemies.empty())
        return;
    state->hitFactors[spellId] = share * AreaFalloff(enemies.size());
    for (Unit* enemy : enemies)
        player->CastSpell(enemy, spellId, true);
}

// --- Soul Shards -----------------------------------------------------------------------------------------------------

uint8 Shards(Player const* player)
{
    return Stacks(player, SPELL_SOUL_SHARDS);
}

void SetShards(Player* player, WarlockState* state, uint8 shards)
{
    shards = std::min(shards, ShardMax);
    if (shards >= ShardMax)
        state->fragments = 0;
    if (shards != Shards(player))
        ShowStacks(player, SPELL_SOUL_SHARDS, shards);
}

// Tenths of a shard
void AddFragments(Player* player, WarlockState* state, uint32 tenths)
{
    uint32 const total = std::min<uint32>(Shards(player) * 10u + state->fragments + tenths, ShardMax * 10u);
    state->fragments = uint8(total % 10);
    SetShards(player, state, uint8(total / 10));
}

void AddShards(Player* player, WarlockState* state, uint8 shards)
{
    AddFragments(player, state, shards * 10u);
}

// Conduit d'âme: each shard spent may come back
void SpendShards(Player* player, WarlockState* state, uint8 count)
{
    uint8 const have = Shards(player);
    uint8 const spent = std::min(have, count);
    uint8 refunded = 0;
    if (uint8 const rank = Rank(player, TALENT_SOUL_CONDUIT_1, TALENT_SOUL_CONDUIT_2))
        for (uint8 index = 0; index < spent; ++index)
            if (roll_chance_i(SoulConduitPerRank * rank))
                ++refunded;
    SetShards(player, state, uint8(have - spent + refunded));
}

// What a cast costs in shards (0 for anything else); Hand of Gul'dan asks for one at least and takes up to 3
uint8 ShardCost(Player* player, uint32 id, uint32 firstRank)
{
    switch (id)
    {
        case SPELL_MALEFIC_RAPTURE:
            return player->HasAura(SPELL_CRESCENDO_BUFF) ? 0 : 1;
        case SPELL_VILE_TAINT:
        case SPELL_HAND_OF_GULDAN:
        case SPELL_GRIMOIRE_FELGUARD:
            return 1;
        case SPELL_CALL_DREADSTALKERS:
            return player->HasAura(SPELL_DEMONIC_CALLING_BUFF) ? 1 : 2;
        case SPELL_BILESCOURGE_BOMBERS:
            return 2;
        case SPELL_RAIN_OF_FIRE:
            return 3;
        default:
            break;
    }
    switch (firstRank)
    {
        case SPELL_SEED_OF_CORRUPTION_R1:
            return IsAffliction(player) ? 1 : 0;
        case SPELL_CHAOS_BOLT_R1:
            return 2;
        case SPELL_SHADOWBURN_R1:
            return 1;
        default:
            return 0;
    }
}

// --- Conflagration's charges ----------------------------------------------------------------------------------------

int32 Recharge(Player* player, uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    int32 cooldown = spellInfo ? int32(std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime)) : 0;
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
    return std::max(cooldown, 1000);
}

void SpendConflagration(Player* player, WarlockState* state)
{
    Charges& charges = state->conflagration;
    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = ConflagrationCharges;
    }
    if (charges.count >= ConflagrationCharges)
        charges.rechargeMs = Recharge(player, SPELL_CONFLAGRATION);
    if (charges.count > 0)
        --charges.count;
    charges.clearCooldown = charges.count > 0;
    ShowStacks(player, SPELL_CONFLAGRATION_CHARGES, charges.count);
}

void UpdateConflagration(Player* player, WarlockState* state, uint32 diff)
{
    Charges& charges = state->conflagration;
    if (!player->HasSpell(SPELL_CONFLAGRATION))
    {
        if (charges.initialized || player->HasAura(SPELL_CONFLAGRATION_CHARGES))
        {
            charges = Charges();
            player->RemoveAurasDueToSpell(SPELL_CONFLAGRATION_CHARGES);
        }
        return;
    }
    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = ConflagrationCharges;
    }
    if (charges.clearCooldown)
    {
        charges.clearCooldown = false;
        ClearChainCooldown(player, SPELL_CONFLAGRATION);
    }
    if (charges.count >= ConflagrationCharges)
    {
        charges.count = ConflagrationCharges;
        if (Stacks(player, SPELL_CONFLAGRATION_CHARGES) != ConflagrationCharges)
            ShowStacks(player, SPELL_CONFLAGRATION_CHARGES, ConflagrationCharges);
        return;
    }
    charges.rechargeMs -= int32(diff);
    if (charges.rechargeMs > 0)
        return;
    // A charge back: usable again if it was the last one gone
    if (!charges.count)
        ClearChainCooldown(player, SPELL_CONFLAGRATION);
    ++charges.count;
    charges.rechargeMs = charges.count < ConflagrationCharges ?
        Recharge(player, SPELL_CONFLAGRATION) + charges.rechargeMs : 0;
    ShowStacks(player, SPELL_CONFLAGRATION_CHARGES, charges.count);
}

// --- The demons ------------------------------------------------------------------------------------------------------

bool IsFelguard(Unit const* unit)
{
    return unit->GetEntry() == NPC_FELGUARD || unit->GetEntry() == NPC_GRIMOIRE_FELGUARD;
}

// What the Warlock's demons gain: Serviteur courroucé, the Tyrant's empowerment, Gangregarde enragé for the Felguards
float DemonBonus(Player* player, WarlockState* state, Unit const* demon)
{
    float factor = 1.0f + WrathfulMinionPerRank *
        float(Rank(player, TALENT_WRATHFUL_MINION_1, TALENT_WRATHFUL_MINION_2));
    if (Now() < state->tyrantUntilMs)
        factor *= TyrantEmpowerment;
    if (demon && IsFelguard(demon))
        factor *= 1.0f + FelRagePerRank * float(Rank(player, TALENT_FEL_RAGE_1, TALENT_FEL_RAGE_2));
    return factor;
}

// An amount a demon's hit carries: a share of the Warlock's spell power, its damage done and its demons' bonuses
float DemonHit(Player* player, WarlockState* state, Unit const* demon, float power, SpellSchoolMask school)
{
    return SpellPower(player) * power * DemonBonus(player, state, demon) *
        player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, school);
}

Creature* FindDemon(Player* player, Demon const& demon)
{
    return ObjectAccessor::GetCreature(*player, demon.guid);
}

void Unsummon(Creature* creature)
{
    if (TempSummon* summon = creature->ToTempSummon())
        summon->UnSummon();
    else
        creature->DespawnOrUnsummon();
}

uint8 CountDemons(WarlockState* state, Optional<DemonKind> kind = std::nullopt)
{
    return uint8(std::count_if(state->demons.begin(), state->demons.end(), [kind](Demon const& demon)
    {
        return !kind || demon.kind == *kind;
    }));
}

// The demons at the Warlock's side: those it summoned, and its pet
uint8 DemonsAtSide(Player* player, WarlockState* state)
{
    Pet* pet = player->GetPet();
    return uint8(CountDemons(state) + (pet && pet->IsAlive() ? 1 : 0));
}

// A melee demon's swing, a share of the Warlock's spell power every 2 s (its bonuses come in ModifyMeleeDamage)
void ArmDemon(Creature* demon, float swing)
{
    demon->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, swing * 0.9f);
    demon->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, swing * 1.1f);
    demon->SetAttackTime(BASE_ATTACK, 2000);
    demon->UpdateDamagePhysical(BASE_ATTACK);
}

void Engage(Creature* demon, Unit* target)
{
    if (!target || (demon->GetVictim() == target && target->IsAlive()))
        return;
    if (demon->IsAIEnabled && demon->AI())
        demon->AI()->AttackStart(target);
}

// A demon at the Warlock's side for `durationMs`: the melee ones sent at the target, the casters following the Warlock
Creature* SummonDemon(Player* player, WarlockState* state, DemonKind kind, Position const& at, uint32 durationMs,
                      Unit* target, float power = 1.0f)
{
    static constexpr std::array<uint32, 6> Entries = { NPC_WILD_IMP, NPC_DREADSTALKER, NPC_DEMONIC_TYRANT,
        NPC_DARKGLARE, NPC_GRIMOIRE_FELGUARD, NPC_INFERNAL };
    SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(DemonSummonProperties);
    TempSummon* demon = player->GetMap()->SummonCreature(Entries[kind], at, properties, DemonMaxLifeMs, player);
    if (!demon)
        return nullptr;

    // Its health a share of the Warlock's (a guardian's own is a level's default, a few thousand)
    uint32 const health = std::max<uint32>(1, uint32(float(player->GetMaxHealth()) * DemonHealthShares[kind]));
    demon->SetMaxHealth(health);
    demon->SetHealth(health);

    uint32 const now = Now();
    Demon entry;
    entry.guid = demon->GetGUID();
    entry.kind = kind;
    entry.untilMs = now + durationMs;
    entry.nextMs = now + urand(300, 900);
    entry.power = power;
    state->demons.push_back(entry);

    switch (kind)
    {
        case DEMON_DREADSTALKER:
            ArmDemon(demon, SpellPower(player) * StalkerSwingPower);
            Engage(demon, target);
            break;
        case DEMON_GRIMOIRE_FELGUARD:
            ArmDemon(demon, SpellPower(player) * GrimoireSwingPower);
            Engage(demon, target);
            break;
        case DEMON_INFERNAL:
            ArmDemon(demon, SpellPower(player) * InfernalSwingPower);
            Engage(demon, target);
            break;
        default:
            demon->GetMotionMaster()->MoveFollow(player, frand(2.0f, 4.0f), frand(0.0f, 2.0f * float(M_PI)));
            break;
    }
    return demon;
}

void AddCores(Player* player, uint8 cores)
{
    if (cores && IsDemonology(player))
        ShowStacks(player, SPELL_DEMONIC_CORE,
            uint8(std::min<uint32>(Stacks(player, SPELL_DEMONIC_CORE) + cores, CoreMax)), -100);
}

void SummonWildImp(Player* player, WarlockState* state, Unit* target)
{
    uint32 const duration = WildImpMs + ImpHordeMsPerRank * Rank(player, TALENT_IMP_HORDE_1, TALENT_IMP_HORDE_2);
    SummonDemon(player, state, DEMON_WILD_IMP, player->GetNearPosition(2.0f, frand(0.0f, 2.0f * float(M_PI))),
        duration, target);
}

// The infernal: lands on a spot (striking and stunning the enemies there when `impact`), then fights
void SummonInfernal(Player* player, WarlockState* state, Position const& spot, uint32 durationMs, bool impact)
{
    if (impact)
        Burst(player, state, SPELL_INFERNAL_IMPACT, EnemiesAtSpot(player, spot, InfernalRadius));
    Unit* target = CurrentTarget(player, state);
    SummonDemon(player, state, DEMON_INFERNAL, spot, durationMs, target);
}

// --- Spell casts -----------------------------------------------------------------------------------------------------

Position SpellSpot(Spell* spell, Unit* target, Player* player)
{
    if (spell->m_targets.HasDst())
        return Position(*spell->m_targets.GetDstPos());
    return Position(target ? *target : *player);
}

class WarlockTalentSpellScript : public AllSpellScript
{
public:
    WarlockTalentSpellScript() : AllSpellScript("WarlockTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // Shard spenders need their shards, Conflagration a charge, Implosion and Power Siphon a Wild Imp, Demonic
    // Strength and Grimoire of Sacrifice a demon; Burning Rush cast again puts its flames out
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered())
            return;
        Player* player = Warlock(spell->GetCaster());
        if (!player)
            return;
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        uint32 const id = spellInfo->Id;
        WarlockState* state = GetState(player);

        if (uint8 const cost = ShardCost(player, id, FirstRank(spellInfo)))
            if (Shards(player) < cost)
            {
                result = SPELL_FAILED_CASTER_AURASTATE;
                return;
            }

        switch (id)
        {
            case SPELL_CONFLAGRATION:
                if (state->conflagration.initialized && !state->conflagration.count)
                    result = SPELL_FAILED_NOT_READY;
                break;
            case SPELL_IMPLOSION:
            case SPELL_POWER_SIPHON:
                if (!CountDemons(state, DEMON_WILD_IMP))
                    result = SPELL_FAILED_CASTER_AURASTATE;
                break;
            case SPELL_DEMONIC_STRENGTH:
            {
                Pet* pet = player->GetPet();
                if (!pet || !pet->IsAlive() || pet->GetEntry() != NPC_FELGUARD)
                    result = SPELL_FAILED_NO_PET;
                break;
            }
            case SPELL_GRIMOIRE_OF_SACRIFICE:
                if (!player->GetPet())
                    result = SPELL_FAILED_NO_PET;
                break;
            case SPELL_CHANNEL_DEMONFIRE:
            {
                bool burning = false;
                for (Unit* enemy : EnemiesNear(player, player, DemonfireRange))
                    if (HasImmolate(player, enemy))
                    {
                        burning = true;
                        break;
                    }
                if (!burning)
                    result = SPELL_FAILED_CASTER_AURASTATE;
                break;
            }
            case SPELL_BURNING_RUSH:
                if (player->HasAura(SPELL_BURNING_RUSH))
                {
                    player->RemoveAurasDueToSpell(SPELL_BURNING_RUSH);
                    result = SPELL_FAILED_DONT_REPORT;
                }
                break;
            default:
                break;
        }
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Warlock(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        WarlockState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);
        uint32 const now = Now();

        if (target && target != player && IsEnemy(player, target))
            state->lastTarget = target->GetGUID();

        // The shards a spender takes (Hand of Gul'dan up to 3, counted for its hit and its imps)
        uint8 guldanShards = 0;
        if (id == SPELL_HAND_OF_GULDAN)
        {
            guldanShards = std::min(Shards(player), uint8(GuldanMaxShards));
            SpendShards(player, state, guldanShards);
        }
        else if (uint8 const cost = ShardCost(player, id, firstRank))
            SpendShards(player, state, cost);

        // Havoc: Immolate cast at another target burns the marked one too
        if (firstRank == SPELL_IMMOLATE_R1 && target)
            if (Unit* marked = HavocTarget(player, state, target))
                player->CastSpell(marked, spellInfo->Id, true);

        switch (id)
        {
            // Class tree
            case SPELL_DARK_PACT:
            {
                int32 const sacrifice = int32(float(player->GetHealth()) * DarkPactSacrifice);
                if (sacrifice > 0 && player->GetHealth() > uint32(sacrifice))
                    player->ModifyHealth(-sacrifice);
                float const bonus = 1.0f + DesperatePactPerRank *
                    float(Rank(player, TALENT_DESPERATE_PACT_1, TALENT_DESPERATE_PACT_2));
                int32 const shield = Amount(float(sacrifice) * DarkPactShield * bonus);
                player->CastCustomSpell(player, SPELL_DARK_PACT_SHIELD, &shield, nullptr, nullptr, true);
                return;
            }
            case SPELL_GRIMOIRE_OF_SACRIFICE:
                if (Pet* pet = player->GetPet())
                    player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
                player->AddAura(SPELL_SACRIFICE_BUFF, player);
                return;
            // Affliction
            case SPELL_MALEFIC_RAPTURE:
                player->RemoveAurasDueToSpell(SPELL_CRESCENDO_BUFF);
                MaleficRapture(player, state);
                return;
            case SPELL_SOUL_ROT:
                AddShards(player, state, 1);
                if (target)
                {
                    uint8 spread = 0;
                    for (Unit* enemy : EnemiesNear(player, target, SpreadRange))
                    {
                        if (spread++ >= SoulRotSpread)
                            break;
                        player->CastSpell(enemy, SPELL_SOUL_ROT, true);
                    }
                }
                return;
            case SPELL_PHANTOM_SINGULARITY:
                if (target)
                {
                    state->phantomTarget = target->GetGUID();
                    state->phantomNextMs = now + PhantomPeriodMs / 2;
                }
                return;
            case SPELL_VILE_TAINT:
            {
                uint32 const agony = KnownRank(player, SPELL_CURSE_OF_AGONY_R1);
                for (Unit* enemy : EnemiesAtSpot(player, SpellSpot(spell, target, player), VileTaintRadius))
                {
                    if (agony)
                        player->CastSpell(enemy, agony, true);
                    player->CastSpell(enemy, SPELL_VILE_TAINT_DOT, true);
                }
                return;
            }
            case SPELL_SUMMON_DARKGLARE:
                SummonDarkglare(player, state);
                return;
            // Demonology
            case SPELL_HAND_OF_GULDAN:
                HandOfGuldan(player, state, target, guldanShards);
                return;
            case SPELL_DEMONBOLT:
                if (uint8 const cores = Stacks(player, SPELL_DEMONIC_CORE))
                    ShowStacks(player, SPELL_DEMONIC_CORE, cores - 1, -100);
                AddShards(player, state, 2);
                DemonicCalling(player);
                if (target && player->HasAura(TALENT_DOOM) && !target->HasAura(SPELL_DOOM_MARK, player->GetGUID()))
                    player->AddAura(SPELL_DOOM_MARK, target);
                return;
            case SPELL_CALL_DREADSTALKERS:
                player->RemoveAurasDueToSpell(SPELL_DEMONIC_CALLING_BUFF);
                CallDreadstalkers(player, state, target);
                return;
            case SPELL_IMPLOSION:
                Implosion(player, state, target);
                return;
            case SPELL_POWER_SIPHON:
                PowerSiphon(player, state);
                return;
            case SPELL_BILESCOURGE_BOMBERS:
                state->bombers.Start(SpellSpot(spell, target, player), now, BombersMs, BombersPeriodMs / 2);
                return;
            case SPELL_DEMONIC_STRENGTH:
                state->felstormUntilMs = now + FelstormMs;
                state->felstormNextMs = now;
                return;
            case SPELL_GRIMOIRE_FELGUARD:
                SummonDemon(player, state, DEMON_GRIMOIRE_FELGUARD, player->GetNearPosition(2.0f, frand(0.0f,
                    2.0f * float(M_PI))), GrimoireFelguardMs, target ? target : CurrentTarget(player, state));
                return;
            case SPELL_SUMMON_DEMONIC_TYRANT:
                SummonTyrant(player, state);
                return;
            // Destruction
            case SPELL_CONFLAGRATION:
                SpendConflagration(player, state);
                AddFragments(player, state, ConflagrationFragments + Rank(player, TALENT_EMBERS_1, TALENT_EMBERS_2));
                if (target && player->HasAura(TALENT_ROARING_BLAZE))
                    player->AddAura(SPELL_ROARING_BLAZE_MARK, target);
                ChaosConflagration(player);
                return;
            case SPELL_RAIN_OF_FIRE:
                state->rainOfFire.Start(SpellSpot(spell, target, player), now, RainOfFireMs, RainOfFirePeriodMs / 2);
                return;
            case SPELL_HAVOC:
                if (target)
                    state->havocTarget = target->GetGUID();
                return;
            case SPELL_CHANNEL_DEMONFIRE:
                state->demonfireNextMs = now;
                return;
            case SPELL_CATACLYSM:
            {
                std::vector<Unit*> const enemies = EnemiesAtSpot(player, SpellSpot(spell, target, player),
                    CataclysmRadius);
                Burst(player, state, SPELL_CATACLYSM_HIT, enemies);
                if (uint32 const immolate = KnownRank(player, SPELL_IMMOLATE_R1))
                    for (Unit* enemy : enemies)
                        if (enemy->IsAlive())
                            player->CastSpell(enemy, immolate, true);
                return;
            }
            case SPELL_SUMMON_INFERNAL:
                SummonInfernal(player, state, SpellSpot(spell, target, player), InfernalMs +
                    (player->HasAura(TALENT_LORD_OF_FLAMES) ? LordOfFlamesMs : 0), true);
                return;
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_SEED_OF_CORRUPTION_R1:
                // Semer les graines: another enemy near the target gets a seed of its own
                if (target && player->HasAura(TALENT_SOW_THE_SEEDS))
                    for (Unit* enemy : EnemiesNear(player, target, SpreadRange))
                        if (!enemy->GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_WARLOCK, 0, 0x10, 0, player->GetGUID()))
                        {
                            player->CastSpell(enemy, spellInfo->Id, true);
                            break;
                        }
                break;
            case SPELL_HAUNT_R1:
                if (player->HasAura(TALENT_HAUNTED_SOUL))
                    AddShards(player, state, 1);
                break;
            case SPELL_SHADOW_BOLT_R1:
                if (IsDemonology(player))
                {
                    AddShards(player, state, 1);
                    DemonicCalling(player);
                }
                break;
            case SPELL_SHADOWBURN_R1:
                if (target)
                {
                    state->shadowburnTarget = target->GetGUID();
                    state->shadowburnUntilMs = now + ShadowburnRefundMs;
                }
                ChaosConflagration(player);
                break;
            case SPELL_CHAOS_BOLT_R1:
                // Avatar de destruction
                if (player->HasAura(TALENT_AVATAR))
                    SummonInfernal(player, state, target ? target->GetNearPosition(3.0f, frand(0.0f,
                        2.0f * float(M_PI))) : player->GetNearPosition(3.0f, 0.0f), SummonedInfernalMs, false);
                break;
            default:
                break;
        }
    }

private:
    // Malefic Rapture: every enemy within 40 yd with the Warlock's damage over time effects, struck once for each
    // (ModifySpellDamageTaken counts them on the hit)
    static void MaleficRapture(Player* player, WarlockState* state)
    {
        std::vector<Unit*> afflicted;
        for (Unit* enemy : EnemiesNear(player, player, MaleficRange))
        {
            if (afflicted.size() >= AreaMaxTargets)
                break;
            if (DotCount(player, enemy))
                afflicted.push_back(enemy);
        }
        Burst(player, state, SPELL_MALEFIC_RAPTURE_HIT, afflicted);

        // Floraison funeste: the enemies near each one with Unstable Affliction
        if (!player->HasAura(TALENT_DOOM_BLOSSOM))
            return;
        for (Unit* enemy : afflicted)
            if (enemy->IsAlive() && HasUnstableAffliction(player, enemy))
                Burst(player, state, SPELL_DOOM_BLOSSOM_HIT, EnemiesAround(player, enemy, DoomBlossomRadius));
    }

    // Summon Darkglare: the Warlock's damage over time effects on the enemies near it lengthened, the eye called
    static void SummonDarkglare(Player* player, WarlockState* state)
    {
        int32 const extend = DarkglareExtendMs + (player->HasAura(TALENT_MALEVOLENT_GAZE) ? MalevolentGazeExtendMs : 0);
        for (Unit* enemy : EnemiesNear(player, player, DarkglareRange))
        {
            std::vector<Aura*> auras;
            for (auto const& [spellId, application] : enemy->GetAppliedAuras())
            {
                Aura* aura = application->GetBase();
                if (aura->GetCasterGUID() == player->GetGUID() && aura->GetDuration() > 0 &&
                    aura->GetSpellInfo()->HasAura(SPELL_AURA_PERIODIC_DAMAGE))
                    auras.push_back(aura);
            }
            for (Aura* aura : auras)
            {
                aura->SetMaxDuration(aura->GetMaxDuration() + extend);
                aura->SetDuration(aura->GetDuration() + extend);
            }
        }
        SummonDemon(player, state, DEMON_DARKGLARE, player->GetNearPosition(2.5f, float(M_PI) * 0.75f), DarkglareMs,
            nullptr);
    }

    // Hand of Gul'dan: the target and the enemies near it struck (stronger a shard), and a Wild Imp a shard
    static void HandOfGuldan(Player* player, WarlockState* state, Unit* target, uint8 shards)
    {
        if (!shards)
            return;
        if (target)
            Burst(player, state, SPELL_HAND_OF_GULDAN_HIT, EnemiesAround(player, target, GuldanRadius), float(shards));
        uint8 imps = shards;
        if (shards >= GuldanMaxShards && player->HasAura(TALENT_GULDAN_SUPREME))
            ++imps;
        for (uint8 index = 0; index < imps; ++index)
            SummonWildImp(player, state, target);
    }

    // Appel démoniaque: Shadow Bolt and Demonbolt may make the next Call Dreadstalkers instant and a shard cheaper
    static void DemonicCalling(Player* player)
    {
        if (uint8 const rank = Rank(player, TALENT_DEMONIC_CALLING_1, TALENT_DEMONIC_CALLING_2))
            if (roll_chance_i(DemonicCallingPerRank * rank))
                player->AddAura(SPELL_DEMONIC_CALLING_BUFF, player);
    }

    // Call Dreadstalkers: two hounds leap at the target and bite it (Fouet de l'effroi: the enemies near it too), a
    // Demonic Core each
    static void CallDreadstalkers(Player* player, WarlockState* state, Unit* target)
    {
        if (!target)
            target = CurrentTarget(player, state);
        for (uint8 index = 0; index < 2; ++index)
        {
            Position const at = target ? target->GetNearPosition(2.0f, frand(0.0f, 2.0f * float(M_PI))) :
                player->GetNearPosition(2.0f, frand(0.0f, 2.0f * float(M_PI)));
            Creature* stalker = SummonDemon(player, state, DEMON_DREADSTALKER, at, DreadstalkerMs, target);
            if (!stalker || !target)
                continue;
            std::vector<Unit*> const bitten = player->HasAura(TALENT_DREADLASH) ?
                EnemiesAround(player, target, DreadlashRadius) : std::vector<Unit*>{ target };
            int32 const amount = Amount(DemonHit(player, state, stalker, DreadbitePower, SPELL_SCHOOL_MASK_SHADOW) *
                AreaFalloff(bitten.size()));
            for (Unit* enemy : bitten)
                stalker->CastCustomSpell(enemy, SPELL_DREADBITE, &amount, nullptr, nullptr, true);
        }
        AddCores(player, 2);
    }

    // Implosion: every Wild Imp explodes on the target
    static void Implosion(Player* player, WarlockState* state, Unit* target)
    {
        if (!target)
            target = CurrentTarget(player, state);
        std::vector<Unit*> const enemies = target ? EnemiesAround(player, target, ImplosionRadius) :
            std::vector<Unit*>();
        for (auto itr = state->demons.begin(); itr != state->demons.end();)
        {
            if (itr->kind != DEMON_WILD_IMP)
            {
                ++itr;
                continue;
            }
            if (Creature* imp = FindDemon(player, *itr))
            {
                Burst(player, state, SPELL_IMPLOSION_HIT, enemies);
                if (roll_chance_i(ImpCoreChance))
                    AddCores(player, 1);
                Unsummon(imp);
            }
            itr = state->demons.erase(itr);
        }
    }

    // Power Siphon: the 2 Wild Imps nearest their end sacrificed for as many Demonic Cores
    static void PowerSiphon(Player* player, WarlockState* state)
    {
        std::sort(state->demons.begin(), state->demons.end(), [](Demon const& left, Demon const& right)
        {
            return left.untilMs < right.untilMs;
        });
        uint8 taken = 0;
        for (auto itr = state->demons.begin(); itr != state->demons.end() && taken < PowerSiphonImps;)
        {
            if (itr->kind != DEMON_WILD_IMP)
            {
                ++itr;
                continue;
            }
            if (Creature* imp = FindDemon(player, *itr))
                Unsummon(imp);
            itr = state->demons.erase(itr);
            ++taken;
        }
        AddCores(player, taken);
    }

    // Summon Demonic Tyrant: the other demons stay 15 s longer and empowered while it stands; it fires Demonfire,
    // stronger for each demon at the Warlock's side as it came (Règne de la tyrannie)
    static void SummonTyrant(Player* player, WarlockState* state)
    {
        uint32 const now = Now();
        uint8 const demons = DemonsAtSide(player, state);
        for (Demon& demon : state->demons)
            if (demon.kind == DEMON_WILD_IMP || demon.kind == DEMON_DREADSTALKER ||
                demon.kind == DEMON_GRIMOIRE_FELGUARD)
                demon.untilMs += TyrantExtendMs;
        state->tyrantUntilMs = now + TyrantMs;
        player->AddAura(SPELL_DEMONIC_POWER, player);
        float const power = 1.0f + ReignPerRank * float(Rank(player, TALENT_REIGN_1, TALENT_REIGN_2)) * float(demons);
        SummonDemon(player, state, DEMON_TYRANT, player->GetNearPosition(3.0f, float(M_PI)), TyrantMs, nullptr,
            power);
        if (player->HasAura(TALENT_SOULBOUND_TYRANT))
            AddShards(player, state, SoulboundTyrantShards);
    }

    // Conflagration du chaos: the next Conflagration or Shadowburn a critical strike (the one cast now took it)
    static void ChaosConflagration(Player* player)
    {
        player->RemoveAurasDueToSpell(SPELL_CHAOS_CONFLAGRATION_BUFF);
        if (uint8 const rank = Rank(player, TALENT_CHAOS_CONFLAGRATION_1, TALENT_CHAOS_CONFLAGRATION_2))
            if (roll_chance_i(ChaosConflagrationPerRank * rank))
                player->AddAura(SPELL_CHAOS_CONFLAGRATION_BUFF, player);
    }

public:
    // The enemy Havoc marked, alive and within reach, when it is not the spell's own target
    static Unit* HavocTarget(Player* player, WarlockState* state, Unit* castTarget)
    {
        if (state->havocTarget.IsEmpty() || (castTarget && castTarget->GetGUID() == state->havocTarget))
            return nullptr;
        Unit* marked = ObjectAccessor::GetUnit(*player, state->havocTarget);
        if (!IsEnemy(player, marked) || !marked->HasAura(SPELL_HAVOC, player->GetGUID()) ||
            !player->IsWithinDistInMap(marked, HavocRange))
            return nullptr;
        return marked;
    }
};

// --- Damage ----------------------------------------------------------------------------------------------------------

// The hits this module hands out at amounts it works out: the demons' and the relays
bool IsComputedHit(uint32 id)
{
    switch (id)
    {
        case SPELL_EYE_BEAM:
        case SPELL_FEL_FIREBOLT:
        case SPELL_DREADBITE:
        case SPELL_FELSTORM_HIT:
        case SPELL_TYRANT_DEMONFIRE:
        case SPELL_LEGION_STRIKE:
        case SPELL_INFERNAL_IMMOLATION:
        case SPELL_HAVOC_RELAY:
        case SPELL_INTERNAL_COMBUSTION_HIT:
            return true;
        default:
            return false;
    }
}

// The single-target Destruction spells whose damage Havoc copies (Immolate is cast at the marked enemy itself)
bool IsHavocSpell(uint32 id, uint32 firstRank)
{
    switch (firstRank)
    {
        case SPELL_SHADOW_BOLT_R1:
        case SPELL_SEARING_PAIN_R1:
        case SPELL_SOUL_FIRE_R1:
        case SPELL_SHADOWBURN_R1:
        case SPELL_INCINERATE_R1:
        case SPELL_CHAOS_BOLT_R1:
            return true;
        default:
            return id == SPELL_CONFLAGRATION;
    }
}

// The Warlock's own module hits and auras, which never bring Grimoire of Sacrifice's hit
bool IsModuleSpell(uint32 id)
{
    return id >= SPELL_SOUL_SHARDS && id <= SPELL_SPEC_DESTRUCTION;
}

class WarlockTalentUnitScript : public UnitScript
{
public:
    WarlockTalentUnitScript() : UnitScript("WarlockTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_ON_SPELL_DAMAGE_DONE,
        UNITHOOK_MODIFY_SPELL_CRIT_CHANCE,
        UNITHOOK_ON_AURA_REMOVE,
        UNITHOOK_ON_UNIT_DEATH
    }) { }

    // Ponction d'âme gathers a share of the damage the Warlock and its demons deal
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage || attacker == victim)
            return;
        Player* player = Warlock(attacker);
        if (!player)
            player = DemonOwner(attacker);
        if (!player || victim->IsFriendlyTo(player) || !player->HasAura(TALENT_SOUL_LEECH))
            return;
        GetState(player)->leechPool += float(damage) * SoulLeechShare;
    }

    // The demons' swings: their bonuses (Serviteur courroucé, the Tyrant, Gangregarde enragé)
    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        if (!damage)
            return;
        Player* player = DemonOwner(attacker);
        if (!player)
            return;
        damage = uint32(float(damage) * DemonBonus(player, GetState(player), attacker));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || damage <= 0)
            return;
        uint32 const id = spellInfo->Id;

        // The pet's own spells get the demons' bonuses; the hits the module works out carry them already
        if (Player* owner = DemonOwner(attacker))
        {
            if (attacker->IsPet() && !IsComputedHit(id))
                damage = int32(float(damage) * DemonBonus(owner, GetState(owner), attacker));
            return;
        }

        Player* player = Warlock(attacker);
        if (!player)
            return;
        WarlockState* state = GetState(player);
        uint32 const firstRank = FirstRank(spellInfo);
        float factor = 1.0f;
        switch (id)
        {
            case SPELL_MALEFIC_RAPTURE_HIT:
                factor *= float(std::max<uint8>(DotCount(player, target), 1)) * HitFactor(state, id);
                break;
            case SPELL_RAIN_OF_FIRE_HIT:
                factor *= HitFactor(state, id);
                if (player->HasAura(TALENT_INFERNO))
                    factor *= InfernoFactor;
                break;
            case SPELL_PHANTOM_SINGULARITY_HIT:
            case SPELL_DOOM_BLOSSOM_HIT:
            case SPELL_HAND_OF_GULDAN_HIT:
            case SPELL_IMPLOSION_HIT:
            case SPELL_BILESCOURGE_HIT:
            case SPELL_DOOM_HIT:
            case SPELL_CATACLYSM_HIT:
            case SPELL_INFERNAL_IMPACT:
                factor *= HitFactor(state, id);
                break;
            default:
                break;
        }

        // Âmes sacrifiées: Shadow Bolt and Demonbolt stronger for each demon at the Warlock's side
        if (firstRank == SPELL_SHADOW_BOLT_R1 || id == SPELL_DEMONBOLT)
            if (uint8 const rank = Rank(player, TALENT_SACRIFICED_SOULS_1, TALENT_SACRIFICED_SOULS_2))
                factor *= 1.0f + SacrificedSoulsPerRank * float(rank) * float(DemonsAtSide(player, state));

        if (!IsComputedHit(id))
            factor *= Eradication(player, target);
        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // Shards from Curse of Agony (Affliction) and Immolate (Destruction); Brasier rugissant, Contact de l'effroi,
    // Éradication, Récolte sinistre
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !damage || spellInfo->SpellFamilyName != SPELLFAMILY_WARLOCK)
            return;
        Player* player = Warlock(attacker);
        if (!player)
            return;
        WarlockState* state = GetState(player);
        float factor = 1.0f;

        if ((spellInfo->SpellFamilyFlags[0] & FlagAgony) && IsAffliction(player))
            AgonyTick(player, state, target);
        if (spellInfo->SpellFamilyFlags[0] & FlagImmolate)
        {
            if (IsDestruction(player))
                AddFragments(player, state, ImmolateFragments);
            if (target->HasAura(SPELL_ROARING_BLAZE_MARK, player->GetGUID()))
                factor *= RoaringBlazeFactor;
        }
        if (target->HasAura(SPELL_DREAD_TOUCH_MARK, player->GetGUID()))
            factor *= 1.0f + DreadTouchPerRank * float(Rank(player, TALENT_DREAD_TOUCH_1, TALENT_DREAD_TOUCH_2));
        factor *= Eradication(player, target);
        if (factor != 1.0f)
            damage = uint32(float(damage) * factor);

        if (uint8 const rank = Rank(player, TALENT_HARVEST_1, TALENT_HARVEST_2))
            if (roll_chance_i(HarvestChancePerRank * rank))
                player->CastSpell(target, SPELL_HARVEST_HIT, true);
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool critical) override
    {
        Player* player = Warlock(caster);
        if (!player || !victim || !spellInfo || !damage)
            return;
        WarlockState* state = GetState(player);
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        // Seed of Corruption's blast spreads Corruption
        if (spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellInfo->SpellFamilyFlags[1] & FlagSeedBlast) &&
            victim->IsAlive() &&
            !victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARLOCK, FlagCorruption, 0, 0,
                player->GetGUID()))
            if (uint32 const corruption = KnownRank(player, SPELL_CORRUPTION_R1))
                player->CastSpell(victim, corruption, true);

        switch (id)
        {
            case SPELL_MALEFIC_RAPTURE_HIT:
                if (victim->IsAlive() && Rank(player, TALENT_DREAD_TOUCH_1, TALENT_DREAD_TOUCH_2))
                    player->AddAura(SPELL_DREAD_TOUCH_MARK, victim);
                break;
            case SPELL_PHANTOM_SINGULARITY_HIT:
                player->ModifyHealth(int32(float(damage) * PhantomLeech));
                break;
            default:
                break;
        }

        if (firstRank == SPELL_INCINERATE_R1 && IsDestruction(player))
            AddFragments(player, state, IncinerateFragments + (critical ? 1 : 0));
        if (firstRank == SPELL_CHAOS_BOLT_R1)
        {
            if (Rank(player, TALENT_ERADICATION_1, TALENT_ERADICATION_2) && victim->IsAlive())
                player->AddAura(SPELL_ERADICATION_MARK, victim);
            if (player->HasAura(TALENT_INTERNAL_COMBUSTION))
                InternalCombustion(player, victim);
        }

        // Havoc: the single-target Destruction spells copied onto the marked enemy
        if (IsHavocSpell(id, firstRank))
            if (Unit* marked = WarlockTalentSpellScript::HavocTarget(player, state, victim))
            {
                int32 const amount = Amount(float(damage));
                player->CastCustomSpell(marked, SPELL_HAVOC_RELAY, &amount, nullptr, nullptr, true);
            }

        // Grimoire de sacrifice: the Warlock's own damaging spells may strike again with the demon's power
        uint32 const now = Now();
        if (!IsModuleSpell(id) && player->HasAura(SPELL_SACRIFICE_BUFF) && now >= state->sacrificeReadyMs &&
            victim->IsAlive() && roll_chance_i(SacrificeChance))
        {
            state->sacrificeReadyMs = now + SacrificeCooldownMs;
            player->CastSpell(victim, SPELL_SACRIFICE_HIT, true);
        }
    }

    // Chaos Bolt always strikes critically (Destruction); Shadowburn's chance grows on a target under 20%
    void ModifySpellCritChance(Unit const* caster, Unit const* victim, SpellInfo const* spellInfo,
                               float& critChance) override
    {
        if (!caster || !spellInfo || !caster->IsPlayer() || caster->getClass() != CLASS_WARLOCK)
            return;
        uint32 const firstRank = FirstRank(spellInfo);
        if (firstRank == SPELL_CHAOS_BOLT_R1 && IsDestruction(caster))
            critChance = 100.0f;
        else if (firstRank == SPELL_SHADOWBURN_R1 && victim && victim->HealthBelowPct(20))
            critChance += float(ShadowburnExecuteCrit);
    }

    // Trépas: the mark explodes as it runs out
    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        if (!unit || !aurApp || mode != AURA_REMOVE_BY_EXPIRE || aurApp->GetBase()->GetId() != SPELL_DOOM_MARK)
            return;
        Player* player = Warlock(aurApp->GetBase()->GetCaster());
        if (!player || !player->IsInMap(unit) || !unit->IsAlive())
            return;
        Burst(player, GetState(player), SPELL_DOOM_HIT, EnemiesAround(player, unit, DoomRadius));
    }

    // Shadowburn: its shard back when the target dies within 5 s
    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        if (!unit || !killer)
            return;
        Player* player = Warlock(killer);
        if (!player)
            player = DemonOwner(killer);
        if (!player)
            return;
        WarlockState* state = GetState(player);
        if (state->shadowburnTarget == unit->GetGUID() && Now() < state->shadowburnUntilMs)
        {
            state->shadowburnTarget.Clear();
            AddShards(player, state, 1);
        }
    }

private:
    // Éradication: the target Chaos Bolt marked takes more from the Warlock
    static float Eradication(Player* player, Unit* target)
    {
        if (!target->HasAura(SPELL_ERADICATION_MARK, player->GetGUID()))
            return 1.0f;
        return 1.0f + EradicationPerRank * float(Rank(player, TALENT_ERADICATION_1, TALENT_ERADICATION_2));
    }

    // A Curse of Agony tick's shard: its chance shared out among the curses ticking (/ sqrt of their count);
    // Crescendo tourmenté
    static void AgonyTick(Player* player, WarlockState* state, Unit* target)
    {
        uint32 const now = Now();
        std::erase_if(state->agonyTicks, [now, target](std::pair<ObjectGuid, uint32> const& tick)
        {
            return now - tick.second > 3000 || tick.first == target->GetGUID();
        });
        state->agonyTicks.emplace_back(target->GetGUID(), now);
        float const chance = float(AgonyShardChance + WritheChancePerRank *
            Rank(player, TALENT_WRITHE_1, TALENT_WRITHE_2)) / std::sqrt(float(state->agonyTicks.size()));
        if (!roll_chance_f(chance))
            return;
        AddShards(player, state, 1);
        if (player->HasAura(TALENT_TORMENTED_CRESCENDO) && roll_chance_i(CrescendoChance))
            player->AddAura(SPELL_CRESCENDO_BUFF, player);
    }

    // Combustion interne: Chaos Bolt burns up to 5 s of the target's Immolate at once
    static void InternalCombustion(Player* player, Unit* victim)
    {
        AuraEffect* immolate = victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARLOCK, FlagImmolate, 0,
            0, player->GetGUID());
        if (!immolate || immolate->GetAmplitude() <= 0)
            return;
        Aura* aura = immolate->GetBase();
        int32 const remaining = aura->GetDuration();
        int32 const consumed = std::min<int32>(remaining, InternalCombustionMs);
        int32 const ticks = consumed / immolate->GetAmplitude();
        if (ticks <= 0)
            return;
        int32 const amount = Amount(float(immolate->GetAmount()) * float(ticks));
        if (remaining - consumed <= 0)
            aura->Remove();
        else
            aura->SetDuration(remaining - consumed);
        player->CastCustomSpell(victim, SPELL_INTERNAL_COMBUSTION_HIT, &amount, nullptr, nullptr, true);
    }
};

// --- Every update: shards, charges, the demons, the effects on the ground --------------------------------------------

class WarlockTalentPlayerScript : public PlayerScript
{
public:
    WarlockTalentPlayerScript() : PlayerScript("WarlockTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_WARLOCK || !player->IsInWorld())
            return;
        WarlockState* state = GetState(player);
        uint32 const now = Now();

        UpdateConflagration(player, state, diff);
        UpdateDemons(player, state, now);
        UpdateRainOfFire(player, state, now);
        UpdateBombers(player, state, now);
        UpdatePhantom(player, state, now);
        UpdateFelstorm(player, state, now);
        UpdateDemonfire(player, state, now);

        state->holdTimer += diff;
        if (state->holdTimer < HoldCheckMs)
            return;
        uint32 const elapsed = state->holdTimer;
        state->holdTimer = 0;
        UpdateShards(player, state, elapsed);
        UpdateSoulLeech(player, state);
        UpdateBurningRush(player);
        UpdateInnerDemons(player, state, elapsed);
        UpdateHolds(player, state);
    }

private:
    // The demons: each acts on its own beat, and goes when its time is up (a Wild Imp may leave a Demonic Core)
    static void UpdateDemons(Player* player, WarlockState* state, uint32 now)
    {
        if (state->demons.empty())
            return;
        Unit* target = CurrentTarget(player, state);
        for (auto itr = state->demons.begin(); itr != state->demons.end();)
        {
            Creature* demon = FindDemon(player, *itr);
            if (!demon || !demon->IsAlive())
            {
                itr = state->demons.erase(itr);
                continue;
            }
            if (now >= itr->untilMs || !player->IsAlive() || !demon->IsInMap(player))
            {
                if (itr->kind == DEMON_WILD_IMP && roll_chance_i(ImpCoreChance))
                    AddCores(player, 1);
                Unsummon(demon);
                itr = state->demons.erase(itr);
                continue;
            }
            if (now >= itr->nextMs)
                Act(player, state, *itr, demon, target, now);
            ++itr;
        }
    }

    static void Act(Player* player, WarlockState* state, Demon& demon, Creature* creature, Unit* target, uint32 now)
    {
        switch (demon.kind)
        {
            case DEMON_WILD_IMP:
            {
                demon.nextMs = now + FireboltPeriodMs;
                if (!target || !creature->IsWithinDistInMap(target, DemonRange))
                    return;
                float const fire = 1.0f + FelFirePerRank * float(Rank(player, TALENT_FEL_FIRE_1, TALENT_FEL_FIRE_2));
                int32 const amount = Amount(DemonHit(player, state, creature, FireboltPower * fire,
                    SPELL_SCHOOL_MASK_FIRE));
                creature->SetFacingToObject(target);
                creature->CastCustomSpell(target, SPELL_FEL_FIREBOLT, &amount, nullptr, nullptr, true);
                return;
            }
            case DEMON_DARKGLARE:
            {
                demon.nextMs = now + 2000;
                if (!target || !creature->IsWithinDistInMap(target, DemonRange))
                    return;
                float const power = EyeBeamPower * (1.0f + EyeBeamPerDot * float(DotCount(player, target)));
                int32 const amount = Amount(DemonHit(player, state, creature, power, SPELL_SCHOOL_MASK_SHADOW));
                creature->SetFacingToObject(target);
                creature->CastCustomSpell(target, SPELL_EYE_BEAM, &amount, nullptr, nullptr, true);
                return;
            }
            case DEMON_TYRANT:
            {
                demon.nextMs = now + TyrantPeriodMs;
                if (!target || !creature->IsWithinDistInMap(target, DemonRange))
                    return;
                int32 const amount = Amount(DemonHit(player, state, creature, TyrantPower * demon.power,
                    SPELL_SCHOOL_MASK_FIRE));
                creature->SetFacingToObject(target);
                creature->CastCustomSpell(target, SPELL_TYRANT_DEMONFIRE, &amount, nullptr, nullptr, true);
                return;
            }
            case DEMON_DREADSTALKER:
                demon.nextMs = now + 1000;
                Engage(creature, target);
                return;
            case DEMON_GRIMOIRE_FELGUARD:
            {
                Engage(creature, target);
                Unit* victim = creature->GetVictim();
                if (!victim || !victim->IsAlive() || !creature->IsWithinMeleeRange(victim))
                {
                    demon.nextMs = now + 500;
                    return;
                }
                demon.nextMs = now + LegionStrikePeriodMs;
                int32 const amount = Amount(DemonHit(player, state, creature, LegionStrikePower,
                    SPELL_SCHOOL_MASK_SHADOW));
                creature->CastCustomSpell(victim, SPELL_LEGION_STRIKE, &amount, nullptr, nullptr, true);
                return;
            }
            case DEMON_INFERNAL:
            {
                demon.nextMs = now + InfernalPeriodMs;
                Engage(creature, target);
                std::vector<Unit*> const enemies = EnemiesAround(player, creature, InfernalRadius);
                if (enemies.empty())
                    return;
                float power = InfernalImmolationPower;
                if (player->HasAura(TALENT_LORD_OF_FLAMES))
                    power *= LordOfFlamesFactor;
                int32 const amount = Amount(DemonHit(player, state, creature, power, SPELL_SCHOOL_MASK_FIRE) *
                    AreaFalloff(enemies.size()));
                for (Unit* enemy : enemies)
                    creature->CastCustomSpell(enemy, SPELL_INFERNAL_IMMOLATION, &amount, nullptr, nullptr, true);
                if (IsDestruction(player))
                    AddFragments(player, state, InfernalFragments);
                return;
            }
            default:
                demon.nextMs = now + 1000;
                return;
        }
    }

    // Rain of Fire: every second, the enemies within 8 yd of its spot (Inferno: a tenth of a shard now and then;
    // Pluie de chaos: an infernal now and then)
    static void UpdateRainOfFire(Player* player, WarlockState* state, uint32 now)
    {
        GroundEffect& rain = state->rainOfFire;
        if (!rain.Active() || now < rain.nextMs)
            return;
        if (now > rain.untilMs + RainOfFirePeriodMs / 2 || !player->IsAlive())
        {
            rain.untilMs = 0;
            return;
        }
        rain.nextMs += RainOfFirePeriodMs;
        Burst(player, state, SPELL_RAIN_OF_FIRE_HIT, EnemiesAtSpot(player, rain.Spot(), RainOfFireRadius));
        if (player->HasAura(TALENT_INFERNO) && roll_chance_i(InfernoFragmentChance))
            AddFragments(player, state, 1);
        if (player->HasAura(TALENT_RAIN_OF_CHAOS) && roll_chance_i(RainOfChaosChance))
            SummonInfernal(player, state, rain.Spot(), SummonedInfernalMs, false);
    }

    // Bilescourge Bombers: every second, the enemies within 8 yd of their spot
    static void UpdateBombers(Player* player, WarlockState* state, uint32 now)
    {
        GroundEffect& bombers = state->bombers;
        if (!bombers.Active() || now < bombers.nextMs)
            return;
        if (now > bombers.untilMs + BombersPeriodMs / 2 || !player->IsAlive())
        {
            bombers.untilMs = 0;
            return;
        }
        bombers.nextMs += BombersPeriodMs;
        Burst(player, state, SPELL_BILESCOURGE_HIT, EnemiesAtSpot(player, bombers.Spot(), BombersRadius));
    }

    // Phantom Singularity: every 2 s while its mark lasts, the enemies within 8 yd of the marked one
    static void UpdatePhantom(Player* player, WarlockState* state, uint32 now)
    {
        if (state->phantomTarget.IsEmpty() || now < state->phantomNextMs)
            return;
        Unit* marked = ObjectAccessor::GetUnit(*player, state->phantomTarget);
        if (!IsEnemy(player, marked) || !marked->HasAura(SPELL_PHANTOM_SINGULARITY, player->GetGUID()) ||
            !player->IsAlive())
        {
            state->phantomTarget.Clear();
            return;
        }
        state->phantomNextMs = now + PhantomPeriodMs;
        Burst(player, state, SPELL_PHANTOM_SINGULARITY_HIT, EnemiesAround(player, marked, PhantomRadius));
    }

    // Demonic Strength: the Felguard whirls, striking the enemies within 8 yd of its target every second
    static void UpdateFelstorm(Player* player, WarlockState* state, uint32 now)
    {
        if (!state->felstormUntilMs || now < state->felstormNextMs)
            return;
        Pet* pet = player->GetPet();
        if (now > state->felstormUntilMs || !pet || !pet->IsAlive() || pet->GetEntry() != NPC_FELGUARD)
        {
            state->felstormUntilMs = 0;
            return;
        }
        state->felstormNextMs = now + FelstormPeriodMs;
        Unit* center = pet->GetVictim();
        if (!IsEnemy(player, center))
        {
            center = CurrentTarget(player, state);
            if (center && pet->IsAIEnabled && pet->AI())
                pet->AI()->AttackStart(center);
        }
        if (!center)
            return;
        std::vector<Unit*> const enemies = EnemiesAround(player, center, FelstormRadius);
        if (enemies.empty())
            return;
        int32 const amount = Amount(DemonHit(player, state, pet, FelstormPower, SPELL_SCHOOL_MASK_SHADOW) *
            AreaFalloff(enemies.size()));
        for (Unit* enemy : enemies)
            pet->CastCustomSpell(enemy, SPELL_FELSTORM_HIT, &amount, nullptr, nullptr, true);
    }

    // Channel Demonfire: a bolt every 0.25 s at a random enemy burning with the Warlock's Immolate
    static void UpdateDemonfire(Player* player, WarlockState* state, uint32 now)
    {
        if (now < state->demonfireNextMs)
            return;
        Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!channel || channel->GetSpellInfo()->Id != SPELL_CHANNEL_DEMONFIRE)
            return;
        state->demonfireNextMs = now + DemonfirePeriodMs;
        std::vector<Unit*> burning;
        for (Unit* enemy : EnemiesNear(player, player, DemonfireRange))
            if (HasImmolate(player, enemy))
                burning.push_back(enemy);
        if (!burning.empty())
            player->CastSpell(burning[urand(0, uint32(burning.size() - 1))], SPELL_DEMONFIRE_BOLT, true);
    }

    // Soul Shards: 3 to start with, back to 3 out of combat (never taken down)
    static void UpdateShards(Player* player, WarlockState* state, uint32 elapsed)
    {
        if (!state->shardsSeeded)
        {
            state->shardsSeeded = true;
            if (!player->HasAura(SPELL_SOUL_SHARDS))
                SetShards(player, state, ShardRest);
        }
        if (player->IsInCombat())
        {
            state->restMs = 0;
            return;
        }
        state->restMs += elapsed;
        if (state->restMs < ShardRestDelayMs || Shards(player) >= ShardRest)
            return;
        if (state->restMs >= ShardRestDelayMs + ShardRestStepMs)
        {
            state->restMs = ShardRestDelayMs;
            SetShards(player, state, Shards(player) + 1);
        }
    }

    // Ponction d'âme: the shield grows with what was gathered, up to its cap
    static void UpdateSoulLeech(Player* player, WarlockState* state)
    {
        if (state->leechPool < 1.0f)
            return;
        float const pool = state->leechPool;
        state->leechPool = 0.0f;
        if (!player->IsAlive() || !player->HasAura(TALENT_SOUL_LEECH))
            return;
        float cap = SoulLeechCap;
        switch (Rank(player, TALENT_DARK_ACCORD_1, TALENT_DARK_ACCORD_2))
        {
            case 2:
                cap = 0.20f;
                break;
            case 1:
                cap = 0.15f;
                break;
            default:
                break;
        }
        float current = 0.0f;
        if (AuraEffect* shield = player->GetAuraEffect(SPELL_SOUL_LEECH_SHIELD, EFFECT_0))
            current = float(shield->GetAmount());
        int32 const amount = Amount(std::min(current + pool, float(player->GetMaxHealth()) * cap));
        if (float(amount) <= current)
            return;
        player->CastCustomSpell(player, SPELL_SOUL_LEECH_SHIELD, &amount, nullptr, nullptr, true);
    }

    // Burning Rush: 4% of the health a second, out under 10%
    static void UpdateBurningRush(Player* player)
    {
        if (!player->HasAura(SPELL_BURNING_RUSH))
            return;
        if (!player->IsAlive() || player->HealthBelowPct(BurningRushFloorPct))
        {
            player->RemoveAurasDueToSpell(SPELL_BURNING_RUSH);
            return;
        }
        player->ModifyHealth(-int32(player->CountPctFromMaxHealth(BurningRushLossPct)));
    }

    // Démons intérieurs: a Wild Imp every 12 s in combat
    static void UpdateInnerDemons(Player* player, WarlockState* state, uint32 elapsed)
    {
        if (!player->IsInCombat() || !IsDemonology(player) || !player->HasAura(TALENT_INNER_DEMONS))
        {
            state->innerDemonsMs = 0;
            return;
        }
        state->innerDemonsMs += elapsed;
        if (state->innerDemonsMs < InnerDemonsMs)
            return;
        state->innerDemonsMs -= InnerDemonsMs;
        if (Unit* target = CurrentTarget(player, state))
            SummonWildImp(player, state, target);
    }

    // What belongs to a specialization goes with it; Grimoire of Sacrifice ends with a demon summoned
    static void UpdateHolds(Player* player, WarlockState* state)
    {
        if (!IsDemonology(player))
        {
            player->RemoveAurasDueToSpell(SPELL_DEMONIC_CORE);
            player->RemoveAurasDueToSpell(SPELL_DEMONIC_CALLING_BUFF);
        }
        if (!IsAffliction(player))
            player->RemoveAurasDueToSpell(SPELL_CRESCENDO_BUFF);
        if (player->HasAura(SPELL_SACRIFICE_BUFF) && player->GetPet())
            player->RemoveAurasDueToSpell(SPELL_SACRIFICE_BUFF);
        if (!player->IsAlive() && !state->demons.empty())
        {
            for (Demon const& demon : state->demons)
                if (Creature* creature = FindDemon(player, demon))
                    Unsummon(creature);
            state->demons.clear();
        }
    }
};
}

void AddWarlockTalentScripts()
{
    new WarlockTalentSpellScript();
    new WarlockTalentUnitScript();
    new WarlockTalentPlayerScript();
}
