/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/ipc/TopicScheme.hpp"

#include <boost/test/unit_test.hpp>

using namespace tools::design::ipc;

BOOST_AUTO_TEST_CASE(topic_scheme_event_roundtrip)
{
    TopicScheme scheme("Sample");
    const EventId id{"MachineA", "Board.Device", "tick"};
    const auto topic = scheme.eventTopic(id);
    BOOST_CHECK_EQUAL(topic, "foundation/Sample/evt/MachineA/Board.Device/tick");

    const auto parsed = scheme.parseEvent(topic);
    BOOST_REQUIRE(parsed.has_value());
    BOOST_CHECK_EQUAL(parsed->srcPlatform, "MachineA");
    BOOST_CHECK_EQUAL(parsed->objectPath, "Board.Device");
    BOOST_CHECK_EQUAL(parsed->event, "tick");
}

BOOST_AUTO_TEST_CASE(topic_scheme_platform_status_roundtrip)
{
    TopicScheme scheme("Sample");
    const auto topic = scheme.platformStatusTopic("MachineB");
    BOOST_CHECK_EQUAL(topic, "foundation/Sample/platforms/MachineB/status");

    const auto platform = scheme.parsePlatformStatus(topic);
    BOOST_REQUIRE(platform.has_value());
    BOOST_CHECK_EQUAL(*platform, "MachineB");
}

BOOST_AUTO_TEST_CASE(topic_scheme_app_topic_generic)
{
    TopicScheme scheme("Sample");
    // Domain packages (e.g. remote port) build their own trees via appTopic.
    const auto topic = scheme.appTopic({"domain", "Obj", "tx"});
    BOOST_CHECK_EQUAL(topic, "foundation/Sample/domain/Obj/tx");

    const auto levels = scheme.parseAppTopic(topic);
    BOOST_REQUIRE(levels.has_value());
    BOOST_REQUIRE_EQUAL(levels->size(), 3u);
    BOOST_CHECK_EQUAL((*levels)[0], "domain");
    BOOST_CHECK_EQUAL((*levels)[1], "Obj");
    BOOST_CHECK_EQUAL((*levels)[2], "tx");
}

BOOST_AUTO_TEST_CASE(topic_scheme_rejects_wrong_app)
{
    TopicScheme scheme("Sample");
    BOOST_CHECK(!scheme.parseEvent("foundation/Other/evt/A/Obj/e").has_value());
    BOOST_CHECK(!scheme.parsePlatformStatus("foundation/Other/platforms/A/status").has_value());
    BOOST_CHECK(!scheme.parseAppTopic("foundation/Other/domain/x").has_value());
}
