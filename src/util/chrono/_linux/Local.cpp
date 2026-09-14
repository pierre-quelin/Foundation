/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "util/chrono/Local.hpp"

#include <ctime>

namespace util::chrono
{

LocalParts toLocal(const Date date)
{
    const auto time = std::chrono::system_clock::to_time_t(date);
    std::tm parts{};
    localtime_r(&time, &parts);

    LocalParts result;
    result.year   = parts.tm_year + 1900;
    result.month  = parts.tm_mon + 1;
    result.day    = parts.tm_mday;
    result.hour   = parts.tm_hour;
    result.minute = parts.tm_min;
    result.second = parts.tm_sec;
    result.isDst  = parts.tm_isdst > 0;
    return result;
}

Date fromLocal(const LocalParts& parts)
{
    std::tm local{};
    local.tm_year  = parts.year - 1900;
    local.tm_mon   = parts.month - 1;
    local.tm_mday  = parts.day;
    local.tm_hour  = parts.hour;
    local.tm_min   = parts.minute;
    local.tm_sec   = parts.second;
    local.tm_isdst = -1;

    const auto time = std::mktime(&local);
    return Date{std::chrono::system_clock::from_time_t(time)};
}

} // namespace util::chrono
