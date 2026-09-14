/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Timer.h
 * @brief Periodic monotonic timer — Windows implementation.
 */
#pragma once

#include "tools/os/thread/Thread.h"
#include "tools/os/timer/ITimer.h"

#include <atomic>
#include <mutex>

namespace tools::os::timer
{

/**
 * @brief Periodic monotonic timer — production ITimer implementation.
 *
 * A background thread sleeps until each absolute deadline, measures the real
 * elapsed time since the previous tick, and notifies listeners while running().
 * On latency overflow, missed deadlines are skipped to resynchronise the grid.
 *
 * @note Windows: QueryPerformanceCounter deadlines and SetWaitableTimer waits
 *       (CREATE_WAITABLE_TIMER_HIGH_RESOLUTION when available).
 */
class Timer : public ITimer, private tools::os::thread::Thread
{
public:
    Timer();
    ~Timer() override;

    void start() override;
    void stop() override;

    [[nodiscard]] bool running() const noexcept override;

    void setPeriod(util::chrono::Delay period) override;
    [[nodiscard]] util::chrono::Delay period() const noexcept override;

private:
    void body() override;

    mutable std::mutex _mutex;
    util::chrono::Delay _period{std::chrono::nanoseconds{0}};
    util::chrono::Delay _periodOverflow{std::chrono::nanoseconds{0}};
    std::atomic<bool> _running{false};
    std::atomic<bool> _quit{false};
};

} // namespace tools::os::timer
