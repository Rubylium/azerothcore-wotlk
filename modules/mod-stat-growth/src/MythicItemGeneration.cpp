#include "MythicItemGeneration.h"

#include "Log.h"
#include "MythicDungeon.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include <cmath>
#include <vector>

// Mythic+ loot above the game's best items (see MythicDungeon.h): every epic of the top tier is copied into each
// generated variant, its item level raised and its stats grown with it. Made at startup, before anyone logs in,
// so the item store never changes under the map threads and the items players keep always exist again after a
// restart. The client draws them with their base item's look through the awesome_wotlk client extension.
namespace
{
// Stats, weapon damage and block grow with the square of the item level ratio, about how WotLK's item budget grows
// between tiers; armor grows linearly with it, as it does between tiers
constexpr float StatGrowthExponent = 2.0f;
// Base items: the top tier, this many item levels below the best
constexpr uint32 BaseItemLevelSpan = 13;

bool IsBaseItem(ItemTemplate const& itemTemplate)
{
    return itemTemplate.Quality == ITEM_QUALITY_EPIC && !Mythic::IsGeneratedItem(itemTemplate.ItemId) &&
        (itemTemplate.Class == ITEM_CLASS_WEAPON || itemTemplate.Class == ITEM_CLASS_ARMOR) &&
        itemTemplate.InventoryType != INVTYPE_NON_EQUIP && itemTemplate.ItemLevel <= Mythic::MaxItemLevel &&
        itemTemplate.ItemLevel + BaseItemLevelSpan >= Mythic::MaxItemLevel && itemTemplate.RandomProperty == 0 &&
        itemTemplate.RandomSuffix == 0 && itemTemplate.Map == 0 && itemTemplate.Area == 0 &&
        !itemTemplate.HasFlag(ITEM_FLAG_DEPRECATED);
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

    float const growth = static_cast<float>(item.ItemLevel) / static_cast<float>(base.ItemLevel);
    float const statGrowth = std::pow(growth, StatGrowthExponent);

    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        item.ItemStat[index].ItemStatValue = Grow(item.ItemStat[index].ItemStatValue, statGrowth);
    for (uint32 index = 0; index < MAX_ITEM_PROTO_DAMAGES; ++index)
    {
        item.Damage[index].DamageMin *= statGrowth;
        item.Damage[index].DamageMax *= statGrowth;
    }

    item.Armor = static_cast<uint32>(Grow(static_cast<int32>(item.Armor), growth));
    item.Block = static_cast<uint32>(Grow(static_cast<int32>(item.Block), statGrowth));
    for (int32* resistance : { &item.HolyRes, &item.FireRes, &item.NatureRes, &item.FrostRes, &item.ShadowRes,
                               &item.ArcaneRes })
        *resistance = Grow(*resistance, growth);
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

        for (ItemTemplate const* base : bases)
            for (uint32 variant = 0; variant < Mythic::GeneratedItemVariants; ++variant)
                sObjectMgr->AddGeneratedItemTemplate(MakeVariant(*base, variant), base->ItemId);

        LOG_INFO("server.loading", ">> Generated {} Mythic+ items ({} bases, item level {} to {}) in {} ms",
            bases.size() * Mythic::GeneratedItemVariants, bases.size(), Mythic::GetGeneratedItemLevel(0),
            Mythic::GetGeneratedItemLevel(Mythic::GeneratedItemVariants - 1), GetMSTimeDiffToNow(startTime));
    }
};
}

void AddMythicItemGenerationScripts()
{
    new MythicItemGenerationWorldScript();
}
