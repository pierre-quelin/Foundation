/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SchedulerService.hpp
 * @brief Shared / exclusive EventScheduler pool.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/os/thread/CpuAffinity.hpp"
#include "tools/os/thread/ThreadPolicy.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::scheduler
{

enum class SchedulerShare
{
    Shared,
    Exclusive
};

/**
 * @brief Owns a pool of shared @c EventScheduler and creates exclusive ones on demand.
 *
 * Threads remain an implementation detail of @c EventScheduler.
 */
class SchedulerService
{
public:
    explicit SchedulerService(ApplicationServices& app, config::Node poolConfig = {});
    ~SchedulerService();

    SchedulerService(const SchedulerService&)            = delete;
    SchedulerService& operator=(const SchedulerService&) = delete;

    [[nodiscard]] std::shared_ptr<EventScheduler> acquireShared(
        tools::os::thread::ThreadPriority priority = tools::os::thread::ThreadPriority::Normal);

    [[nodiscard]] std::shared_ptr<EventScheduler> acquireExclusive(
        std::string name,
        tools::os::thread::ThreadPriority priority = tools::os::thread::ThreadPriority::Normal,
        tools::os::thread::SchedulingPolicy policy = tools::os::thread::SchedulingPolicy::Other,
        tools::os::thread::CpuAffinity affinity    = tools::os::thread::CpuAffinity::any(),
        std::size_t maxPending                     = 256);

    [[nodiscard]] std::size_t sharedCount() const noexcept { return _shared.size(); }

    /** @brief Number of exclusive schedulers still held by components (weak refs). */
    [[nodiscard]] std::size_t exclusiveAliveCount() const;

private:
    void ensurePool();
    void pruneExclusiveLocked();

    ApplicationServices& _app;
    config::Node _poolConfig;
    std::size_t _sharedCount{1};
    std::size_t _maxPending{256};
    std::string _namePrefix{"EvtShared"};
    tools::os::thread::CpuAffinity _poolAffinity{tools::os::thread::CpuAffinity::any()};
    mutable std::mutex _mutex;
    std::vector<std::shared_ptr<EventScheduler>> _shared;
    std::vector<std::weak_ptr<EventScheduler>> _exclusive;
    std::atomic<std::size_t> _rr{0};
    bool _started{false};
};

} // namespace tools::design::scheduler
