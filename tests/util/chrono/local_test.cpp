/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "util/chrono/Local.hpp"

#include "util/chrono/Date.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cmath>

using namespace util::chrono;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_CASE(local_round_trip_preserves_utc_instant)
{
    const Date original = Date::now();
    const auto local    = toLocal(original);
    const Date rebuilt  = fromLocal(local);

    const auto delta = (rebuilt - original).toNanoseconds().count();
    BOOST_CHECK(std::abs(delta) < std::chrono::nanoseconds(2s).count());
}

BOOST_AUTO_TEST_CASE(local_parts_fields_in_range)
{
    const Date date        = Date::now();
    const LocalParts parts = toLocal(date);

    BOOST_CHECK(parts.year >= 1970);
    BOOST_CHECK(parts.month >= 1 && parts.month <= 12);
    BOOST_CHECK(parts.day >= 1 && parts.day <= 31);
    BOOST_CHECK(parts.hour >= 0 && parts.hour <= 23);
    BOOST_CHECK(parts.minute >= 0 && parts.minute <= 59);
    BOOST_CHECK(parts.second >= 0 && parts.second <= 59);
}

BOOST_AUTO_TEST_CASE(from_local_known_fields)
{
    LocalParts parts;
    parts.year   = 2024;
    parts.month  = 1;
    parts.day    = 15;
    parts.hour   = 12;
    parts.minute = 30;
    parts.second = 0;
    parts.isDst  = false;

    const Date date  = fromLocal(parts);
    const auto round = toLocal(date);

    BOOST_CHECK_EQUAL(round.year, parts.year);
    BOOST_CHECK_EQUAL(round.month, parts.month);
    BOOST_CHECK_EQUAL(round.day, parts.day);
    BOOST_CHECK_EQUAL(round.minute, parts.minute);
}
