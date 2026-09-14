/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ApplicationServices.hpp
 * @brief Application-wide services passed to factory-created objects.
 */
#pragma once

#include "tools/design/config/IConfigCenter.hpp"
#include "tools/design/config/Node.hpp"
#include "tools/design/db/IDatabase.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/os/timer/ITimer.h"
#include "util/logger/Logger.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace tools::design
{

namespace factory
{
class InstanceRegistry;
} // namespace factory

namespace scheduler
{
class SchedulerService;
} // namespace scheduler

using LogServicePtr       = std::shared_ptr<util::logger::LogService>;
using TimerPtr            = std::shared_ptr<tools::os::timer::ITimer>;
using TimeManagerPtr      = std::shared_ptr<time::ITimeManager>;
using InstanceRegistryPtr = std::shared_ptr<factory::InstanceRegistry>;
using ConfigCenterPtr     = config::ConfigCenterPtr;
using SchedulerServicePtr = std::shared_ptr<scheduler::SchedulerService>;
using DatabasePtr         = std::shared_ptr<db::IDatabase>;
using EventBusPtr         = std::shared_ptr<ipc::IEventBus>;

/**
 * @brief Services installed at boot and passed to factory::create.
 *
 * Factory constructors: `(ApplicationServices&, config::Node)`.
 *
 * Boot: set `logs`, `timer`, `timeManager`, `config`, then `install(app)` — also
 * registers config via `config::install` internally.
 *
 * Typical boot: `timer` + `TimeManagerByTimer` on ApplicationServices.
 * Factory may still instantiate `SimpleTimeManager` without `timer` (bootstrap).
 *
 * @c db is optional — remains null unless the app calls @c db::wireDatabase.
 * @c eventBus is optional — remains null unless the app calls @c ipc::wireEventBus.
 */
struct ApplicationServices
{
    LogServicePtr logs;
    TimerPtr timer;
    TimeManagerPtr timeManager;
    ConfigCenterPtr config;
    // Declared before instances so destruction order stops Bridges (in instances)
    // while EventBus / scheduler are still alive.
    EventBusPtr eventBus;
    SchedulerServicePtr schedulerService;
    DatabasePtr db;
    InstanceRegistryPtr instances;

    /**
     * @brief Active deployment platform (from main.ini @c platformName).
     *
     * Used by @c config::resolvePlatform to select optional @c Platform.&lt;name&gt; branches.
     */
    std::string platformName;

    /**
     * @brief Logger named from @c config::instanceName (unique via @c allocateUniqueLoggerName).
     *
     * Applies optional @c LogLevel on the node.
     */
    [[nodiscard]] util::logger::Logger logger(config::Node node) const;

    /**
     * @brief Named channel logger; optional @c LogLevel read from @p levelSource when present.
     */
    [[nodiscard]] util::logger::Logger logger(std::string_view channel, config::Node levelSource) const;

    /** Channel logger for boot / ad-hoc code without a config node. */
    [[nodiscard]] std::shared_ptr<util::logger::Logger> loggerFor(std::string_view channel) const;

    /**
     * @brief Shared logger named from @c config::instanceName (unique on collision).
     */
    [[nodiscard]] std::shared_ptr<util::logger::Logger> loggerPtr(config::Node node) const;

    /** Shared named logger; optional @c LogLevel from @p levelSource. */
    [[nodiscard]] std::shared_ptr<util::logger::Logger> loggerPtr(std::string_view channel,
                                                                  config::Node levelSource) const;

    [[nodiscard]] time::ITimeManager& timeManagerService() const;

    [[nodiscard]] tools::os::timer::ITimer& timerService() const;

    /**
     * @brief Shared/exclusive EventScheduler pool.
     * Creates a default @c SchedulerService on first use if unset.
     */
    [[nodiscard]] scheduler::SchedulerService& schedulerServiceRef();

    [[nodiscard]] config::Node root() const;

    /** Resolves a dotted reference from the document root (e.g. "Board.Device"). */
    [[nodiscard]] config::Node resolveReference(std::string_view reference) const;

    /** @see config::follow */
    [[nodiscard]] config::Node follow(config::Node node, std::string_view key) const;

    /** @see config::resolveNamed */
    [[nodiscard]] config::Node resolveNamed(config::Node node, std::string_view name) const;
};

void install(ApplicationServices services);

[[nodiscard]] ApplicationServices& current();

void reset();

} // namespace tools::design
