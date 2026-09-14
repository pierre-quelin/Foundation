/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "util/chrono/Date.hpp"
#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace util::chrono;
using namespace util::chrono::literals;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_CASE(delay_literal_10ms)
{
    BOOST_CHECK_EQUAL(Delay{10_ms}.toNanoseconds().count(), std::chrono::nanoseconds(10ms).count());
}

BOOST_AUTO_TEST_CASE(delay_literal_1000s)
{
    BOOST_CHECK_EQUAL(Delay{1000_s}.toNanoseconds().count(), std::chrono::nanoseconds(1000s).count());
}

BOOST_AUTO_TEST_CASE(delay_parse_10ms)
{
    BOOST_CHECK_EQUAL(Delay::parse("10ms").toNanoseconds().count(),
                      std::chrono::nanoseconds(10ms).count());
}

BOOST_AUTO_TEST_CASE(delay_parse_1000s)
{
    BOOST_CHECK_EQUAL(Delay::parse("1000s").toNanoseconds().count(),
                      std::chrono::nanoseconds(1000s).count());
}

BOOST_AUTO_TEST_CASE(delay_parse_suffixes)
{
    BOOST_CHECK_EQUAL(Delay::parse("500ns").toNanoseconds().count(), 500);
    BOOST_CHECK_EQUAL(Delay::parse("250us").toNanoseconds().count(),
                      std::chrono::nanoseconds(250us).count());
    BOOST_CHECK_EQUAL(Delay::parse("2min").toNanoseconds().count(),
                      std::chrono::nanoseconds(2min).count());
    BOOST_CHECK_EQUAL(Delay::parse("1h").toNanoseconds().count(),
                      std::chrono::nanoseconds(1h).count());
}

BOOST_AUTO_TEST_CASE(delay_parse_invalid_throws)
{
    BOOST_CHECK_THROW((void)Delay::parse("bogus"), std::invalid_argument);
    BOOST_CHECK_THROW((void)Delay::parse(""), std::invalid_argument);
    BOOST_CHECK_THROW((void)Delay::parse("10"), std::invalid_argument);
    BOOST_CHECK_THROW((void)Delay::parse("ms"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(date_since_known_interval)
{
    using clock    = std::chrono::system_clock;
    const auto tp0 = clock::now();
    const auto tp1 = tp0 + std::chrono::milliseconds{25};

    const Date earlier{tp0};
    const Date later{tp1};

    BOOST_CHECK_EQUAL(later.since(earlier).toNanoseconds().count(),
                      std::chrono::nanoseconds(std::chrono::milliseconds{25}).count());
}

BOOST_AUTO_TEST_CASE(date_now_advances)
{
    const Date before = Date::now();
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    const Date after = Date::now();

    BOOST_CHECK(after.since(before).toNanoseconds().count() > 0);
}

BOOST_AUTO_TEST_CASE(delay_arithmetic_and_comparison)
{
    using namespace std::chrono_literals;

    const Delay a{10_ms};
    const Delay b{5_ms};

    BOOST_CHECK_EQUAL((a + b).toNanoseconds().count(), std::chrono::nanoseconds(15ms).count());
    BOOST_CHECK_EQUAL((a - b).toNanoseconds().count(), std::chrono::nanoseconds(5ms).count());
    BOOST_CHECK_EQUAL((-b).toNanoseconds().count(), std::chrono::nanoseconds(-5ms).count());
    BOOST_CHECK_EQUAL((2 * a).toNanoseconds().count(), std::chrono::nanoseconds(20ms).count());

    BOOST_CHECK(a > b);
    BOOST_CHECK(b < a);
    BOOST_CHECK(a >= b);
    BOOST_CHECK(b <= a);
    BOOST_CHECK(a != b);
}

BOOST_AUTO_TEST_CASE(date_arithmetic_and_comparison)
{
    using clock    = std::chrono::system_clock;
    const auto tp0 = clock::now();
    const Date d0{tp0};
    const Date d1{tp0 + std::chrono::milliseconds{30}};

    const Delay delta{25_ms};
    BOOST_CHECK((d0 + delta).since(d0).toNanoseconds().count() == std::chrono::nanoseconds(25ms).count());
    BOOST_CHECK((d1 - delta).since(d0).toNanoseconds().count() == std::chrono::nanoseconds(5ms).count());
    BOOST_CHECK((d1 - d0).toNanoseconds().count() == std::chrono::nanoseconds(30ms).count());

    BOOST_CHECK(d0 < d1);
    BOOST_CHECK(d1 > d0);
    BOOST_CHECK(d0 != d1);
}
