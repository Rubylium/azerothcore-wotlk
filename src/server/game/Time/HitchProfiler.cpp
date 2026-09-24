/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 */

#include "HitchProfiler.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

HitchProfiler sHitchProfiler;

namespace
{
#ifdef _WIN32
    uint64 FileTimeToUint64(FILETIME const& value)
    {
        return (uint64(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    }
#endif

    std::string FormatDuration(uint64 durationUs)
    {
        if (durationUs >= 1000)
            return std::to_string(durationUs / 1000) + "." + std::to_string((durationUs % 1000) / 100) + "ms";
        return std::to_string(durationUs) + "us";
    }
}

HitchProfiler::HitchProfiler()
{
    _sections.reserve(32);
    _maps.reserve(32);
}

HitchProfiler::~HitchProfiler()
{
    _stopWatchdog = true;
    if (_watchdog.joinable())
        _watchdog.join();
}

uint64 HitchProfiler::NowNs()
{
    return uint64(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void HitchProfiler::LoadFromConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("HitchProfiler.Enabled", true);
    _thresholdMs = std::max<uint32>(50, sConfigMgr->GetOption<uint32>("HitchProfiler.ThresholdMs", 200));
    _watchdogMs = std::max<uint32>(50, sConfigMgr->GetOption<uint32>("HitchProfiler.WatchdogMs", _thresholdMs.load()));
    _sectionThresholdMs = sConfigMgr->GetOption<uint32>("HitchProfiler.SectionThresholdMs", 2);
    _sampleIntervalMs = std::max<uint32>(10, sConfigMgr->GetOption<uint32>("HitchProfiler.SampleIntervalMs", 25));
    _topSections = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("HitchProfiler.TopSections", 12));
    _topMaps = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("HitchProfiler.TopMaps", 8));
    EnsureWatchdogStarted();

    LOG_INFO("server.hitch", "Hitch profiler {} (threshold={}ms, watchdog={}ms, section floor={}ms)",
        _enabled ? "enabled" : "disabled", _thresholdMs.load(), _watchdogMs.load(), _sectionThresholdMs.load());
}

void HitchProfiler::EnsureWatchdogStarted()
{
    if (!_watchdog.joinable())
        _watchdog = std::thread(&HitchProfiler::WatchdogLoop, this);
}

void HitchProfiler::BeginTick(uint32 diff, uint32 players)
{
    if (!_enabled)
        return;

    {
        std::lock_guard<std::mutex> lock(_dataMutex);
        _sections.clear();
        _maps.clear();
    }

    ++_tickId;
    _tickDiff = diff;
    _players = players;
    _currentSection = "world update setup";
    _watchdogReported = false;
    _tickStartNs = NowNs();
    _tickActive = true;
}

void HitchProfiler::EndTick()
{
    if (!_tickActive)
        return;

    if (!_enabled)
    {
        _tickActive = false;
        _currentSection = "idle";
        return;
    }

    uint64 const startNs = _tickStartNs.load();
    uint32 const elapsedMs = uint32((NowNs() - startNs) / 1000000);
    _tickActive = false;
    _currentSection = "idle";

    if (elapsedMs >= _thresholdMs)
        LogCompletedHitch(elapsedMs);
}

void HitchProfiler::EnterSection(char const* name, char const*& previousSection)
{
    previousSection = _currentSection.exchange(name);
}

void HitchProfiler::LeaveSection(char const* name, char const* previousSection, uint64 durationUs)
{
    _currentSection = previousSection ? previousSection : "world update";
    if (durationUs < uint64(_sectionThresholdMs.load()) * 1000)
        return;

    std::lock_guard<std::mutex> lock(_dataMutex);
    _sections.push_back({ name, durationUs });
}

void HitchProfiler::RecordMapUpdate(uint32 mapId, uint32 instanceId, uint64 durationUs)
{
    if (!_enabled || durationUs < uint64(_sectionThresholdMs.load()) * 1000)
        return;

    std::lock_guard<std::mutex> lock(_dataMutex);
    _maps.push_back({ mapId, instanceId, durationUs });
}

void HitchProfiler::SetEnabled(bool enabled)
{
    _enabled = enabled;
    if (!enabled)
    {
        _tickActive = false;
        _currentSection = "idle";
    }
    LOG_INFO("server.hitch", "Hitch profiler {} by command", enabled ? "enabled" : "disabled");
}

void HitchProfiler::SetThreshold(uint32 thresholdMs)
{
    _thresholdMs = std::max<uint32>(50, thresholdMs);
    _watchdogMs = _thresholdMs.load();
    LOG_INFO("server.hitch", "Hitch profiler threshold changed to {}ms", _thresholdMs.load());
}

HitchProfilerSnapshot HitchProfiler::GetSnapshot() const
{
    HitchProfilerSnapshot snapshot;
    snapshot.enabled = _enabled;
    snapshot.tickActive = _tickActive;
    snapshot.thresholdMs = _thresholdMs;
    snapshot.hitchCount = _hitchCount;
    snapshot.lastHitchMs = _lastHitchMs;
    snapshot.currentSection = _currentSection.load();
    return snapshot;
}

void HitchProfiler::WatchdogLoop()
{
    while (!_stopWatchdog)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(_sampleIntervalMs.load()));
        UpdateProcessMetrics();

