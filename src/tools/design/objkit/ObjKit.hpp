/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ObjKit.hpp
 * @brief Base for factory-instantiated components.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/scheduler/SchedulerService.hpp"
#include "tools/design/statemachine/FrontEnd.hpp"
#include "tools/os/thread/CpuAffinity.hpp"
#include "tools/os/thread/ThreadPolicy.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace tools::design::objkit
{

/**
 * @brief Object kit — shared services and config helpers for factory classes.
 *
 * Simplifies component code, improves readability, and increases productivity.
 * Logger is obtained from ApplicationServices — not passed as a constructor parameter.
 */
class ObjKit : public factory::IObject
{
public:
    ObjKit(ApplicationServices& app, config::Node node);
    ~ObjKit() override;

    ObjKit(const ObjKit&)            = delete;
    ObjKit& operator=(const ObjKit&) = delete;
    ObjKit(ObjKit&&)                 = delete;
    ObjKit& operator=(ObjKit&&)      = delete;

    [[nodiscard]] ApplicationServices& services() const noexcept { return _app; }

    [[nodiscard]] config::Node config() const noexcept { return _config; }

    /** @brief Last segment of @c config().path() — instance name from configuration. */
    [[nodiscard]] std::string shortName() const;

    /**
     * @brief Binds a @c Logger named after @ref shortName.
     *
     * Applies optional @c LogLevel from the component config node.
     */
    void needLogger();

    [[nodiscard]] util::logger::Logger& logger() const;

    /**
     * @brief Acquires an EventScheduler via @c SchedulerService (Shared by default).
     * @param minMaxPending if non-zero, requires capacity >= @p minMaxPending.
     *
     * No per-component @c EventScheduler config key is required. Optional root
     * @c SchedulerPool configures the shared pool.
     *
     * If @c EventScheduler with @c InstanceOf is present (no @c Mode),
     * keeps the factory @c createShared path.
     */
    void needScheduler(std::size_t minMaxPending = 0);

    /**
     * @brief Explicit Shared/Exclusive acquire via @c SchedulerService.
     */
    void needScheduler(tools::os::thread::ThreadPriority priority,
                       scheduler::SchedulerShare share,
                       tools::os::thread::SchedulingPolicy policy =
                           tools::os::thread::SchedulingPolicy::Other,
                       tools::os::thread::CpuAffinity affinity =
                           tools::os::thread::CpuAffinity::any(),
                       std::size_t minMaxPending = 0);

    [[nodiscard]] scheduler::EventScheduler& scheduler() const;

    /**
     * @brief Waits until the component scheduler queue is drained.
     *
     * Call from the derived destructor @b after cancelling timers / disconnecting
     * signals and @b before members that callbacks may touch are destroyed.
     * Shared pool threads keep running; Exclusive schedulers stop when the last
     * @c shared_ptr is released (see @ref releaseScheduler).
     *
     * Default timeout is @ref defaultDrainTimeout (5s). On timeout, logs a WARNING
     * when a logger is available and returns false.
     *
     * @return false on timeout.
     */
    bool drainScheduler();
    bool drainScheduler(util::chrono::Delay timeout);

    /** @brief Default bound for @ref drainScheduler() — avoids hanging shutdown. */
    [[nodiscard]] static util::chrono::Delay defaultDrainTimeout() noexcept;

    /**
     * @brief Drops this kit's scheduler handle (Exclusive may then quit).
     *
     * Prefer @ref drainScheduler first. Invoked automatically from @c ~ObjKit.
     */
    void releaseScheduler() noexcept;

    /**
     * @brief Wires @c FrontEnd + scheduler for a generated Boost.MSM machine.
     *
     * Creates @c _frontEnd and @p machine when null. Call @c startStateMachine after
     * setting @c Sm_::owner on the pilot.
     */
    template <typename SmType>
    void needStateMachine(std::unique_ptr<SmType>& machine)
    {
        needScheduler();
        if (_frontEnd == nullptr)
        {
            _frontEnd = std::make_unique<statemachine::FrontEnd>(_app, scheduler());
        }
        if (machine == nullptr)
        {
            machine = std::make_unique<SmType>();
        }
    }

    [[nodiscard]] statemachine::FrontEnd& frontEnd()
    {
        if (_frontEnd == nullptr)
        {
            throw std::logic_error("ObjKit::frontEnd: call needStateMachine() first");
        }
        return *_frontEnd;
    }

    [[nodiscard]] const statemachine::FrontEnd& frontEnd() const
    {
        if (_frontEnd == nullptr)
        {
            throw std::logic_error("ObjKit::frontEnd: call needStateMachine() first");
        }
        return *_frontEnd;
    }

    /** @brief Starts @p machine on the component EventScheduler thread. */
    template <typename SmType>
    void startStateMachine(SmType& machine)
    {
        frontEnd().schedule([&machine]()
                            { machine.start(); });
    }

protected:
    ApplicationServices& _app;
    config::Node _config;

private:
    std::shared_ptr<util::logger::Logger> _logger;
    std::shared_ptr<scheduler::EventScheduler> _scheduler;
    std::unique_ptr<statemachine::FrontEnd> _frontEnd;
};

} // namespace tools::design::objkit
