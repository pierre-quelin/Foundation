/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LoggerSinks.hpp
 * @brief spdlog sink registration helpers.
 */
#pragma once

#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <ostream>
#include <string>

namespace util
{
namespace logger
{
namespace sinks
{

/**
 * @brief Built-in sink factories for LogService construction (InitParams).
 *
 * Use these for standard outputs, or pass your own spdlog::sink_ptr for custom
 * sinks (e.g. MQTT, syslog, network). Any class that implements spdlog's sink
 * interface can be used. For live TCP log streaming + commands see
 * util/logger/TcpLoggerHub.hpp (sinks::tcpStream).
 */
/** @return Colored stdout sink (thread-safe). */
inline spdlog::sink_ptr console()
{
    return std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
}

#if defined(_WIN32)
/** @return MSVC / Visual Studio debug output window sink (thread-safe). */
inline spdlog::sink_ptr msvc()
{
    return std::make_shared<spdlog::sinks::msvc_sink_mt>();
}
#endif

/**
 * @brief Rotating file sink.
 * @param filepath Path to the log file (e.g. "logs/application.log").
 * @param max_size Max size in bytes before rotation.
 * @param max_files Number of rotated files to keep.
 * @return Thread-safe rotating file sink.
 */
inline spdlog::sink_ptr rotatingFile(const std::string& filepath,
                                     std::size_t max_size  = 1048576 * 5,
                                     std::size_t max_files = 3)
{
    return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filepath, max_size, max_files);
}

/**
 * @brief User-supplied std::ostream sink (e.g. stringstream, file stream).
 * @param stream The stream; must outlive the LogService.
 * @return Thread-safe ostream sink.
 */
inline spdlog::sink_ptr ostream(std::ostream& stream)
{
    return std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
}

} // namespace sinks
} // namespace logger
} // namespace util