#include "ForgeVisuals.h"

#include "Player.h"
#include "Random.h"
#include "SpellAuras.h"

#include <array>

namespace ForgeVisuals
{
namespace
{
constexpr uint32 EmberTrailSpell = 92400;
constexpr uint32 TrailDurationMs = 1200;
constexpr uint32 MinimumArmourRanks = 24;
constexpr std::array<uint32, 4> LegacyBodyAuras = { 92401, 92402, 92404, 92405 };

uint32 NextTrailDelay(uint32 ranks)
{
    // More forged armour slightly increases frequency, never the size or number of attached effects.
    if (ranks >= 104)
        return urand(20000, 30000);
    if (ranks >= 72)
        return urand(25000, 35000);
    return urand(30000, 45000);
}

void ClearLegacyBodyAuras(Player* player)
{
    for (uint32 spell : LegacyBodyAuras)
        if (player->HasAura(spell))
            player->RemoveAurasDueToSpell(spell);
}

bool CanShowTrail(Player const* player)
{
    return player->IsAlive() && player->isMoving() && !player->IsInCombat() && !player->IsMounted() &&
        !player->IsInFlight() && !player->IsFlying() && !player->IsInWater() &&
        !player->HasStealthAura() && !player->HasInvisibilityAura();
}
}

void Reset(Player* player, State& state)
{
    ClearLegacyBodyAuras(player);
    player->RemoveAurasDueToSpell(EmberTrailSpell);
    state = {};
}

void SetArmourRanks(Player* player, State& state, uint32 ranks)
{
    ClearLegacyBodyAuras(player);
    if (state.armourRanks == ranks)
        return;

    state.armourRanks = ranks;
    state.cooldownMs = NextTrailDelay(ranks);
    player->RemoveAurasDueToSpell(EmberTrailSpell);
}

void UpdateTrail(Player* player, State& state, uint32 diff)
{
    state.cooldownMs = state.cooldownMs > diff ? state.cooldownMs - diff : 0;
    if (state.armourRanks < MinimumArmourRanks || !CanShowTrail(player))
    {
        if (player->HasAura(EmberTrailSpell))
            player->RemoveAurasDueToSpell(EmberTrailSpell);
        return;
    }

    if (state.cooldownMs)
        return;

    state.cooldownMs = NextTrailDelay(state.armourRanks);
    // Reuse the quiet feet-only embers already shipped in the client. No casts, lightning, shields,
    // hand/head attachments or permanent body aura; the trail expires even if movement packets stop.
    if (Aura* trail = player->AddAura(EmberTrailSpell, player))
    {
        trail->SetMaxDuration(TrailDurationMs);
        trail->SetDuration(TrailDurationMs);
    }
}
}
