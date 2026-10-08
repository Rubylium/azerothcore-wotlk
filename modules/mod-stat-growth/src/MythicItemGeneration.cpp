#include "MythicItemGeneration.h"

#include "Log.h"
#include "MythicDungeon.h"
#include "ObjectMgr.h"
#include "PowerScaling.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include <cmath>
#include <vector>

// Mythic+ loot above the game's best items (see MythicDungeon.h): every epic of the Northrend raids and dungeons from
// Ulduar on is copied into each generated variant, its item level raised and its stats grown with it. Made at startup,
// before anyone logs in, so the item store never changes under the map threads and the items players keep always exist
// again after a restart. The client draws them with their base item's look through the awesome_wotlk client extension.
namespace
{
// The first bases were the top tier alone (this many item levels below the best): kept, or the variants of them
// players own would be gone after a restart
constexpr uint32 LegacyBaseItemLevelSpan = 13;
// Bases from Ulduar's item level up: a variant's stats are grown to its own item level whatever its base's, so a lower
// base gives as much. The top tier alone left a slot a handful of items (four trinkets, one neck at 278 and above),
// and the same ones dropped over and over. Not lower: on-use and proc effects do not grow, and Naxxramas' trinkets are
// far behind at a high key.
constexpr uint32 MinimumBaseItemLevel = 226;

bool HasStat(ItemTemplate const& itemTemplate, uint32 stat)
{
    for (uint32 index = 0; index < itemTemplate.StatsCount && index < MAX_ITEM_PROTO_STATS; ++index)
        if (itemTemplate.ItemStat[index].ItemStatType == stat && itemTemplate.ItemStat[index].ItemStatValue)
            return true;
    return false;
}

bool IsLegacyBaseItem(ItemTemplate const& itemTemplate)
{
    return itemTemplate.ItemLevel + LegacyBaseItemLevelSpan >= Mythic::MaxItemLevel && itemTemplate.Map == 0 &&
        itemTemplate.Area == 0 && !itemTemplate.HasFlag(ITEM_FLAG_DEPRECATED);
}

// What the Mythic+ reward may give (SmartLootSystem's catalog takes no more): no reputation, profession or quest item,
// and no PvP gear - resilience, or a gladiator's relic whose effect only works in arenas
bool IsWiderBaseItem(ItemTemplate const& itemTemplate)
{
    return itemTemplate.ItemLevel >= MinimumBaseItemLevel && itemTemplate.Map == 0 && itemTemplate.Area == 0 &&
        itemTemplate.HolidayId == 0 && itemTemplate.StartQuest == 0 && itemTemplate.RequiredReputationFaction == 0 &&
        itemTemplate.RequiredSkill == 0 && itemTemplate.RequiredSpell == 0 && itemTemplate.RequiredHonorRank == 0 &&
        itemTemplate.Bonding != BIND_QUEST_ITEM && itemTemplate.Bonding != BIND_QUEST_ITEM1 &&
        !itemTemplate.HasFlag(ITEM_FLAG_DEPRECATED) && !itemTemplate.HasFlag(ITEM_FLAG_NO_PICKUP) &&
        !itemTemplate.HasFlag2(ITEM_FLAG2_INTERNAL_ITEM) && !HasStat(itemTemplate, ITEM_MOD_RESILIENCE_RATING) &&
        itemTemplate.Name1.find("Gladiator's") == std::string::npos;
}

bool IsBaseItem(ItemTemplate const& itemTemplate)
{
    if (itemTemplate.Quality != ITEM_QUALITY_EPIC || Mythic::IsGeneratedItem(itemTemplate.ItemId) ||
        (itemTemplate.Class != ITEM_CLASS_WEAPON && itemTemplate.Class != ITEM_CLASS_ARMOR) ||
        itemTemplate.InventoryType == INVTYPE_NON_EQUIP || itemTemplate.ItemLevel > Mythic::MaxItemLevel ||
        itemTemplate.RandomProperty != 0 || itemTemplate.RandomSuffix != 0)
        return false;
    return IsLegacyBaseItem(itemTemplate) || IsWiderBaseItem(itemTemplate);
}

int32 Grow(int32 value, float factor)
{
    return static_cast<int32>(std::lround(value * factor));
}

ItemTemplate MakeVariant(ItemTemplate const& base, uint32 variant)
{
    ItemTemplate item = base;
    item.ItemId = Mythic::GetGeneratedItemEntry(base.ItemId, variant);
    item.ItemLevel = Mythic::GetGeneratedItemLevel(variant);
    // Set bonuses count the set's own item entries
    item.ItemSet = 0;

    // The power model's growth (PowerScaling.h): primary stats, stamina, weapon damage and armour linearly with the
    // item level, ratings slower
    float const from = static_cast<float>(base.ItemLevel);
    float const to = static_cast<float>(item.ItemLevel);
    float const statGrowth = Power::StatGrowth(from, to);
    float const ratingGrowth = Power::StatGrowth(from, to, true);

    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        item.ItemStat[index].ItemStatValue = Grow(item.ItemStat[index].ItemStatValue,
            Power::IsRatingStat(item.ItemStat[index].ItemStatType) ? ratingGrowth : statGrowth);
    for (uint32 index = 0; index < MAX_ITEM_PROTO_DAMAGES; ++index)
    {
        item.Damage[index].DamageMin *= statGrowth;
        item.Damage[index].DamageMax *= statGrowth;
    }

    item.Armor = static_cast<uint32>(Grow(static_cast<int32>(item.Armor), statGrowth));
    item.Block = static_cast<uint32>(Grow(static_cast<int32>(item.Block), statGrowth));
    for (int32* resistance : { &item.HolyRes, &item.FireRes, &item.NatureRes, &item.FrostRes, &item.ShadowRes,
                               &item.ArcaneRes })
        *resistance = Grow(*resistance, statGrowth);
    return item;
}

class MythicItemGenerationWorldScript : public WorldScript
{
public:
    MythicItemGenerationWorldScript() : WorldScript("MythicItemGenerationWorldScript", {
        WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED
    }) { }

