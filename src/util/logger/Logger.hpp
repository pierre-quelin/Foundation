/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Logger.hpp
 * @brief Application logger facade over spdlog.
 */
#pragma once

// TODO - No cpp in the first approach to simplify development.
// To be reworked in order to remove the dependency on spdlog in the .hpp.
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory> // for std::make_shared
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility> // std::forward
#include <vector>

namespace util
{
namespace logger
{

// Forward declaration: only Logger may call LogService::registerLogger (friend).
class Logger;

/**
 * @brief Sink list used to construct LogService (ApplicationServices::logs).
 *
 * Pass any spdlog-compatible sink (built-in or custom, e.g. MQTT). Use the
 * factories in LoggerSinks.hpp for console, file, ostream, etc., or add your
 * own spdlog::sink_ptr instances.
 */
using InitParams = std::vector<spdlog::sink_ptr>;

/**
 * @brief Logging service — owned by ApplicationServices::logs (no global singleton).
 *
 * Sinks are fixed at construction; levels are configured per channel. Log calls go through Logger.
 */
class LogService : public std::enable_shared_from_this<LogService>
{
    friend class Logger;

public:
    /**
     * @brief Default spdlog channel name for ad-hoc and global logging.
     */
    static constexpr const char* DefaultLoggerName = "Default";

    /**
     * @brief Constructs LogService with the given sinks.
     *
     * @param init Sink instances (empty uses a default console sink).
     */
    explicit LogService(InitParams const& init = {});

    /**
     * @brief Returns a Logger bound to the default channel (DefaultLoggerName).
     */
    Logger defaultLogger();

    /**
     * @brief Severity levels for log messages (mapped to spdlog).
     */
    enum class LogLevel
    {
        TRACE,    ///< Most verbose; fine-grained tracing.
        DEBUG,    ///< Diagnostic information for debugging.
        INFO,     ///< General informational messages.
        WARNING,  ///< Potentially harmful situations.
        ERROR,    ///< Error events that may still allow the app to continue.
        CRITICAL, ///< Severe errors that may abort the application.
        OFF       ///< No log output (spdlog::level::off).
    };

    /**
     * @brief Sets the minimum log level for a named logger channel.
     *
     * Level can only be set for a name already bound by Logger(name). No spdlog::get.
     *
     * @param logger_name spdlog logger name (channel identifier).
     * @param level Minimum level below which messages are discarded.
     * @return void
     * @throws std::logic_error if no Logger has been constructed for logger_name.
     */
    void setLogLevel(const std::string& logger_name, LogLevel level)
    {
        findLogger(logger_name)->set_level(spdLogLevel(level));
    }

    /**
     * @brief Returns the current log level for a named logger channel.
     *
     * @param logger_name spdlog logger name (channel identifier).
     * @return Current minimum level (including OFF).
     * @throws std::logic_error if no Logger has been constructed for logger_name.
     */
    [[nodiscard]] LogLevel getLogLevel(const std::string& logger_name) const
    {
        return logLevelFromSpdlog(findLogger(logger_name)->level());
    }

    /**
     * @brief Returns the names of all logger channels registered via Logger(name).
     *
     * Order is unspecified; sort the result if a stable listing is required.
     *
     * @return Copy of registered logger names (empty if none constructed yet).
     */
    [[nodiscard]] std::vector<std::string> registeredLoggerNames() const
    {
        std::vector<std::string> names;
        names.reserve(_loggers.size());
        for (const auto& entry : _loggers)
        {
            names.push_back(entry.first);
        }
        return names;
    }

    /**
     * @brief Picks a free channel name: @p base, then @c base~1, @c base~2, …
     *
     * Does not create the logger — call @c Logger afterwards to register the name.
     */
    [[nodiscard]] std::string allocateUniqueLoggerName(const std::string& base) const
    {
        if (!hasLoggerName(base))
        {
            return base;
        }
        for (unsigned n = 1;; ++n)
        {
            const std::string candidate = base + "~" + std::to_string(n);
            if (!hasLoggerName(candidate))
            {
                return candidate;
            }
        }
    }

private:
    [[nodiscard]] bool hasLoggerName(const std::string& logger_name) const
    {
        return _loggers.find(logger_name) != _loggers.end();
    }

    std::shared_ptr<spdlog::logger> registerLogger(const std::string& logger_name)
    {
        auto it = _loggers.find(logger_name);
        if (it != _loggers.end())
        {
            return it->second;
        }
        std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
        if (!logger)
        {
            logger = std::make_shared<spdlog::logger>(logger_name, _sinks.begin(), _sinks.end());
            spdlog::initialize_logger(logger);
        }
        _loggers[logger_name] = logger;
        return logger;
    }

