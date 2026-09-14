/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Date.hpp
 * @brief Foundation time point type (Date).
 */
#pragma once

#include "util/chrono/Delay.hpp"

#include <chrono>

namespace util::chrono
{

/**
 * @brief Time point wrapper for Foundation APIs.
 *
 * Public Phase 0 surface uses Date instead of raw std::chrono::system_clock::time_point.
 * A Date is that time point (thin wrapper); converts implicitly for std::chrono interop.
 */
class Date
{
public:
    [[nodiscard]] static Date now()
    {
        return Date{std::chrono::system_clock::now()};
    }

    explicit Date(std::chrono::system_clock::time_point tp) noexcept
        : _timePoint(tp)
    {
    }

    [[nodiscard]] Delay since(const Date& earlier) const noexcept
    {
        return *this - earlier;
    }

    Date& operator+=(const Delay& rhs) noexcept
    {
        _timePoint += std::chrono::duration_cast<std::chrono::system_clock::duration>(
            rhs.toNanoseconds());
        return *this;
    }

    Date& operator-=(const Delay& rhs) noexcept
    {
        _timePoint -= std::chrono::duration_cast<std::chrono::system_clock::duration>(
            rhs.toNanoseconds());
        return *this;
    }

    [[nodiscard]] friend Date operator+(Date lhs, const Delay& rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    [[nodiscard]] friend Date operator-(Date lhs, const Delay& rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    [[nodiscard]] friend Delay operator-(const Date& lhs, const Date& rhs) noexcept
    {
        return Delay{std::chrono::duration_cast<std::chrono::nanoseconds>(lhs._timePoint - rhs._timePoint)};
    }

    [[nodiscard]] friend bool operator==(const Date& lhs, const Date& rhs) noexcept
    {
        return lhs._timePoint == rhs._timePoint;
    }

    [[nodiscard]] friend bool operator!=(const Date& lhs, const Date& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend bool operator<(const Date& lhs, const Date& rhs) noexcept
    {
        return lhs._timePoint < rhs._timePoint;
    }

    [[nodiscard]] friend bool operator>(const Date& lhs, const Date& rhs) noexcept
    {
        return rhs < lhs;
    }

    [[nodiscard]] friend bool operator<=(const Date& lhs, const Date& rhs) noexcept
    {
        return !(rhs < lhs);
    }

    [[nodiscard]] friend bool operator>=(const Date& lhs, const Date& rhs) noexcept
    {
        return !(lhs < rhs);
    }

    [[nodiscard]] operator std::chrono::system_clock::time_point() const noexcept
    {
        return _timePoint;
    }

private:
    std::chrono::system_clock::time_point _timePoint;
};

} // namespace util::chrono
