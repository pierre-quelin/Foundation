/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/time/ITimeManager.hpp"

#include <stdexcept>

namespace tools::design::time
{

void ITimeManager::sleepFor(util::chrono::Delay delay)
{
    design::current().timeManagerService().sleep(delay);
}

void ITimeManager::ensureStopped(const TimeRequest& request)
{
    if (request.isRunning() || request.isSuspended())
    {
        throw std::logic_error("ITimeManager::arm: request not stopped");
    }
}

void ITimeManager::ensureNotRunning(const TimeRequest& request)
{
    if (request.isRunning())
    {
        throw std::logic_error("ITimeManager::arm: request already running");
    }
}

void ITimeManager::ensureHasTarget(const TimeRequest& request)
{
    if (!request.hasTarget())
    {
        throw std::logic_error("ITimeManager::arm: request has no target");
    }
}

} // namespace tools::design::time
