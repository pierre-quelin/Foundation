/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Registry.hpp"
#include "tools/design/ipc/EventBusBoot.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/design/ipc/LinkState.hpp"
#include "tools/design/ipc/TransportLoopback.hpp"
#include "tools/os/serport/ISerport.h"
#include "tools/os/serport/SerportGhost.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::ipc;

namespace
{

[[nodiscard]] ApplicationServices makeApp(const std::string& json)
{
    ApplicationServices app;
    app.logs   = std::make_shared<util::logger::LogService>();
    app.config = createLocalFromString(json);
    return app;
}

} // namespace

BOOST_AUTO_TEST_CASE(eventbus_service_optional_null_by_default)
{
    ApplicationServices app;
    BOOST_CHECK(app.eventBus == nullptr);
}

BOOST_AUTO_TEST_CASE(eventbus_factory_wire_loopback)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "MachineA"
      }
    })";

    auto app = makeApp(json);
    BOOST_CHECK(app.eventBus == nullptr);

    wireEventBus(app, app.config->root()["EventBus"]);

    BOOST_REQUIRE(app.eventBus != nullptr);
    BOOST_CHECK(app.eventBus->linkState() == LinkState::Online);
    BOOST_CHECK(dynamic_cast<TransportLoopback*>(&app.eventBus->transport()) != nullptr);
}

BOOST_AUTO_TEST_CASE(eventbus_alias_mqtt_to_loopback)
{
    Registry::instance().addAlias("tools::design::ipc::TransportByMqtt",
                                  "tools::design::ipc::TransportLoopback");

    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "MachineA",
        "Transport": {
          "InstanceOf": "tools::design::ipc::TransportByMqtt",
          "Broker": "tcp://127.0.0.1:1883",
          "ClientId": "utest-alias"
        }
      }
    })";

    auto app = makeApp(json);
    wireEventBus(app, app.config->root()["EventBus"]);

    BOOST_REQUIRE(app.eventBus != nullptr);
    BOOST_CHECK(dynamic_cast<TransportLoopback*>(&app.eventBus->transport()) != nullptr);

    Registry::instance().clearAliases();
}

BOOST_AUTO_TEST_CASE(eventbus_mqtt_inline_derives_client_id)
{
    Registry::instance().addAlias("tools::design::ipc::TransportByMqtt",
                                  "tools::design::ipc::TransportLoopback");

    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "EsploraPoC",
        "PlatformName": "Local",
        "Broker": "tcp://127.0.0.1:1883"
      }
    })";

    auto app = makeApp(json);
    BOOST_REQUIRE_NO_THROW(wireEventBus(app, app.config->root()["EventBus"]));
    BOOST_REQUIRE(app.eventBus != nullptr);
    BOOST_CHECK(dynamic_cast<TransportLoopback*>(&app.eventBus->transport()) != nullptr);

    Registry::instance().clearAliases();
}

BOOST_AUTO_TEST_CASE(eventbus_mqtt_nested_transport_derives_client_id)
{
    Registry::instance().addAlias("tools::design::ipc::TransportByMqtt",
                                  "tools::design::ipc::TransportLoopback");

    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "EsploraPoC",
        "PlatformName": "Local",
        "Transport": {
          "InstanceOf": "tools::design::ipc::TransportByMqtt",
          "Broker": "tcp://127.0.0.1:1883"
        }
      }
    })";

    auto app = makeApp(json);
    BOOST_REQUIRE_NO_THROW(wireEventBus(app, app.config->root()["EventBus"]));
    BOOST_REQUIRE(app.eventBus != nullptr);
    BOOST_CHECK(dynamic_cast<TransportLoopback*>(&app.eventBus->transport()) != nullptr);
    BOOST_CHECK(app.instances->contains("EventBus.Transport"));

    Registry::instance().clearAliases();
}

BOOST_AUTO_TEST_CASE(eventbus_factory_serport_ghost)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "MachineA"
      },
      "ComPort": {
        "InstanceOf": "tools::os::serport::SerportGhost"
      }
    })";

    auto app = makeApp(json);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto ghost = createShared<tools::os::serport::ISerport>(app, app.config->root()["ComPort"]);
    BOOST_REQUIRE(ghost != nullptr);
    auto* asGhost = dynamic_cast<tools::os::serport::SerportGhost*>(ghost.get());
    BOOST_REQUIRE(asGhost != nullptr);
    asGhost->start();
    BOOST_CHECK(!ghost->isReady());
    asGhost->stop();
}
