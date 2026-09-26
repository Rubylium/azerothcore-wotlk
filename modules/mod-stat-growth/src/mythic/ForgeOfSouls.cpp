#include "MythicDungeons.h"
#include "MythicTuning.h"

#include <initializer_list>

// The Forge of Souls in Mythic+: damage tuning, boss reworks and trash abilities
// (audit: .agents/plans/mplus-damage-audit/).
//
// Both bosses are already reworked on the indicators (BronjahmRework.cpp, DevourerRework.cpp); their damage in a key is
// set there, as shares of the reference health. What is left here: the trash's spikes, and a few trash abilities to
// step out of, so the packs are not all punching balls.
namespace
{
enum ForgeTrash : uint32
{
    // Normal entry, then the heroic one (a spawn keeps its normal entry in heroic: both are registered)
    NPC_SOULGUARD_WATCHMAN      = 36478,
    NPC_SOULGUARD_WATCHMAN_H    = 37569,
    NPC_SOULGUARD_REAPER        = 36499,
    NPC_SOULGUARD_REAPER_H      = 37568,
    NPC_SOULGUARD_ANIMATOR      = 36516,
    NPC_SOULGUARD_ANIMATOR_H    = 37567,
};

enum ForgeSpells : uint32
{
    // Spectral Warden: Wail of Souls, 40 yards around it every 5 seconds
    SPELL_WAIL_OF_SOULS         = 69148,
    SPELL_WAIL_OF_SOULS_H       = 70210,
    // Soulguard Bonecaster: Bone Volley, 30 yards around it
    SPELL_BONE_VOLLEY           = 69080,
    SPELL_BONE_VOLLEY_H         = 70206,

    // Only named in the log by the trash abilities below (none is a weapon spell)
    SPELL_SHADOW_CLEAVE         = 70670,
    SPELL_SHADOW_LANCE          = 69058,
    SPELL_SOUL_SIPHON           = 69128,
};

void RegisterFor(std::initializer_list<uint32> entries, MythicTrash::Ability ability)
{
    for (uint32 entry : entries)
    {
        ability.entry = entry;
        MythicTrash::Register(ability);
    }
}
}

void AddMythicForgeOfSoulsScripts()
{
    // Spectral Warden: the whole room took 23% every 5 seconds with nothing to do about it (4.7% a second of
    // unavoidable group damage, over the pulse budget): halved
    for (uint32 spell : { SPELL_WAIL_OF_SOULS, SPELL_WAIL_OF_SOULS_H })
        MythicTuning::SetSpellMultiplier(spell, 0.5f);

    // Soulguard Bonecaster: 19% on the whole group per volley, over the 8-15% pulse budget
    for (uint32 spell : { SPELL_BONE_VOLLEY, SPELL_BONE_VOLLEY_H })
        MythicTuning::SetSpellMultiplier(spell, 0.75f);

    // Soulguard Watchman: a shadow cleave at the tank, holding still for it (don't stand in front of it)
    MythicTrash::Ability cleave;
    cleave.shape = MythicTrash::Shape::ConeAtVictim;
    cleave.size = 10.0f;
    cleave.width = 90.0f;
    cleave.warnMs = 2500;
    cleave.cooldownMs = 20000;
    cleave.firstMs = 7000;
    cleave.percent = 25.0f;
    cleave.spellId = SPELL_SHADOW_CLEAVE;
    cleave.theme = GroundIndicators::Theme::Shadow;
    RegisterFor({ NPC_SOULGUARD_WATCHMAN, NPC_SOULGUARD_WATCHMAN_H }, cleave);

    // Soulguard Reaper: a Shadow Lance thrown through its target and whoever stands behind (step aside)
    MythicTrash::Ability lance;
    lance.shape = MythicTrash::Shape::LineAtVictim;
    lance.size = 20.0f;
    lance.width = 5.0f;
    lance.warnMs = 2500;
    lance.cooldownMs = 18000;
    lance.firstMs = 9000;
    lance.percent = 30.0f;
    lance.spellId = SPELL_SHADOW_LANCE;
    lance.theme = GroundIndicators::Theme::Shadow;
    RegisterFor({ NPC_SOULGUARD_REAPER, NPC_SOULGUARD_REAPER_H }, lance);

    // Soulguard Animator: a Soul Siphon carried by one player that tears at everyone next to them (take it away)
    MythicTrash::Ability siphon;
    siphon.shape = MythicTrash::Shape::CarriedByTarget;
    siphon.size = 7.0f;
    siphon.warnMs = 3000;
    siphon.cooldownMs = 22000;
    siphon.firstMs = 10000;
    siphon.percent = 25.0f;
    siphon.spellId = SPELL_SOUL_SIPHON;
    siphon.theme = GroundIndicators::Theme::Shadow;
    siphon.holdStill = false;
    RegisterFor({ NPC_SOULGUARD_ANIMATOR, NPC_SOULGUARD_ANIMATOR_H }, siphon);
}
