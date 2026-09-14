/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MainIni.hpp
 * @brief Minimal bootstrap INI ([CFG]) for main — no third-party parser.
 */
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tools::os::startup
{

/**
 * @brief Values loaded from the [CFG] section of main.ini.
 */
struct MainIni
{
    std::string platformName;
    std::vector<std::string> loggerTokens; ///< e.g. "console", "tcp:8023"
    std::filesystem::path cfgPath;
};

/** @brief Parse [CFG] from an INI file path. Throws on I/O or missing required keys. */
[[nodiscard]] MainIni parseMainIni(const std::filesystem::path& path);

/** @brief Parse [CFG] from INI text (unit tests / in-memory). */
[[nodiscard]] MainIni parseMainIniFromString(std::string_view text, std::string_view source = "main.ini");

/**
 * @brief Resolve main.ini next to the executable, then cwd/src/main.ini (dev).
 * @throws std::runtime_error if not found.
 */
[[nodiscard]] std::filesystem::path resolveMainIniPath();

} // namespace tools::os::startup
