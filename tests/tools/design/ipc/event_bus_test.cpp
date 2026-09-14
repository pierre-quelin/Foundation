/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/ipc/EventBus.hpp"
#include "tools/design/ipc/IpcException.hpp"
#include "tools/design/ipc/TransportLoopback.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <memory>
#include <string>

using namespace tools::design::ipc;
using namespace tools::design::scheduler;
using namespace util::chrono::literals;

namespace
{

struct DualBus
{
    TransportLoopback transport;
    std::shared_ptr<EventScheduler> schedA =
        std::make_shared<EventScheduler>(256, "EvtBusA");
    std::shared_ptr<EventScheduler> schedB =
        std::make_shared<EventScheduler>(256, "EvtBusB");
    TopicScheme topics{"Sample"};
    EventBus busA{transport, topics, "MachineA", schedA};
    EventBus busB{transport, topics, "MachineB", schedB};

    DualBus()
    {
        schedA->start();
        schedB->start();
    }

    ~DualBus()
    {
        drain();
        busA.stop();
        busB.stop();
        drain();
        transport.disconnect();
        schedA->quit();
        schedB->quit();
    }

    void drain()
    {
        BOOST_REQUIRE(schedA->synchronise(200_ms));
        BOOST_REQUIRE(schedB->synchronise(200_ms));
    }
};

} // namespace

BOOST_AUTO_TEST_CASE(event_bus_publish_subscribe)
{
    DualBus env;
    env.busA.start();
    env.busB.start();
    env.drain();

    std::atomic<int> hits{0};
    std::string seen;
    (void)env.busB.subscribe("Board.Device", "tick", [&](const EventId& id, const std::string& payload)
                             {
                                 BOOST_CHECK_EQUAL(id.srcPlatform, "MachineA");
                                 seen = payload;
                                 hits.fetch_add(1); });

    env.busA.publish("Board.Device", "tick", "hello");
    env.drain();

    BOOST_CHECK_EQUAL(hits.load(), 1);
    BOOST_CHECK_EQUAL(seen, "hello");
}

BOOST_AUTO_TEST_CASE(event_bus_synchronize_peer_online)
{
    DualBus env;
    env.busA.start();
    env.busB.start();
    env.drain();

    env.busA.synchronize({"MachineB"}, 500_ms);
    BOOST_CHECK(env.busA.peerOnline("MachineB"));
    BOOST_CHECK(env.busB.peerOnline("MachineA"));
    BOOST_CHECK_EQUAL(static_cast<int>(env.busA.linkState()),
                      static_cast<int>(LinkState::Online));
}

BOOST_AUTO_TEST_CASE(event_bus_offline_publish_throws)
{
    DualBus env;
    BOOST_CHECK_EQUAL(static_cast<int>(env.busA.linkState()),
                      static_cast<int>(LinkState::Offline));
    BOOST_CHECK_THROW(env.busA.publish("x", "y", "z"), IpcException);

    env.busA.start();
    env.drain();
    env.transport.disconnect();
    BOOST_CHECK_THROW(env.busA.publish("x", "y", "z"), IpcException);
}

BOOST_AUTO_TEST_CASE(event_bus_request_reply)
{
    DualBus env;
    env.busA.start();
    env.busB.start();
    env.drain();
    env.busA.synchronize({"MachineB"}, 500_ms);

    env.busB.onRequest("Service", "echo", [](const EventId&, const std::string& payload)
                       { return "echo:" + payload; });

    const std::string reply = env.busA.request("Service", "echo", "ping", 500_ms);
    env.drain();
    BOOST_CHECK_EQUAL(reply, "echo:ping");
}

BOOST_AUTO_TEST_CASE(event_bus_request_timeout)
{
    DualBus env;
    env.busA.start();
    env.busB.start();
    env.drain();

    BOOST_CHECK_THROW((void)env.busA.request("Service", "none", "x", 50_ms), IpcException);
}