        if (!_enabled || !_tickActive || _watchdogReported)
            continue;

        uint64 const startNs = _tickStartNs.load();
        uint32 const elapsedMs = uint32((NowNs() - startNs) / 1000000);
        if (elapsedMs < _watchdogMs)
            continue;

        _watchdogReported = true;
        LOG_WARN("server.hitch",
            "HITCH_IN_PROGRESS tick={} elapsed={}ms section=\"{}\" inputDiff={}ms players={} cpuCore={}pct rss={}MB",
            _tickId.load(), elapsedMs, _currentSection.load(), _tickDiff.load(), _players.load(),
            _processCpuCorePct.load(), _processMemoryMb.load());
    }
}

void HitchProfiler::UpdateProcessMetrics()
{
#ifdef _WIN32
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
    {
        uint64 const cpuTime = FileTimeToUint64(kernelTime) + FileTimeToUint64(userTime);
        uint64 const wallNs = NowNs();
        if (_lastCpuWallNs && wallNs > _lastCpuWallNs)
        {
            uint64 const cpuDelta100ns = cpuTime - _lastCpuTime100ns;
            uint64 const wallDelta100ns = (wallNs - _lastCpuWallNs) / 100;
            if (wallDelta100ns)
                _processCpuCorePct = uint32(std::min<uint64>(9999, cpuDelta100ns * 100 / wallDelta100ns));
        }
        _lastCpuTime100ns = cpuTime;
        _lastCpuWallNs = wallNs;
    }

    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        _processMemoryMb = uint32(counters.WorkingSetSize / (1024 * 1024));
#endif
}

void HitchProfiler::LogCompletedHitch(uint32 elapsedMs)
{
    std::vector<SectionTiming> sections;
    std::vector<MapTiming> maps;
    {
        std::lock_guard<std::mutex> lock(_dataMutex);
        sections = _sections;
        maps = _maps;
    }

    std::sort(sections.begin(), sections.end(), [](SectionTiming const& left, SectionTiming const& right)
    {
        return left.durationUs > right.durationUs;
    });
    std::sort(maps.begin(), maps.end(), [](MapTiming const& left, MapTiming const& right)
    {
        return left.durationUs > right.durationUs;
    });

    std::ostringstream sectionText;
    for (std::size_t index = 0; index < sections.size() && index < _topSections; ++index)
    {
        if (index)
            sectionText << ", ";
        sectionText << sections[index].name << '=' << FormatDuration(sections[index].durationUs);
    }
    if (sectionText.str().empty())
        sectionText << "none-above-floor";

    std::ostringstream mapText;
    for (std::size_t index = 0; index < maps.size() && index < _topMaps; ++index)
    {
        if (index)
            mapText << ", ";
        mapText << "map:" << maps[index].mapId << "/instance:" << maps[index].instanceId
                << '=' << FormatDuration(maps[index].durationUs);
    }
    if (mapText.str().empty())
        mapText << "none-above-floor";

    ++_hitchCount;
    _lastHitchMs = elapsedMs;
    LOG_WARN("server.hitch",
        "HITCH_COMPLETE tick={} duration={}ms inputDiff={}ms players={} cpuCore={}pct rss={}MB "
        "dbQueues={{login:{},characters:{},world:{}}} sections=[{}] maps=[{}]",
        _tickId.load(), elapsedMs, _tickDiff.load(), _players.load(), _processCpuCorePct.load(),
        _processMemoryMb.load(), LoginDatabase.QueueSize(), CharacterDatabase.QueueSize(), WorldDatabase.QueueSize(),
        sectionText.str(), mapText.str());
}

HitchProfilerSectionScope::HitchProfilerSectionScope(char const* name)
    : _name(name), _start(std::chrono::steady_clock::now()), _active(sHitchProfiler.IsEnabled())
{
    if (_active)
        sHitchProfiler.EnterSection(_name, _previousSection);
}

HitchProfilerSectionScope::~HitchProfilerSectionScope()
{
    if (!_active)
        return;

    uint64 const durationUs = uint64(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - _start).count());
    sHitchProfiler.LeaveSection(_name, _previousSection, durationUs);
}
