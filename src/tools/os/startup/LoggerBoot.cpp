/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/startup/LoggerBoot.hpp"

#include "util/logger/LoggerSinks.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tools::os::startup
{

namespace
{

[[nodiscard]] std::uint16_t parseTcpPort(std::string_view token)
{
    constexpr std::string_view prefix = "tcp:";
    if (token.size() <= prefix.size() || token.compare(0, prefix.size(), prefix) != 0)
    {
        throw std::runtime_error("invalid tcp token \"" + std::string{token} + "\"");
    }
    const auto portStr = token.substr(prefix.size());
    try
    {
        const int port = std::stoi(std::string{portStr});
        if (port <= 0 || port > 65535)
        {
            throw std::out_of_range("port");
        }
        return static_cast<std::uint16_t>(port);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error("invalid tcp port in \"" + std::string{token} + "\"");
    }
}

} // namespace

LoggerBoot makeLoggerBoot(const std::vector<std::string>& tokens)
{
    LoggerBoot boot;
    boot.sinks.reserve(tokens.size());

    for (const auto& token : tokens)
    {
        if (token == "console")
        {
            boot.sinks.push_back(util::logger::sinks::console());
            continue;
        }
        if (token.compare(0, 4, "tcp:") == 0)
        {
            const auto port = parseTcpPort(token);
            auto hub        = std::make_shared<util::logger::TcpLoggerHub>(port);
            boot.tcpHubs.push_back(hub);
            boot.sinks.push_back(hub);
            continue;
        }
#if defined(_WIN32)
        if (token == "msvc")
        {
            boot.sinks.push_back(util::logger::sinks::msvc());
            continue;
        }
#endif
        throw std::runtime_error("unknown logger sink token \"" + token +
                                 "\" (expected console, tcp:<port>"
#if defined(_WIN32)
                                 ", msvc"
#endif
                                 ")");
    }

    return boot;
}

} // namespace tools::os::startup
