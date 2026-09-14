/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
  *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
*/

#include "util/logger/LoggerCommands.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace util
{
namespace logger
{

namespace
{

std::string toUpper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                   { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char c)
    { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

} // namespace

LogService::LogLevel parseLogLevel(const std::string& levelName)
{
    const std::string upper = toUpper(levelName);
    if (upper == "TRACE")
    {
        return LogService::LogLevel::TRACE;
    }
    if (upper == "DEBUG")
    {
        return LogService::LogLevel::DEBUG;
    }
    if (upper == "INFO")
    {
        return LogService::LogLevel::INFO;
    }
    if (upper == "WARNING" || upper == "WARN")
    {
        return LogService::LogLevel::WARNING;
    }
    if (upper == "ERROR")
    {
        return LogService::LogLevel::ERROR;
    }
    if (upper == "CRITICAL")
    {
        return LogService::LogLevel::CRITICAL;
    }
    if (upper == "OFF")
    {
        return LogService::LogLevel::OFF;
    }
    throw std::invalid_argument("unknown log level \"" + levelName + "\"");
}

std::string logLevelToString(LogService::LogLevel level)
{
    switch (level)
    {
        case LogService::LogLevel::TRACE:
            return "Trace";
        case LogService::LogLevel::DEBUG:
            return "Debug";
        case LogService::LogLevel::INFO:
            return "Info";
        case LogService::LogLevel::WARNING:
            return "Warning";
        case LogService::LogLevel::ERROR:
            return "Error";
        case LogService::LogLevel::CRITICAL:
            return "Critical";
        case LogService::LogLevel::OFF:
            return "Off";
    }
    return "Info";
}

std::string loggerHelpText()
{
    return "Logger control commands:\n"
           "  sll <module> <level>  set log level\n"
           "  gll [module]          get log level (all modules if omitted)\n"
           "  help                  show this message\n"
           "  quit                  close session\n"
           "\n"
           "Levels: Trace, Debug, Info, Warning, Error, Critical, Off\n"
           "\n"
           "Examples:\n"
           "  sll EsploraBoard Debug\n"
           "  sll EsploraBoard Off\n"
           "  gll EsploraBoard\n"
           "  gll";
}

std::string processLoggerCommand(LogService& service, const std::string& line)
{
    const std::string trimmed = trim(line);
    if (trimmed.empty())
    {
        return "Commands: sll, gll, help";
    }

    std::istringstream iss(trimmed);
    std::string cmd;
    iss >> cmd;
    cmd = toUpper(cmd);

    if (cmd == "HELP" || cmd == "?")
    {
        return loggerHelpText();
    }

    if (cmd == "SLL")
    {
        std::string module;
        std::string levelName;
        iss >> module >> levelName;
        if (module.empty() || levelName.empty())
        {
            return "Usage: sll <module> <level>";
        }
        try
        {
            const auto level = parseLogLevel(levelName);
            service.setLogLevel(module, level);
            return "Set Log Level of module " + module + " to " + logLevelToString(level) + ".";
        }
        catch (const std::exception& e)
        {
            return std::string("Error: ") + e.what();
        }
    }

    if (cmd == "GLL")
    {
        std::string module;
        iss >> module;
        try
        {
            if (module.empty())
            {
                auto names = service.registeredLoggerNames();
                if (names.empty())
                {
                    return "No loggers registered.";
                }
                std::sort(names.begin(), names.end());
                std::ostringstream out;
                for (const auto& name : names)
                {
                    out << name << ": " << logLevelToString(service.getLogLevel(name)) << '\n';
                }
                std::string result = out.str();
                result.pop_back();
                return result;
            }
            const auto level = service.getLogLevel(module);
            return "Log Level of module " + module + " is " + logLevelToString(level) + '.';
        }
        catch (const std::exception& e)
        {
            return std::string("Error: ") + e.what();
        }
    }

    return "Unknown command \"" + cmd + "\". Type help for available commands.";
}

} // namespace logger
} // namespace util
