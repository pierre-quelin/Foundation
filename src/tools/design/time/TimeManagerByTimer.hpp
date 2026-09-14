/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TimeManagerByTimer.hpp
 * @brief TimeManager driven by periodic ITimer ticks.
 */
#pragma once

#include "tools/design/factory/IObject.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/os/thread/Thread.h"
#include "tools/os/timer/ITimer.h"

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
 * @brief Production TimeManager — functional time advances on ITimer ticks.
 */
class TimeManagerByTimer : public tools::design::factory::IObject,
                           public ITimeManager,
                           private tools::os::thread::Thread,
                           private tools::os::timer::ITimerListener
{
public:
    explicit TimeManagerByTimer(tools::os::timer::ITimer& timer, std::size_t maxArmed = 256);
    TimeManagerByTimer(ApplicationServices& app, config::Node node);

    ~TimeManagerByTimer() override;

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
        util::chrono::Date deadline;
        std::uint64_t generation;

        [[nodiscard]] bool operator>(const ArmedEntry& other) const noexcept
        {
            return deadline > other.deadline;
        }
    };

    void startWorker();
    void stopWorker();
    void body() override;

    void onTick(util::chrono::Delay elapsed) override;

    void armInternal(TimeRequest& request, util::chrono::Delay delay);
    void expireRequest(TimeRequest& request);

    void refreshMidIncrement();

    tools::os::timer::ITimer& _timer;
    std::size_t _maxArmed;
    util::chrono::Delay _midIncrement{std::chrono::nanoseconds{0}};
    std::mutex _mutex;
    std::condition_variable _cv;
    std::priority_queue<ArmedEntry, std::vector<ArmedEntry>, std::greater<>> _queue;
    std::unordered_map<TimeRequest*, std::uint64_t> _generations;
    std::unordered_map<TimeRequest*, util::chrono::Date> _deadlines;
    bool _quit{false};

    mutable std::mutex _dateMutex;
    std::condition_variable _dateCv;
    util::chrono::Date _currentDate{std::chrono::system_clock::time_point{}};
};

} // namespace tools::design::time