    std::shared_ptr<spdlog::logger> findLogger(const std::string& logger_name) const
    {
        auto it = _loggers.find(logger_name);
        if (it == _loggers.end())
        {
            throw std::logic_error("LogService: no Logger constructed for \"" + logger_name + "\"");
        }
        return it->second;
    }

    /**
     * @brief Writes a formatted log line for the given logger (friend-only).
     *
     * @tparam Args Format argument types (spdlog fmt placeholders).
     * @param logger Target spdlog logger.
     * @param level Severity of the message.
     * @param fmt spdlog-style format string.
     * @param args Values bound to the format string.
     * @return void
     */
    template <typename... Args>
    static void dispatch(const std::shared_ptr<spdlog::logger>& logger, LogLevel level, const std::string& fmt, Args&&... args)
    {
        switch (level)
        {
            case LogLevel::TRACE:
                logger->trace(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::DEBUG:
                logger->debug(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::INFO:
                logger->info(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::WARNING:
                logger->warn(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::ERROR:
                logger->error(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::CRITICAL:
                logger->critical(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::OFF:
                // OFF is a filter threshold, not a message severity; nothing to emit.
                break;
        }
    }

    /**
     * @brief Maps LogService::LogLevel to spdlog's level enum.
     *
     * @param level Application log level.
     * @return Corresponding spdlog::level::level_enum.
     */
    spdlog::level::level_enum spdLogLevel(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::TRACE:
                return spdlog::level::trace;
            case LogLevel::DEBUG:
                return spdlog::level::debug;
            case LogLevel::INFO:
                return spdlog::level::info;
            case LogLevel::WARNING:
                return spdlog::level::warn;
            case LogLevel::ERROR:
                return spdlog::level::err;
            case LogLevel::CRITICAL:
                return spdlog::level::critical;
            case LogLevel::OFF:
                return spdlog::level::off;
        }
        return spdlog::level::info;
    }

    /**
     * @brief Maps spdlog's level enum to LogService::LogLevel.
     */
    LogLevel logLevelFromSpdlog(spdlog::level::level_enum level) const
    {
        switch (level)
        {
            case spdlog::level::trace:
                return LogLevel::TRACE;
            case spdlog::level::debug:
                return LogLevel::DEBUG;
            case spdlog::level::info:
                return LogLevel::INFO;
            case spdlog::level::warn:
                return LogLevel::WARNING;
            case spdlog::level::err:
                return LogLevel::ERROR;
            case spdlog::level::critical:
                return LogLevel::CRITICAL;
            case spdlog::level::off:
                return LogLevel::OFF;
            default:
                return LogLevel::INFO;
        }
    }

    std::vector<spdlog::sink_ptr> _sinks;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> _loggers;
};

inline LogService::LogService(InitParams const& init)
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%t][%n][%^%l%$] %v");
    if (init.empty())
    {
        _sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    else
    {
        _sinks = init;
    }
}

/**
 * @brief Named logger facade; one spdlog channel per LogService instance.
 */
class Logger
{
public:
    explicit Logger(const std::shared_ptr<LogService>& service, const std::string& loggerName) :
        _service(service),
        _impl(service->registerLogger(loggerName))
    {
    }

    /**
     * @brief Logs a formatted message at the given level on this channel.
     *
     * @tparam Args Format argument types.
     * @param level Message severity.
     * @param fmt spdlog-style format string.
     * @param args Values for the format string.
     * @return void
     */
    template <typename... Args>
    void log(LogService::LogLevel level, const std::string& fmt, Args&&... args)
    {
        LogService::dispatch(_impl, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Sets the minimum log level for this logger's channel only.
     *
     * @param level Minimum level for messages attributed to this instance.
     * @return void
     */
    void setLogLevel(LogService::LogLevel level)
    {
        _service->setLogLevel(_impl->name(), level);
    }

    /** @brief spdlog channel name (may include @c ~n uniquification suffix). */
    [[nodiscard]] const std::string& name() const
    {
        return _impl->name();
    }

private:
    std::shared_ptr<LogService> _service;
    std::shared_ptr<spdlog::logger> _impl;
};

inline Logger LogService::defaultLogger()
{
    return Logger(shared_from_this(), DefaultLoggerName);
}

} // namespace logger
} // namespace util