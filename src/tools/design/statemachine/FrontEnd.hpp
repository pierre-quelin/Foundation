/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file FrontEnd.hpp
 * @brief Wires Boost.MSM machines to EventScheduler, TimeRequest, and Logger.
 */
#pragma once

#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/statemachine/StateMachineBase.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/design/time/TimeRequest.hpp"
#include "util/logger/Logger.hpp"

namespace tools::design::statemachine
{

/**
 * @brief Posts SM events on the event scheduler thread and arms timeouts via ITimeManager.
 *
 * Used by pilot classes together with generated *_sm.h/cpp.
 * All state machine events must go through schedule() — never from the timer thread.
 */
class FrontEnd
{
public:
    FrontEnd(tools::design::ApplicationServices& app, scheduler::EventScheduler& evt);

    [[nodiscard]] time::ITimeManager& timeManager() const;

    [[nodiscard]] util::logger::Logger logger() const;

    template <typename F>
    void schedule(F&& work)
    {
        _evt.schedule(std::forward<F>(work));
    }

    template <typename Machine, typename Event>
    void postEvent(Machine& machine, const Event& event)
    {
        _evt.schedule([&machine, event]()
                      { machine.process_event(event); });
    }

    void armTimeout(time::TimeRequest& request);
    void cancelTimeout(time::TimeRequest& request);

private:
    tools::design::ApplicationServices& _app;
    scheduler::EventScheduler& _evt;
};

} // namespace tools::design::statemachine
