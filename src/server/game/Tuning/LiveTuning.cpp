/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "LiveTuning.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Log.h"
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace
{
// Knobs register while the program's statics are built, in no set order: the registry is made on first use
struct Registry
{
    std::map<std::string_view, LiveTuning::KnobBase*> knobs;
    std::vector<std::string_view> duplicates;   // reported at Load, once logging is up
};

Registry& GetRegistry()
{
    static Registry registry;
    return registry;
}

// Spell multipliers: read on every damage and heal, from every map thread; none set (the usual case) costs one load
std::shared_mutex SpellLock;
std::unordered_map<uint32, float> Spells;
std::atomic<bool> AnySpell = false;
}

namespace LiveTuning
{
KnobBase::KnobBase(char const* key, double defaultValue) : _key(key), _default(defaultValue)
{
    Registry& registry = GetRegistry();
    if (!registry.knobs.emplace(_key, this).second)
        registry.duplicates.emplace_back(_key);
}

std::vector<KnobBase*> Knobs()
{
    std::vector<KnobBase*> knobs;
    for (auto const& [key, knob] : GetRegistry().knobs)
        knobs.push_back(knob);
    return knobs;
}

KnobBase* FindKnob(std::string_view key)
{
    auto const& knobs = GetRegistry().knobs;
    auto const itr = knobs.find(key);
    return itr != knobs.end() ? itr->second : nullptr;
}

bool SetOverride(std::string_view key, double value)
{
    KnobBase* knob = FindKnob(key);
    if (!knob)
        return false;

    knob->Apply(value);
    if (!knob->IsOverridden())
        return ResetOverride(key);

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_LIVE_TUNING);
    stmt->SetData(0, std::string(knob->Key()));
    stmt->SetData(1, float(knob->Value()));
    WorldDatabase.Execute(stmt);
    return true;
}

bool ResetOverride(std::string_view key)
{
    KnobBase* knob = FindKnob(key);
    if (!knob)
        return false;

    knob->Apply(knob->Default());
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_LIVE_TUNING);
    stmt->SetData(0, std::string(knob->Key()));
    WorldDatabase.Execute(stmt);
    return true;
}

float SpellMultiplier(uint32 spellId)
{
    if (!AnySpell.load(std::memory_order_relaxed))
        return 1.0f;
    std::shared_lock lock(SpellLock);
    auto const itr = Spells.find(spellId);
    return itr != Spells.end() ? itr->second : 1.0f;
}

void SetSpellMultiplier(uint32 spellId, float multiplier)
{
    bool const drop = multiplier == 1.0f;
    {
        std::unique_lock lock(SpellLock);
        if (drop)
            Spells.erase(spellId);
        else
            Spells[spellId] = multiplier;
        AnySpell.store(!Spells.empty(), std::memory_order_relaxed);
    }

    if (drop)
    {
        WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_LIVE_TUNING_SPELL);
        stmt->SetData(0, spellId);
        WorldDatabase.Execute(stmt);
        return;
    }
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_LIVE_TUNING_SPELL);
    stmt->SetData(0, spellId);
    stmt->SetData(1, multiplier);
    WorldDatabase.Execute(stmt);
}

std::vector<std::pair<uint32, float>> SpellMultipliers()
{
    std::shared_lock lock(SpellLock);
    std::map<uint32, float> const sorted(Spells.begin(), Spells.end());
    return { sorted.begin(), sorted.end() };
}

std::pair<uint32, uint32> Load()
{
    for (std::string_view const key : GetRegistry().duplicates)
        LOG_ERROR("server.loading", "Live tuning: the knob {} is declared twice, only its first declaration is tuned",
                  key);

    // Back to the code's values, then the overrides on top: a reload forgets one removed from the table
    for (KnobBase* knob : Knobs())
        knob->Apply(knob->Default());

    uint32 knobs = 0;
    if (PreparedQueryResult result = WorldDatabase.Query(WorldDatabase.GetPreparedStatement(WORLD_SEL_LIVE_TUNING)))
    {
        do
        {
            Field* fields = result->Fetch();
            std::string const key = fields[0].Get<std::string>();
            if (KnobBase* knob = FindKnob(key))
            {
                knob->Apply(fields[1].Get<double>());
                ++knobs;
            }
            else
                LOG_WARN("server.loading", "Live tuning: an override of {}, which no code declares (renamed or "
                         "removed?), is ignored", key);
        } while (result->NextRow());
    }

    std::unordered_map<uint32, float> spells;
    if (PreparedQueryResult result =
        WorldDatabase.Query(WorldDatabase.GetPreparedStatement(WORLD_SEL_LIVE_TUNING_SPELL)))
    {
        do
        {
            Field* fields = result->Fetch();
            spells[fields[0].Get<uint32>()] = fields[1].Get<float>();
        } while (result->NextRow());
    }

    uint32 const spellCount = uint32(spells.size());
    {
        std::unique_lock lock(SpellLock);
        Spells = std::move(spells);
        AnySpell.store(!Spells.empty(), std::memory_order_relaxed);
    }
    return { knobs, spellCount };
}
}
