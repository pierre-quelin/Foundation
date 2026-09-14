/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/time/TimeRequest.hpp"

#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/time/ITimeManager.hpp"

namespace tools::design::time
{

TimeRequest::TimeRequest(util::chrono::Delay delay, bool retriggerable, std::string name) : _delay(delay), _retriggerable(retriggerable), _name(std::move(name))
{
}

TimeRequest::~TimeRequest()
{
    if (_manager != nullptr && isRunning())
    {
        _manager->cancel(*this);
    }
}

void TimeRequest::setDelay(util::chrono::Delay delay) noexcept
{
    _delay = delay;
}

void TimeRequest::setTarget(scheduler::EventScheduler& evt, std::function<void()> handler)
{
    _onExpire = [&, work = std::move(handler)]()
    {
        evt.schedule([work]()
                     { work(); });
    };
}

void TimeRequest::dispatch()
{
    if (_onExpire)
    {
        _onExpire();
    }
}

} // namespace tools::design::time
