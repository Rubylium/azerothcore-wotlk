/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 */

#ifndef HITCH_PROFILER_H
#define HITCH_PROFILER_H

#include "Define.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct HitchProfilerSnapshot
{
    bool enabled = false;
    bool tickActive = false;
    uint32 thresholdMs = 0;
    uint64 hitchCount = 0;
    uint32 lastHitchMs = 0;
    std::string currentSection;
};

class AC_GAME_API HitchProfiler
{
public:
    HitchProfiler();
    ~HitchProfiler();

    HitchProfiler(HitchProfiler const&) = delete;
    HitchProfiler& operator=(HitchProfiler const&) = delete;

    void LoadFromConfig();
    void BeginTick(uint32 diff, uint32 players);
    void EndTick();
    void EnterSection(char const* name, char const*& previousSection);
    void LeaveSection(char const* name, char const* previousSection, uint64 durationUs);
    void RecordMapUpdate(uint32 mapId, uint32 instanceId, uint64 durationUs);

    void SetEnabled(bool enabled);
    void SetThreshold(uint32 thresholdMs);
    bool IsEnabled() const { return _enabled.load(); }
    HitchProfilerSnapshot GetSnapshot() const;

private:
    struct SectionTiming
    {
        char const* name;
        uint64 durationUs;
    };

    struct MapTiming
    {
        uint32 mapId;
        uint32 instanceId;
        uint64 durationUs;
    };

    static uint64 NowNs();
    void EnsureWatchdogStarted();
    void WatchdogLoop();
    void UpdateProcessMetrics();
    void LogCompletedHitch(uint32 elapsedMs);

    std::atomic<bool> _enabled{ false };
    std::atomic<bool> _stopWatchdog{ false };
    std::atomic<bool> _tickActive{ false };
    std::atomic<bool> _watchdogReported{ false };
    std::atomic<uint32> _thresholdMs{ 200 };
    std::atomic<uint32> _watchdogMs{ 200 };
    std::atomic<uint32> _sectionThresholdMs{ 2 };
    std::atomic<uint32> _sampleIntervalMs{ 25 };
    std::atomic<uint32> _topSections{ 12 };
    std::atomic<uint32> _topMaps{ 8 };
    std::atomic<uint64> _tickStartNs{ 0 };
    std::atomic<uint64> _tickId{ 0 };
    std::atomic<uint64> _hitchCount{ 0 };
    std::atomic<uint32> _tickDiff{ 0 };
    std::atomic<uint32> _players{ 0 };
    std::atomic<uint32> _lastHitchMs{ 0 };
    std::atomic<uint32> _processMemoryMb{ 0 };
    std::atomic<uint32> _processCpuCorePct{ 0 };
    std::atomic<char const*> _currentSection{ "idle" };

    mutable std::mutex _dataMutex;
    std::vector<SectionTiming> _sections;
    std::vector<MapTiming> _maps;
    std::thread _watchdog;
    uint64 _lastCpuTime100ns = 0;
    uint64 _lastCpuWallNs = 0;
};

class AC_GAME_API HitchProfilerSectionScope
{
public:
    explicit HitchProfilerSectionScope(char const* name);
    ~HitchProfilerSectionScope();

private:
    char const* _name;
    char const* _previousSection = nullptr;
    std::chrono::steady_clock::time_point _start;
    bool _active;
};

#define HITCH_PROFILE_CONCAT_INNER(a, b) a##b
#define HITCH_PROFILE_CONCAT(a, b) HITCH_PROFILE_CONCAT_INNER(a, b)
#define HITCH_PROFILE_SCOPE(name) HitchProfilerSectionScope HITCH_PROFILE_CONCAT(hitchProfilerScope, __LINE__)(name)

AC_GAME_API extern HitchProfiler sHitchProfiler;

#endif
