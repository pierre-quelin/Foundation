/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Local.hpp
 * @brief Human wall-clock conversion via OS timezone APIs (C++17).
 */
#pragma once

#include "util/chrono/Date.hpp"

namespace util::chrono
{

/** @brief Calendar fields in the local timezone (platform OS). */
struct LocalParts
{
    int year{0};
    int month{0};  ///< 1–12
    int day{0};    ///< 1–31
    int hour{0};   ///< 0–23
    int minute{0}; ///< 0–59
    int second{0}; ///< 0–59
    bool isDst{false};
};

/** @brief Converts a UTC-based Date to local calendar fields. */
[[nodiscard]] LocalParts toLocal(Date date);

/** @brief Builds a Date from local calendar fields (inverse of toLocal). */
[[nodiscard]] Date fromLocal(const LocalParts& parts);

} // namespace util::chrono
