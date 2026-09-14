/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/objkit/ObjKit.hpp"

#include "tools/design/factory/Obtain.hpp"
#include "tools/design/scheduler/SchedulerService.hpp"
#include "tools/os/thread/CpuAffinity.hpp"
#include "tools/os/thread/ThreadPolicy.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>

namespace tools::design::objkit
{

namespace
{

void requireMinPending(const scheduler::EventScheduler& evt, std::size_t minMaxPending)
{
    if (minMaxPending > 0 && evt.maxPending() < minMaxPending)
    {
        throw std::runtime_error(
            "ObjKit::needScheduler: EventScheduler MaxEventCount (" +
            std::to_string(evt.maxPending()) + ") < required (" +
            std::to_string(minMaxPending) + ")");
    }
}

} // namespace

ObjKit::ObjKit(ApplicationServices& app, config::Node node) : _app(app), _config(std::move(node))
{
}

ObjKit::~ObjKit()
{
    releaseScheduler();
}

std::string ObjKit::shortName() const
{
    const std::string path = _config.path();
    if (path.empty())
    {
        return {};
    }
    const auto slash = path.rfind('/');
    const auto dot   = path.rfind('.');
    const auto pos =
        (slash == std::string::npos) ? dot
        : (dot == std::string::npos) ? slash
                                     : (std::max)(slash, dot);
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

void ObjKit::needLogger()
{
    if (_logger != nullptr)
    {
        return;
    }

    const std::string base = shortName();
    if (base.empty())
    {
        throw std::runtime_error("ObjKit::needLogger: config node has no instance name");
    }
    if (_app.logs == nullptr)
    {
        throw std::logic_error("ObjKit::needLogger: LogService not configured");
    }

    const std::string unique = _app.logs->allocateUniqueLoggerName(base);
    _logger                  = _app.loggerPtr(unique, _config);
}

util::logger::Logger& ObjKit::logger() const
{
    if (_logger == nullptr)
    {
        throw std::logic_error("ObjKit::logger: call needLogger() first");
    }
    return *_logger;
}

void ObjKit::needScheduler(const std::size_t minMaxPending)
{
    if (_scheduler != nullptr)
    {
        return;
    }

    // Dedicated EventScheduler via factory when InstanceOf is present.
    if (_config.contains("EventScheduler"))
    {
        _scheduler =
            factory::createShared<scheduler::EventScheduler>(_app, _config, "EventScheduler");
        requireMinPending(*_scheduler, minMaxPending);
        return;
    }

    // Code-first default — Shared pool (root SchedulerPool optional).
    needScheduler(tools::os::thread::ThreadPriority::Normal,
                  scheduler::SchedulerShare::Shared,
                  tools::os::thread::SchedulingPolicy::Other,
                  tools::os::thread::CpuAffinity::any(),
                  minMaxPending);
}

void ObjKit::needScheduler(tools::os::thread::ThreadPriority priority,
                           scheduler::SchedulerShare share,
                           tools::os::thread::SchedulingPolicy policy,
                           tools::os::thread::CpuAffinity affinity,
                           std::size_t minMaxPending)
{
    if (_scheduler != nullptr)
    {
        return;
    }

    auto& svc = _app.schedulerServiceRef();

    if (share == scheduler::SchedulerShare::Shared)
    {
        if (policy != tools::os::thread::SchedulingPolicy::Other)
        {
            throw std::runtime_error(
                "ObjKit::needScheduler: RoundRobin/Fifo only allowed with Exclusive share");
        }
        _scheduler = svc.acquireShared(priority);
    }
    else
    {
        const std::string name =
            shortName().empty() ? std::string{"EvtExcl"} : (shortName() + ".evt");
        const std::size_t maxPending =
            minMaxPending > 0 ? (std::max)(minMaxPending, std::size_t{256}) : std::size_t{256};
        _scheduler =
            svc.acquireExclusive(name, priority, policy, affinity, maxPending);
    }

    requireMinPending(*_scheduler, minMaxPending);
}

scheduler::EventScheduler& ObjKit::scheduler() const
{
    if (_scheduler == nullptr)
    {
        throw std::logic_error("ObjKit::scheduler: call needScheduler() first");
    }
    return *_scheduler;
}

util::chrono::Delay ObjKit::defaultDrainTimeout() noexcept
{
    return util::chrono::Delay{std::chrono::seconds{5}};
}

bool ObjKit::drainScheduler()
{
    return drainScheduler(defaultDrainTimeout());
}

bool ObjKit::drainScheduler(util::chrono::Delay timeout)
{
    if (_scheduler == nullptr)
    {
        return true;
    }
    if (_scheduler->synchronise(timeout))
    {
        return true;
    }

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        timeout.toNanoseconds())
                        .count();
    try
    {
        const auto msg =
            "drainScheduler: timed out after {}ms — queued work may still run (UAF risk)";
        if (_logger != nullptr)
        {
            _logger->log(util::logger::LogService::LogLevel::WARNING, msg, ms);
        }
        else if (_app.logs != nullptr)
        {
            const std::string channel =
                shortName().empty() ? std::string{"ObjKit"} : shortName();
            if (const auto log = _app.loggerFor(channel))
            {
                log->log(util::logger::LogService::LogLevel::WARNING, msg, ms);
            }
        }
    }
    catch (...)
    {
        // Logging must not throw from destruction paths.
    }
    return false;
}

void ObjKit::releaseScheduler() noexcept
{
    _frontEnd.reset();
    _scheduler.reset();
}

} // namespace tools::design::objkit
