/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SimpleTimeManager.hpp
 * @brief Autonomous TimeManager — no Timer dependency at instantiation.
 *
 * Breaks the bootstrap dependency: factory can create a TimeManager before
 * ApplicationServices::timer is configured. At boot, replace with
 * TimeManagerByTimer when a shared Timer is injected.
 */
#pragma once

#include "tools/design/factory/IObject.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Date.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace tools::design::time
{

/**
 * @brief Default TimeManager for early factory instantiation (no app.timer required).
 */
class SimpleTimeManager : public tools::design::factory::IObject,
                          public ITimeManager,
                          private tools::os::thread::Thread
{
public:
    explicit SimpleTimeManager(std::size_t maxArmed = 256);
    SimpleTimeManager(ApplicationServices& app, config::Node node);

    ~SimpleTimeManager() override;

    void arm(TimeRequest& request) override;
    void forceArm(TimeRequest& request) override;
    void cancel(TimeRequest& request) override;
    void suspend(TimeRequest& request) override;
    void resume(TimeRequest& request) override;

    [[nodiscard]] util::chrono::Date getDate() const override;
    void setDate(util::chrono::Date date) override;
    void sleep(util::chrono::Delay delay) override;

private:
    struct ArmedEntry
    {
        TimeRequest* request;
        std::chrono::steady_clock::time_point deadline;
        std::uint64_t generation;

        [[nodiscard]] bool operator>(const ArmedEntry& other) const noexcept
        {
            return deadline > other.deadline;
        }
    };

    void startWorker();
    void stopWorker();
    void body() override;

    void armInternal(TimeRequest& request, util::chrono::Delay delay);
    void expireRequest(TimeRequest& request);

    std::size_t _maxArmed;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::priority_queue<ArmedEntry, std::vector<ArmedEntry>, std::greater<>> _queue;
    std::unordered_map<TimeRequest*, std::uint64_t> _generations;
    std::unordered_map<TimeRequest*, std::chrono::steady_clock::time_point> _deadlines;
    bool _quit{false};

    mutable std::mutex _dateMutex;
    util::chrono::Date _epochDate{
        std::chrono::system_clock::time_point{}};
    std::chrono::steady_clock::time_point _epochSteady{std::chrono::steady_clock::now()};
};

} // namespace tools::design::time
