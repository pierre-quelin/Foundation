/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LoggerBoot.hpp
 * @brief Build LogService InitParams from main.ini logger= tokens.
 */
#pragma once

#include "util/logger/Logger.hpp"
#include "util/logger/TcpLoggerHub.hpp"

#include <memory>
#include <string>
#include <vector>

namespace tools::os::startup
{

/**
 * @brief LogService construction result from logger= tokens.
 */
struct LoggerBoot
{
    util::logger::InitParams sinks;
    std::vector<std::shared_ptr<util::logger::TcpLoggerHub>> tcpHubs;
};

/**
 * @brief Map logger tokens to sinks (fail-fast on unknown tokens).
 * @param tokens From MainIni::loggerTokens ("console", "tcp:<port>", …).
 *
 * After @c LogService is constructed, call @c setLogService on each hub so
 * control commands (sll, …) are available on the same TCP port as the stream.
 */
[[nodiscard]] LoggerBoot makeLoggerBoot(const std::vector<std::string>& tokens);

} // namespace tools::os::startup
