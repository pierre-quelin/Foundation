/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/timer/ITimer.h"

#include <algorithm>

namespace tools::os::timer
{

void ITimer::addListener(ITimerListener& listener)
{
    const std::lock_guard lock(_listenerMutex);
    auto it = std::find(_listeners.begin(), _listeners.end(), &listener);
    if (it == _listeners.end())
    {
        _listeners.push_back(&listener);
    }
}

void ITimer::removeListener(ITimerListener& listener)
{
    const std::lock_guard lock(_listenerMutex);
    auto it = std::find(_listeners.begin(), _listeners.end(), &listener);
    if (it != _listeners.end())
    {
        _listeners.erase(it);
    }
}

void ITimer::notifyListeners(util::chrono::Delay elapsed)
{
    std::vector<ITimerListener*> listeners;
    {
        const std::lock_guard lock(_listenerMutex);
        listeners = _listeners;
    }

    for (ITimerListener* listener : listeners)
    {
        listener->onTick(elapsed);
    }
}

} // namespace tools::os::timer
