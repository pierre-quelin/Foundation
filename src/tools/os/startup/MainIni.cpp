/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/startup/MainIni.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace tools::os::startup
{

namespace
{

[[nodiscard]] std::filesystem::path executableDirectory()
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
#else
    char buffer[4096];
    const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0)
    {
        return {};
    }
    buffer[length] = '\0';
    return std::filesystem::path(buffer).parent_path();
#endif
}

[[nodiscard]] std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char c)
    { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

[[nodiscard]] std::vector<std::string> splitCommaList(const std::string& value)
{
    std::vector<std::string> tokens;
    std::istringstream iss(value);
    std::string part;
    while (std::getline(iss, part, ','))
    {
        auto token = trim(std::move(part));
        if (!token.empty())
        {
            tokens.push_back(std::move(token));
        }
    }
    return tokens;
}

[[nodiscard]] bool isCommentOrEmpty(const std::string& line)
{
    if (line.empty())
    {
        return true;
    }
    return line[0] == '#' || line[0] == ';';
}

[[nodiscard]] MainIni parseLines(std::istream& in, std::string_view source)
{
    MainIni result;
    bool inCfg           = false;
    bool sawPlatformName = false;
    bool sawLogger       = false;
    bool sawCfg          = false;
    std::string line;
    std::size_t lineNo = 0;

    while (std::getline(in, line))
    {
        ++lineNo;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        line = trim(std::move(line));
        if (isCommentOrEmpty(line))
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            const auto section = trim(line.substr(1, line.size() - 2));
            inCfg              = (section == "CFG");
            continue;
        }

        if (!inCfg)
        {
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            throw std::runtime_error(std::string{source} + ':' + std::to_string(lineNo) +
                                     ": expected key=value in [CFG]");
        }

        const auto key   = trim(line.substr(0, eq));
        const auto value = trim(line.substr(eq + 1));
        if (key.empty())
        {
            throw std::runtime_error(std::string{source} + ':' + std::to_string(lineNo) +
                                     ": empty key in [CFG]");
        }

        if (key == "platformName")
        {
            result.platformName = value;
            sawPlatformName     = true;
        }
        else if (key == "logger")
        {
            result.loggerTokens = splitCommaList(value);
            sawLogger           = true;
        }
        else if (key == "cfg")
        {
            result.cfgPath = value;
            sawCfg         = true;
        }
        // Unknown keys in [CFG] are ignored (forward-compatible).
    }

    if (!sawPlatformName || result.platformName.empty())
    {
        throw std::runtime_error(std::string{source} + ": missing required [CFG] platformName");
    }
    if (!sawLogger)
    {
        throw std::runtime_error(std::string{source} + ": missing required [CFG] logger");
    }
    if (result.loggerTokens.empty())
    {
        throw std::runtime_error(std::string{source} + ": [CFG] logger list is empty");
    }
    if (!sawCfg || result.cfgPath.empty())
    {
        throw std::runtime_error(std::string{source} + ": missing required [CFG] cfg");
    }

    return result;
}

} // namespace

MainIni parseMainIni(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("cannot open main.ini: " + path.string());
    }
    return parseLines(file, path.string());
}

MainIni parseMainIniFromString(std::string_view text, std::string_view source)
{
    std::istringstream in{std::string{text}};
    return parseLines(in, source);
}

std::filesystem::path resolveMainIniPath()
{
    std::vector<std::filesystem::path> candidates;

    const auto exeDir = executableDirectory();
    if (!exeDir.empty())
    {
        candidates.push_back(exeDir / "main.ini");
        candidates.push_back(exeDir / "cfg" / "main.ini");
    }

    // Dev / workspace: repo root as cwd, source tree layout.
    candidates.push_back(std::filesystem::current_path() / "src" / "main.ini");

    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }

    throw std::runtime_error("main.ini not found (beside executable or src/main.ini from cwd)");
}

} // namespace tools::os::startup
