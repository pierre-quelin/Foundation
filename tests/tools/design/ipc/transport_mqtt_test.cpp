/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/ipc/EventBus.hpp"
#include "tools/design/ipc/MqttTransportOptions.hpp"
#include "tools/design/ipc/TransportByMqtt.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

using namespace tools::design::ipc;
using namespace tools::design::scheduler;
using namespace util::chrono::literals;

namespace
{

[[nodiscard]] const char* mqttBrokerUri()
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    const char* uri = std::getenv("FOUNDATION_TEST_MQTT_BROKER");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    return uri;
}

[[nodiscard]] bool mqttBrokerConfigured()
{
    const char* uri = mqttBrokerUri();
    return uri != nullptr && uri[0] != '\0';
}

void skipWithoutBroker()
{
    if (!mqttBrokerConfigured())
    {
        BOOST_TEST_MESSAGE(
            "skip: set FOUNDATION_TEST_MQTT_BROKER (e.g. tcp://127.0.0.1:1883) "
            "and run a local Mosquitto broker");
    }
}

[[nodiscard]] bool requireBrokerOrSkip()
{
    if (mqttBrokerConfigured())
    {
        return true;
    }
    skipWithoutBroker();
    BOOST_CHECK(true); // skipped — no broker
    return false;
}

[[nodiscard]] std::string uniqueTopic(const char* suffix)
{
    using clock = std::chrono::steady_clock;
    const auto tick =
        std::chrono::duration_cast<std::chrono::microseconds>(clock::now().time_since_epoch())
            .count();
    return std::string("foundation/utest/") + std::to_string(tick) + "/" + suffix;
}

} // namespace

BOOST_AUTO_TEST_CASE(transport_mqtt_connect_disconnect)
{
    if (!requireBrokerOrSkip())
    {
        return;
    }

    MqttTransportOptions opts;
    opts.brokerUri = mqttBrokerUri();
    opts.clientId  = "foundation_utest_connect";

    TransportByMqtt transport(opts);
    BOOST_CHECK(!transport.isConnected());

    transport.connect();
    BOOST_CHECK(transport.isConnected());

    transport.disconnect();
    BOOST_CHECK(!transport.isConnected());
}

BOOST_AUTO_TEST_CASE(transport_mqtt_subscribe_publish_roundtrip)
{
    if (!requireBrokerOrSkip())
    {
        return;
    }

    const std::string topic = uniqueTopic("roundtrip");

    MqttTransportOptions opts;
    opts.brokerUri = mqttBrokerUri();
    opts.clientId  = "foundation_utest_roundtrip";

    TransportByMqtt transport(opts);
    transport.connect();

    std::atomic<int> hits{0};
    std::string seenTopic;
    std::string seenPayload;
    const auto subId =
        transport.subscribe(topic, [&](const Envelope& env)
                            {
            seenTopic   = env.topic;
            seenPayload = env.payload;
            hits.fetch_add(1); });

    // Allow broker to register the subscription before publish.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Envelope out;
    out.topic   = topic;
    out.payload = "ping-mqtt";
    transport.publish(out);

    for (int i = 0; i < 50 && hits.load() == 0; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    BOOST_CHECK_EQUAL(hits.load(), 1);
    BOOST_CHECK_EQUAL(seenTopic, topic);
    BOOST_CHECK_EQUAL(seenPayload, "ping-mqtt");

    transport.unsubscribe(subId);
    transport.disconnect();
}

BOOST_AUTO_TEST_CASE(transport_mqtt_event_bus_short)
{
    if (!requireBrokerOrSkip())
    {
        return;
    }

    MqttTransportOptions optsA;
    optsA.brokerUri = mqttBrokerUri();
    optsA.clientId  = "foundation_utest_bus_a";

    MqttTransportOptions optsB;
    optsB.brokerUri = mqttBrokerUri();
    optsB.clientId  = "foundation_utest_bus_b";

    TransportByMqtt transportA(optsA);
    TransportByMqtt transportB(optsB);
    transportA.connect();
    transportB.connect();

    auto schedA = std::make_shared<EventScheduler>(256, "MqttBusA");
    auto schedB = std::make_shared<EventScheduler>(256, "MqttBusB");
    schedA->start();
    schedB->start();

    TopicScheme topics{"Sample"};
    EventBus busA{transportA, topics, "MachineA", schedA};
    EventBus busB{transportB, topics, "MachineB", schedB};
    busA.start();
    busB.start();

    BOOST_REQUIRE(schedA->synchronise(200_ms));
    BOOST_REQUIRE(schedB->synchronise(200_ms));

    std::atomic<int> hits{0};
    std::string seen;
    (void)busB.subscribe("Board.Device", "tick", [&](const EventId& id, const std::string& payload)
                         {
                             BOOST_CHECK_EQUAL(id.srcPlatform, "MachineA");
                             seen = payload;
                             hits.fetch_add(1); });

    BOOST_REQUIRE(schedB->synchronise(200_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    busA.publish("Board.Device", "tick", "hello-mqtt");
    BOOST_REQUIRE(schedA->synchronise(200_ms));

    for (int i = 0; i < 50 && hits.load() == 0; ++i)
    {
        (void)schedB->synchronise(50_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    BOOST_CHECK_EQUAL(hits.load(), 1);
    BOOST_CHECK_EQUAL(seen, "hello-mqtt");

    busA.stop();
    busB.stop();
    (void)schedA->synchronise(200_ms);
    (void)schedB->synchronise(200_ms);
    transportA.disconnect();
    transportB.disconnect();
    schedA->quit();
    schedB->quit();
}
