/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ITimer.h
 * @brief Periodic functional time source for TimeManagerByTimer.
 */
#pragma once

#include "util/chrono/Delay.hpp"

#include <mutex>
#include <vector>

namespace tools::os::timer
{

class ITimer;

/**
 * @brief Receives periodic elapsed-time notifications from an ITimer.
 */
class ITimerListener
{
public:
    virtual ~ITimerListener() = default;

    /** @brief Called on each timer tick with the measured functional elapsed time. */
    virtual void onTick(util::chrono::Delay elapsed) = 0;
};

/**
 * @brief Periodic timer that reports elapsed functional time to listeners.
 *
 * Implementations:
 * - Timer — OS thread with monotonic absolute sleeps (production).
 * - DrivenTimer — backlog; external tick() for simulation (sources present, not built yet).
 */
class ITimer
{
public:
    virtual ~ITimer() = default;

    ITimer()                         = default;
    ITimer(const ITimer&)            = delete;
    ITimer& operator=(const ITimer&) = delete;
    ITimer(ITimer&&)                 = delete;
    ITimer& operator=(ITimer&&)      = delete;

    virtual void start() = 0;
    virtual void stop()  = 0;

    [[nodiscard]] virtual bool running() const noexcept = 0;

    virtual void setPeriod(util::chrono::Delay period)                = 0;
    [[nodiscard]] virtual util::chrono::Delay period() const noexcept = 0;

    void addListener(ITimerListener& listener);
    void removeListener(ITimerListener& listener);

protected:
    void notifyListeners(util::chrono::Delay elapsed);

private:
    std::mutex _listenerMutex;
    std::vector<ITimerListener*> _listeners;
};

} // namespace tools::os::timer