    void OnBeforeWorldInitialized() override
    {
        uint32 const startTime = getMSTime();

        // The store grows while generating: the bases are taken first
        std::vector<ItemTemplate const*> bases;
        for (auto const& [entry, itemTemplate] : *sObjectMgr->GetItemTemplateStore())
            if (IsBaseItem(itemTemplate))
                bases.push_back(&itemTemplate);

        // The ladder above the highest reward (Mythic::MaxPinnacleItemLevel) is only kept for the first bases, whose
        // variants up there players may still own: nothing gives one any more, and a wider base never had them
        uint32 const highestRewardVariant = Mythic::GetGeneratedVariant(Mythic::MaxPinnacleItemLevel);
        size_t generated = 0;
        for (ItemTemplate const* base : bases)
        {
            uint32 const variants = IsLegacyBaseItem(*base) ? Mythic::GeneratedItemVariants : highestRewardVariant + 1;
            for (uint32 variant = 0; variant < variants; ++variant)
                sObjectMgr->AddGeneratedItemTemplate(MakeVariant(*base, variant), base->ItemId);
            generated += variants + Mythic::CapVariants;
            for (uint32 variant = Mythic::FirstCapVariant;
                variant < Mythic::FirstCapVariant + Mythic::CapVariants; ++variant)
                sObjectMgr->AddGeneratedItemTemplate(MakeVariant(*base, variant), base->ItemId);
        }

        LOG_INFO("server.loading", ">> Generated {} Mythic+ items ({} bases, item level {} to {}) in {} ms",
            generated, bases.size(),
            Mythic::GetGeneratedItemLevel(0),
            Mythic::GetGeneratedItemLevel(Mythic::GeneratedItemVariants - 1), GetMSTimeDiffToNow(startTime));
    }
};
}

bool IsMythicBaseItem(ItemTemplate const& itemTemplate)
{
    return IsBaseItem(itemTemplate);
}

void AddMythicItemGenerationScripts()
{
    new MythicItemGenerationWorldScript();
}
