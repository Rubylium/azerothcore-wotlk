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

#ifndef ACORE_LIVE_TUNING_H
#define ACORE_LIVE_TUNING_H

#include "Define.h"
#include <atomic>
#include <string_view>
#include <utility>
#include <vector>

// Live tuning: the numbers a class or a fight is balanced with, changed while the server runs - no build, no restart
// (.tune, src/server/scripts/Commands/cs_tune.cpp). Made for the combat bench's loop: run, read, change, run again
// (.agents/docs/systems/combat-bench.md, "Live tuning").
//
//  - A knob is a tuning number of the code's, declared where its constant was:
//        LiveTuning::Knob const MortalStrikeBonus("warrior.mortal_strike_bonus", 1.20f);
//    It reads as the number wherever the constant did (a float or an integer). Its default is the code's value.
//    `.tune set <key> <value>` overrides it at once, for everyone; the override is kept in the world database
//    (`live_tuning`) across restarts until `.tune reset` or until it is baked into the code
//    (localTools/tuning/bakeTuning.py writes the value as the knob's default and drops the override).
//  - A spell multiplier scales one spell's damage and healing - direct, over time, a pet's - whatever its numbers come
//    from (spell data, coefficients, scripts): `.tune spell <id> <multiplier>`, kept in `live_tuning_spell`. Spell
//    data is shipped to the client too, so a multiplier is the quick way to try a value; once it is right, put it in
//    the spell's data or its script, and reset it.
namespace LiveTuning
{
    class KnobBase
    {
    public:
        KnobBase(char const* key, double defaultValue);
        KnobBase(KnobBase const&) = delete;
        KnobBase& operator=(KnobBase const&) = delete;
        virtual ~KnobBase() = default;

        [[nodiscard]] char const* Key() const { return _key; }
        [[nodiscard]] double Default() const { return _default; }
        [[nodiscard]] virtual double Value() const = 0;
        [[nodiscard]] bool IsOverridden() const { return Value() != _default; }

        // The value used from now on (not saved: SetOverride saves it)
        virtual void Apply(double value) = 0;

    private:
        char const* _key;
        double _default;
    };

    template<typename T>
    class BasicKnob final : public KnobBase
    {
    public:
        BasicKnob(char const* key, T defaultValue) : KnobBase(key, double(defaultValue)), _value(defaultValue) { }

        // Read on every map thread; written by the world thread (.tune)
        operator T() const { return _value.load(std::memory_order_relaxed); }
        [[nodiscard]] T Get() const { return _value.load(std::memory_order_relaxed); }

        [[nodiscard]] double Value() const override { return double(Get()); }
        void Apply(double value) override { _value.store(T(value), std::memory_order_relaxed); }

    private:
        std::atomic<T> _value;
    };

    using Knob = BasicKnob<float>;
    using KnobInt = BasicKnob<int32>;
    using KnobUInt = BasicKnob<uint32>;

    // Every knob declared, in key order
    std::vector<KnobBase*> Knobs();
    KnobBase* FindKnob(std::string_view key);

    // Overrides a knob and saves it; a value equal to its default drops the override
    bool SetOverride(std::string_view key, double value);
    // Back to its default, the override dropped
    bool ResetOverride(std::string_view key);

    // A spell's damage and healing multiplier: 1 unless set
    float SpellMultiplier(uint32 spellId);
    // Sets and saves it; 1 drops it
    void SetSpellMultiplier(uint32 spellId, float multiplier);
    std::vector<std::pair<uint32, float>> SpellMultipliers();

    // Every override from the world database (at startup, .tune reload). Returns knobs, spells loaded.
    std::pair<uint32, uint32> Load();
}

#endif
