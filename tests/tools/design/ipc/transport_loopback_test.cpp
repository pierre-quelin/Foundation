/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/ipc/TransportLoopback.hpp"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

using namespace tools::design::ipc;

BOOST_AUTO_TEST_CASE(transport_loopback_publish_subscribe)
{
    TransportLoopback transport;
    transport.connect();
    BOOST_CHECK(transport.isConnected());

    std::vector<std::string> payloads;
    const auto id = transport.subscribe(
        "foundation/Sample/evt/+/+/tick", [&](const Envelope& env)
        { payloads.push_back(env.payload); });

    Envelope env;
    env.topic   = "foundation/Sample/evt/MachineA/Board/tick";
    env.payload = "hello";
    transport.publish(env);

    BOOST_REQUIRE_EQUAL(payloads.size(), 1u);
    BOOST_CHECK_EQUAL(payloads[0], "hello");

    transport.unsubscribe(id);
    transport.publish(env);
    BOOST_CHECK_EQUAL(payloads.size(), 1u);
}

BOOST_AUTO_TEST_CASE(transport_loopback_hash_wildcard)
{
    TransportLoopback transport;
    transport.connect();

    int hits = 0;
    (void)transport.subscribe("foundation/Sample/#", [&](const Envelope&)
                              { ++hits; });

    Envelope env;
    env.topic   = "foundation/Sample/domain/Obj/rx";
    env.payload = "bin";
    transport.publish(env);
    BOOST_CHECK_EQUAL(hits, 1);
}

BOOST_AUTO_TEST_CASE(transport_loopback_disconnect_stops_delivery)
{
    TransportLoopback transport;
    transport.connect();

    int hits = 0;
    (void)transport.subscribe("a/b", [&](const Envelope&)
                              { ++hits; });

    Envelope env;
    env.topic = "a/b";
    transport.publish(env);
    BOOST_CHECK_EQUAL(hits, 1);

    transport.disconnect();
    BOOST_CHECK(!transport.isConnected());
    BOOST_CHECK_THROW(transport.publish(env), std::runtime_error);
    BOOST_CHECK_EQUAL(hits, 1);
}

BOOST_AUTO_TEST_CASE(transport_loopback_retained_replay)
{
    TransportLoopback transport;
    transport.connect();

    Envelope retained;
    retained.topic    = "foundation/Sample/platforms/MachineB/status";
    retained.payload  = "online";
    retained.retained = true;
    transport.publish(retained);

    std::string seen;
    (void)transport.subscribe("foundation/Sample/platforms/+/status",
                              [&](const Envelope& env)
                              { seen = env.payload; });
    BOOST_CHECK_EQUAL(seen, "online");
}
