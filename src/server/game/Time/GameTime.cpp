/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "GameTime.h"
#include "Config.h"
#include "Timer.h"
#include <algorithm>
#include <atomic>

namespace GameTime
{
    using namespace std::chrono;

    Seconds const StartTime = GetEpochTime();

    Seconds GameTime = GetEpochTime();
    Milliseconds GameMSTime = 0ms;

    SystemTimePoint GameTimeSystemPoint = SystemTimePoint::min();
    TimePoint GameTimeSteadyPoint = TimePoint::min();

    Seconds GetStartTime()
    {
        return StartTime;
    }

    Seconds GetGameTime()
    {
        return GameTime;
    }

    Milliseconds GetGameTimeMS()
    {
        return GameMSTime;
    }

    SystemTimePoint GetSystemTime()
    {
        return GameTimeSystemPoint;
    }

    TimePoint Now()
    {
        return GameTimeSteadyPoint;
    }

    Seconds GetUptime()
    {
        return GameTime - StartTime;
    }

    void UpdateGameTimers()
    {
        // The simulation bench's warp (Timer.h, 0 on a live server) moves every one of them alike
        Milliseconds const warp(Acore::Time::GetWarpMS());
        GameTimeSystemPoint = system_clock::now() + warp;
        GameTime = duration_cast<Seconds>(GameTimeSystemPoint.time_since_epoch());
        GameMSTime = GetTimeMS();
        GameTimeSteadyPoint = steady_clock::now() + warp;
    }

    bool Simulation = false;
    std::atomic<uint32> SimulationStepMs{ 10 };
    std::atomic<bool> SimulationTurbo{ false };

    void LoadSimulationSettings()
    {
        Simulation = sConfigMgr->GetOption<bool>("Sim.Enable", false);
        // Creatures update on the maps' full updates only, one in about four world updates: a step much past 10 ms
        // makes their timing coarser than a live server's
        SimulationStepMs = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Sim.StepMs", 10));
    }

    bool IsSimulation()
    {
        return Simulation;
    }

    uint32 GetSimulationStepMs()
    {
        return SimulationStepMs;
    }

    void SetSimulationStepMs(uint32 stepMs)
    {
        SimulationStepMs = std::max<uint32>(1, stepMs);
    }

    void SetSimulationTurbo(bool turbo)
    {
        SimulationTurbo.store(turbo, std::memory_order_relaxed);
    }

    bool IsSimulationTurbo()
    {
        return Simulation && SimulationTurbo.load(std::memory_order_relaxed);
    }
}
