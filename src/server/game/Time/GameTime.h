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

#ifndef __GAMETIME_H
#define __GAMETIME_H

#include "Define.h"
#include "Duration.h"

namespace GameTime
{
    // Server start time
    AC_GAME_API Seconds GetStartTime();

    // Current server time (unix)
    AC_GAME_API Seconds GetGameTime();

    // Milliseconds since server start
    AC_GAME_API Milliseconds GetGameTimeMS();

    /// Current chrono system_clock time point
    AC_GAME_API SystemTimePoint GetSystemTime();

    /// Current chrono steady_clock time point
    AC_GAME_API TimePoint Now();

    /// Uptime
    AC_GAME_API Seconds GetUptime();

    /// Uptime since a given time point
    inline Microseconds Elapsed(TimePoint start)
    {
        return std::chrono::duration_cast<Microseconds>(Now() - start);
    }

    /// Check if a duration has elapsed since a given time point
    template<class T>
    inline bool HasElapsed(TimePoint start, T duration)
    {
        return (Now() - start) >= duration;
    }

    /// Update all timers
    void UpdateGameTimers();

    /// The simulation bench (Sim.Enable, a world server that is no live realm): in turbo the world loop no longer waits
    /// for the real clock, each update moving every game clock by Sim.StepMs (Timer.h's warp). Off on a live server.
    AC_GAME_API void LoadSimulationSettings();
    AC_GAME_API bool IsSimulation();
    AC_GAME_API uint32 GetSimulationStepMs();
    AC_GAME_API void SetSimulationStepMs(uint32 stepMs);

    /// Turbo, asked by the simulation's job while its fights run (bots logged in, nothing waited from the database);
    /// IsSimulationTurbo is false whenever the simulation is off
    AC_GAME_API void SetSimulationTurbo(bool turbo);
    AC_GAME_API bool IsSimulationTurbo();
}

#endif
