/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SchedulerService.cpp
 */
#include "tools/design/scheduler/SchedulerService.hpp"

#include "tools/design/factory/ApplicationServices.hpp"

#include <algorithm>
#include <stdexcept>
#include <thread>
#include <vector>

namespace tools::design::scheduler
{

namespace
{

[[nodiscard]] std::size_t defaultSharedCount()
{
    // Query only — does not spawn a std::thread. Foundation workers are EventScheduler/Thread.
    const unsigned hc = std::thread::hardware_concurrency();
    if (hc <= 1u)
    {
        return 1u;
    }
    return static_cast<std::size_t>(hc - 1u);
}

[[nodiscard]] std::string truncateName(std::string name, std::size_t maxLen = 15)
{
    if (name.size() > maxLen)
    {
        name.resize(maxLen);
    }
    return name;
}

[[nodiscard]] tools::os::thread::CpuAffinity parseAffinity(const config::Node& aff)
{
    using tools::os::thread::CpuAffinity;

    if (aff.contains("Range") && aff["Range"].is_array() && aff["Range"].size() >= 2)
    {
        const unsigned first = aff["Range"][0].value<unsigned int>();
        const unsigned last  = aff["Range"][1].value<unsigned int>();
        return CpuAffinity::range(first, last);
    }
    if (aff.contains("Cores") && aff["Cores"].is_array())
    {
        std::vector<unsigned int> ids;
        const std::size_t n = aff["Cores"].size();
        ids.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            ids.push_back(aff["Cores"][i].value<unsigned int>());
        }
        if (ids.empty())
        {
            return CpuAffinity::any();
        }
        return CpuAffinity::cores(std::move(ids));
    }
    return CpuAffinity::any();
}

} // namespace

SchedulerService::SchedulerService(ApplicationServices& app, config::Node poolConfig) : _app(app), _poolConfig(std::move(poolConfig)), _sharedCount(defaultSharedCount())
{
    (void)_app;
    if (_poolConfig.isNull())
    {
        return;
    }
    try
    {
        if (_poolConfig.contains("SharedSchedulerCount"))
        {
            _sharedCount = (std::max)(std::size_t{1},
                                      static_cast<std::size_t>(_poolConfig["SharedSchedulerCount"].value<unsigned int>()));
        }
        if (_poolConfig.contains("MaxEventCount"))
        {
            _maxPending = _poolConfig["MaxEventCount"].value<unsigned int>();
        }
        if (_poolConfig.contains("NamePrefix"))
        {
            _namePrefix = _poolConfig["NamePrefix"].value<std::string>();
        }
        if (_poolConfig.contains("Affinity"))
        {
            _poolAffinity = parseAffinity(_poolConfig["Affinity"]);
        }
    }
    catch (const std::runtime_error&)
    {
        // Malformed pool config — keep defaults.
    }
}

SchedulerService::~SchedulerService()
{
    std::lock_guard lock(_mutex);
    _exclusive.clear();
    _shared.clear();
}

void SchedulerService::pruneExclusiveLocked()
{
    _exclusive.erase(std::remove_if(_exclusive.begin(), _exclusive.end(), [](const std::weak_ptr<EventScheduler>& w)
                                    { return w.expired(); }),
                     _exclusive.end());
}

std::size_t SchedulerService::exclusiveAliveCount() const
{
    std::lock_guard lock(_mutex);
    std::size_t n = 0;
    for (const auto& w : _exclusive)
    {
        if (!w.expired())
        {
            ++n;
        }
    }
    return n;
}

void SchedulerService::ensurePool()
{
    if (_started)
    {
        return;
    }

    _shared.reserve(_sharedCount);
    for (std::size_t i = 0; i < _sharedCount; ++i)
    {
        const std::string name = truncateName(_namePrefix + "-" + std::to_string(i));
        auto evt               = std::make_shared<EventScheduler>(_maxPending, name);
        evt->setSchedPolicy(tools::os::thread::SchedPolicy::Other);
        evt->setPriority(tools::os::thread::toPosixPriority(tools::os::thread::ThreadPriority::Normal));
        if (!_poolAffinity.isAny())
        {
            evt->setAffinity(_poolAffinity);
        }
        evt->start();
        _shared.push_back(std::move(evt));
    }
    _started = true;
}

std::shared_ptr<EventScheduler> SchedulerService::acquireShared(
    tools::os::thread::ThreadPriority /*priority*/)
{
    std::lock_guard lock(_mutex);
    ensurePool();
    if (_shared.empty())
    {
        throw std::logic_error("SchedulerService::acquireShared: empty pool");
    }
    const std::size_t idx = _rr.fetch_add(1, std::memory_order_relaxed) % _shared.size();
    return _shared[idx];
}

std::shared_ptr<EventScheduler> SchedulerService::acquireExclusive(
    std::string name,
    tools::os::thread::ThreadPriority priority,
    tools::os::thread::SchedulingPolicy policy,
    tools::os::thread::CpuAffinity affinity,
    std::size_t maxPending)
{
    using tools::os::thread::SchedulingPolicy;
    using tools::os::thread::toPosixPriority;
    using tools::os::thread::toSchedPolicy;

#if defined(_WIN32)
    if (policy == SchedulingPolicy::RoundRobin || policy == SchedulingPolicy::Fifo)
    {
        policy = SchedulingPolicy::Other;
    }
#endif

    std::lock_guard lock(_mutex);
    // Do not ensurePool() here — exclusive kits must not spawn the shared pool.

    if (name.empty())
    {
        name = "EvtExcl";
    }
    name = truncateName(std::move(name));

    auto evt = std::make_shared<EventScheduler>(maxPending, name);
    evt->setSchedPolicy(toSchedPolicy(policy));
    evt->setPriority(toPosixPriority(priority));
    if (!affinity.isAny())
    {
        evt->setAffinity(affinity);
    }
    evt->start();
    pruneExclusiveLocked();
    _exclusive.push_back(evt);
    return evt;
}

} // namespace tools::design::scheduler
