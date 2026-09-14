/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ILoggerCtrl.hpp
 * @brief Logger control service interface.
 */
#pragma once

namespace util
{
namespace logger
{

/**
 * @brief Transport interface for remote LogService control.
 *
 * Concrete implementations (Telnet, HTTP, serial, …) expose runtime commands
 * that delegate to LogService. Logging policy itself lives in LogService.
 */
class ILoggerCtrl
{
public:
    virtual ~ILoggerCtrl() = default;

    /** @brief Starts the control channel (non-blocking). */
    virtual void start() = 0;

    /** @brief Stops the control channel and releases resources. */
    virtual void stop() = 0;

    /** @brief Returns true while the control channel is active. */
    [[nodiscard]] virtual bool isRunning() const = 0;
};

} // namespace logger
} // namespace util