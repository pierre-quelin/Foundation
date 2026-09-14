/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ITimeManager.hpp
 * @brief Time manager interface (Foundation Phase 4).
 */
#pragma once

#include "tools/design/time/TimeRequest.hpp"
#include "util/chrono/Date.hpp"
#include "util/chrono/Delay.hpp"

namespace tools::design::time
{

/**
 * @brief Manages armed TimeRequest instances and the application functional timeline.
 *
 * Functional time (date, delay, duration) goes through TimeManager — including
 * getDate() / setDate() for the application timeline.
 * Human wall-clock concerns (timezone, DST) belong in util::chrono.
 *
 * Implementations:
 * - SimpleTimeManager — autonomous; factory-instantiable without Timer (bootstrap).
 * - TimeManagerByTimer — requires ApplicationServices::timer.
 * Owned by ApplicationServices::timeManager at boot.
 */
class ITimeManager
{
public:
    ITimeManager()          = default;
    virtual ~ITimeManager() = default;

    ITimeManager(const ITimeManager&)            = delete;
    ITimeManager& operator=(const ITimeManager&) = delete;
    ITimeManager(ITimeManager&&)                 = delete;
    ITimeManager& operator=(ITimeManager&&)      = delete;

    virtual void arm(TimeRequest& request)      = 0;
    virtual void forceArm(TimeRequest& request) = 0;
    virtual void cancel(TimeRequest& request)   = 0;

    virtual void suspend(TimeRequest& request) = 0;
    virtual void resume(TimeRequest& request)  = 0;

    /** @brief Functional application date. */
    [[nodiscard]] virtual util::chrono::Date getDate() const = 0;

    /** @brief Repositions the functional application date. */
    virtual void setDate(util::chrono::Date date) = 0;

    /**
     * @brief Blocks until functional time advances by @p delay.
     *
     * SimpleTimeManager — steady_clock aligned with getDate().
     * TimeManagerByTimer — waits on ITimer ticks (not IThread::sleep_for).
     */
    virtual void sleep(util::chrono::Delay delay) = 0;

    /** @brief Delegates to ApplicationServices::timeManagerService().sleep(). */
    static void sleepFor(util::chrono::Delay delay);

protected:
    static void ensureStopped(const TimeRequest& request);
    static void ensureNotRunning(const TimeRequest& request);
    static void ensureHasTarget(const TimeRequest& request);
};

} // namespace tools::design::time
