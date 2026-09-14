/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
  * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LoggerCommands.hpp
 * @brief Logger control command parsing (e.g. sll).
 */
#pragma once

#include "util/logger/Logger.hpp"

#include <string>

namespace util
{
namespace logger
{

/**
 * @brief Parses a log level name (case-insensitive, WARN accepted for WARNING, Off for OFF).
 * @throws std::invalid_argument if the name is unknown.
 */
LogService::LogLevel parseLogLevel(const std::string& levelName);

/** @brief Returns the canonical log level name (e.g. "Debug", "Off"). */
std::string logLevelToString(LogService::LogLevel level);

/** @brief Returns the help text listing available logger control commands. */
std::string loggerHelpText();

/**
 * @brief Processes one logger control command line.
 *
 * Supported commands:
 *   sll <module> <level>   — set log level (Trace..Critical, Off)
 *   gll [module]           — get log level (all registered modules if omitted)
 *   help                   — list commands
 *
 * @return Response text (may be multi-line); empty line yields a short prompt hint.
 */
std::string processLoggerCommand(LogService& service, const std::string& line);

} // namespace logger
} // namespace util