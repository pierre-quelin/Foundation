/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/statemachine/FrontEnd.hpp"

#include <stdexcept>

namespace tools::design::statemachine
{

FrontEnd::FrontEnd(ApplicationServices& app, scheduler::EventScheduler& evt) : _app(app), _evt(evt)
{
}

time::ITimeManager& FrontEnd::timeManager() const
{
    return _app.timeManagerService();
}

util::logger::Logger FrontEnd::logger() const
{
    return *_app.loggerFor("statemachine");
}

void FrontEnd::armTimeout(time::TimeRequest& request)
{
    timeManager().arm(request);
}

void FrontEnd::cancelTimeout(time::TimeRequest& request)
{
    if (request.isRunning())
    {
        timeManager().cancel(request);
    }
}

} // namespace tools::design::statemachine
