/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/factory/ApplicationServices.hpp"

#include "tools/design/config/Reference.hpp"
#include "tools/design/config/Registry.hpp"
#include "tools/design/factory/InstanceRegistry.hpp"
#include "tools/design/scheduler/SchedulerService.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/os/timer/ITimer.h"
#include "util/logger/Logger.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace tools::design
{

namespace
{

ApplicationServices* g_services = nullptr;

[[nodiscard]] util::logger::LogService::LogLevel parseLogLevel(std::string_view name)
{
    using Level = util::logger::LogService::LogLevel;
    if (name == "TRACE")
    {
        return Level::TRACE;
    }
    if (name == "DEBUG")
    {
        return Level::DEBUG;
    }
    if (name == "INFO")
    {
        return Level::INFO;
    }
    if (name == "WARNING")
    {
        return Level::WARNING;
    }
    if (name == "ERROR")
    {
        return Level::ERROR;
    }
    if (name == "CRITICAL")
    {
        return Level::CRITICAL;
    }
    if (name == "OFF")
    {
        return Level::OFF;
    }
    throw std::runtime_error(std::string{"ApplicationServices: unknown LogLevel '"} +
                             std::string{name} + "'");
}

void requireLogs(const std::shared_ptr<util::logger::LogService>& logs)
{
    if (logs == nullptr)
    {
        throw std::logic_error("ApplicationServices: LogService not configured");
    }
}

void requireTimeManager(const TimeManagerPtr& timeManager)
{
    if (timeManager == nullptr)
    {
        throw std::logic_error("ApplicationServices: TimeManager not configured");
    }
}

void requireTimer(const TimerPtr& timer)
{
    if (timer == nullptr)
    {
        throw std::logic_error("ApplicationServices: Timer not configured");
    }
}

} // namespace

util::logger::Logger ApplicationServices::logger(config::Node node) const
{
    requireLogs(logs);
    const std::string base    = config::instanceName(node.path());
    const std::string channel = logs->allocateUniqueLoggerName(base.empty() ? "factory" : base);
    return logger(channel, node);
}

util::logger::Logger ApplicationServices::logger(std::string_view channel, config::Node levelSource) const
{
    requireLogs(logs);
    util::logger::Logger result{logs, std::string{channel}};
    if (levelSource.contains("LogLevel"))
    {
        result.setLogLevel(parseLogLevel(levelSource["LogLevel"].value<std::string>()));
    }
    return result;
}

std::shared_ptr<util::logger::Logger> ApplicationServices::loggerFor(std::string_view channel) const
{
    requireLogs(logs);
    return std::make_shared<util::logger::Logger>(logs, std::string{channel});
}

std::shared_ptr<util::logger::Logger> ApplicationServices::loggerPtr(config::Node node) const
{
    requireLogs(logs);
    const std::string base    = config::instanceName(node.path());
    const std::string channel = logs->allocateUniqueLoggerName(base.empty() ? "factory" : base);
    return loggerPtr(channel, node);
}

std::shared_ptr<util::logger::Logger> ApplicationServices::loggerPtr(std::string_view channel,
                                                                     config::Node levelSource) const
{
    requireLogs(logs);
    auto result = std::make_shared<util::logger::Logger>(logs, std::string{channel});
    if (levelSource.contains("LogLevel"))
    {
        result->setLogLevel(parseLogLevel(levelSource["LogLevel"].value<std::string>()));
    }
    return result;
}

time::ITimeManager& ApplicationServices::timeManagerService() const
{
    requireTimeManager(timeManager);
    return *timeManager;
}

tools::os::timer::ITimer& ApplicationServices::timerService() const
{
    requireTimer(timer);
    return *timer;
}

scheduler::SchedulerService& ApplicationServices::schedulerServiceRef()
{
    if (schedulerService == nullptr)
    {
        config::Node pool{};
        try
        {
            if (config != nullptr && root().contains("SchedulerPool"))
            {
                pool = root()["SchedulerPool"];
            }
        }
        catch (const std::exception&)
        {
        }
        schedulerService = std::make_shared<scheduler::SchedulerService>(*this, pool);
    }
    return *schedulerService;
}

config::Node ApplicationServices::root() const
{
    if (config == nullptr)
    {
        throw std::logic_error("ApplicationServices::root: config not configured");
    }
    return config->root();
}

config::Node ApplicationServices::resolveReference(std::string_view reference) const
{
    return config::resolveReference(root(), reference);
}

config::Node ApplicationServices::follow(config::Node node, std::string_view key) const
{
    return config::follow(root(), node, key);
}

config::Node ApplicationServices::resolveNamed(config::Node node, std::string_view name) const
{
    return config::resolveNamed(root(), node, name);
}

void install(ApplicationServices services)
{
    if (services.instances == nullptr)
    {
        services.instances = std::make_shared<factory::InstanceRegistry>();
    }
    if (services.config != nullptr)
    {
        config::install(services.config);
    }
    if (g_services != nullptr)
    {
        delete g_services;
    }
    g_services = new ApplicationServices{std::move(services)};
}

ApplicationServices& current()
{
    if (g_services == nullptr)
    {
        throw std::logic_error("tools::design::current: install() not called");
    }
    return *g_services;
}

void reset()
{
    if (g_services != nullptr && g_services->instances != nullptr)
    {
        g_services->instances->clear();
    }
    delete g_services;
    g_services = nullptr;
    config::reset();
}

} // namespace tools::design
