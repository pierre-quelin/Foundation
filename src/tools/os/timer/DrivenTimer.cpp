/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/timer/DrivenTimer.h"

#include "util/chrono/Delay.hpp"

namespace tools::os::timer
{

using namespace util::chrono::literals;

DrivenTimer::DrivenTimer() : _period(20_ms)
{
}

void DrivenTimer::tick(util::chrono::Delay step)
{
    if (!_running.load())
    {
        return;
    }

    notifyListeners(step);
}

void DrivenTimer::start()
{
    _running.store(true);
}

void DrivenTimer::stop()
{
    _running.store(false);
}

bool DrivenTimer::running() const noexcept
{
    return _running.load();
}

void DrivenTimer::setPeriod(util::chrono::Delay period)
{
    const std::lock_guard lock(_mutex);
    _period = period;
}

util::chrono::Delay DrivenTimer::period() const noexcept
{
    const std::lock_guard lock(_mutex);
    return _period;
}

} // namespace tools::os::timer
