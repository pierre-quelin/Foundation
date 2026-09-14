/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Delay.hpp
 * @brief Foundation duration type (Delay) — Phase 0 prelude.
 */
#pragma once

#include <cctype>
#include <charconv>
#include <chrono>
#include <stdexcept>
#include <string_view>

namespace util::chrono
{

/**
 * @brief Duration wrapper for Foundation APIs.
 *
 * Public Phase 0 surface uses Delay instead of raw std::chrono types.
 *
 * Construction examples:
 * @code
 * using namespace util::chrono;
 * using namespace util::chrono::literals;  // preferred
 *
 * Delay a(10_s);
 * Delay b(10_ms);
 * @endcode
 *
 * A @c std::chrono::duration is also accepted (e.g. with @c std::chrono_literals):
 * @code
 * using namespace std::chrono_literals;
 * Delay c(10s);  // works, but prefer 10_s above in Foundation code
 * @endcode
 *
 * Prefer @c util::chrono::literals (@c 10_s, @c 10_ms) over @c std::chrono_literals
 * (@c 10s, @c 10ms) for consistency and to avoid MSVC ambiguities.
 *
 * Config / JSON strings are parsed by @c Delay::parse — suffixes @c ns, @c us, @c ms,
 * @c s, @c min, @c h.
 */
class Delay
{
public:
    template <typename Rep, typename Period>
    constexpr explicit Delay(std::chrono::duration<Rep, Period> d) noexcept
        : _ns(std::chrono::duration_cast<std::chrono::nanoseconds>(d))
    {
    }

    constexpr explicit Delay(std::chrono::nanoseconds ns) noexcept
        : _ns(ns)
    {
    }

    [[nodiscard]] static Delay parse(std::string_view text)
    {
        if (text.empty())
        {
            throw std::invalid_argument("Delay::parse: empty string");
        }

        std::size_t suffixStart = 0;
        while (suffixStart < text.size() && std::isdigit(static_cast<unsigned char>(text[suffixStart])))
        {
            ++suffixStart;
        }

        if (suffixStart == 0)
        {
            throw std::invalid_argument("Delay::parse: expected digits");
        }

        const std::string_view number = text.substr(0, suffixStart);
        unsigned long long value      = 0;
        const auto [end, ec] =
            std::from_chars(number.data(), number.data() + number.size(), value);
        if (ec != std::errc{} || end != number.data() + number.size())
        {
            throw std::invalid_argument("Delay::parse: invalid number");
        }

        const std::string_view suffix = text.substr(suffixStart);
        if (suffix == "ns")
        {
            return Delay{std::chrono::nanoseconds{value}};
        }
        if (suffix == "us")
        {
            return Delay{std::chrono::microseconds{value}};
        }
        if (suffix == "ms")
        {
            return Delay{std::chrono::milliseconds{value}};
        }
        if (suffix == "s")
        {
            return Delay{std::chrono::seconds{value}};
        }
        if (suffix == "min")
        {
            return Delay{std::chrono::minutes{value}};
        }
        if (suffix == "h")
        {
            return Delay{std::chrono::hours{value}};
        }

        throw std::invalid_argument("Delay::parse: unsupported suffix");
    }

    [[nodiscard]] static Delay max() noexcept
    {
        return Delay{std::chrono::nanoseconds::max()};
    }

    [[nodiscard]] constexpr std::chrono::nanoseconds toNanoseconds() const noexcept
    {
        return _ns;
    }

    Delay& operator+=(const Delay& rhs) noexcept
    {
        _ns += rhs._ns;
        return *this;
    }

    [[nodiscard]] constexpr Delay operator-() const noexcept { return Delay{-_ns}; }

    [[nodiscard]] friend constexpr bool operator==(const Delay& lhs, const Delay& rhs) noexcept
    {
        return lhs._ns == rhs._ns;
    }

    [[nodiscard]] friend constexpr bool operator!=(const Delay& lhs, const Delay& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend constexpr bool operator<(const Delay& lhs, const Delay& rhs) noexcept
    {
        return lhs._ns < rhs._ns;
    }

    [[nodiscard]] friend constexpr bool operator>(const Delay& lhs, const Delay& rhs) noexcept
    {
        return rhs < lhs;
    }

    [[nodiscard]] friend constexpr bool operator<=(const Delay& lhs, const Delay& rhs) noexcept
    {
        return !(rhs < lhs);
    }

    [[nodiscard]] friend constexpr bool operator>=(const Delay& lhs, const Delay& rhs) noexcept
    {
        return !(lhs < rhs);
    }

private:
    std::chrono::nanoseconds _ns;
};

[[nodiscard]] inline constexpr Delay operator+(Delay lhs, const Delay& rhs) noexcept
{
    return Delay{lhs.toNanoseconds() + rhs.toNanoseconds()};
}

[[nodiscard]] inline constexpr Delay operator-(Delay lhs, const Delay& rhs) noexcept
{
    return Delay{lhs.toNanoseconds() - rhs.toNanoseconds()};
}

[[nodiscard]] inline constexpr Delay operator*(int scalar, Delay delay) noexcept
{
    return Delay{delay.toNanoseconds() * scalar};
}

namespace literals
{

/** @brief Preferred Foundation duration literals — use @c 10_s, @c 10_ms in new code. */
constexpr Delay operator"" _ms(unsigned long long v)
{
    return Delay{std::chrono::milliseconds{v}};
}

constexpr Delay operator"" _s(unsigned long long v)
{
    return Delay{std::chrono::seconds{v}};
}

} // namespace literals

} // namespace util::chrono
