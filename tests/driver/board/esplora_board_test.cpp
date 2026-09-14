/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "driver/board/EsploraBoard.h"
#include "driver/board/EsploraLightSensor.h"
#include "driver/board/EsploraSwitch.h"
#include "io/board/IBoard.h"
#include "io/in/IIn.h"
#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/ipc/EventBusBoot.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>

using namespace driver::board;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::ipc;

namespace
{

[[nodiscard]] ApplicationServices makeApp(const std::string& json, const std::string& platformName)
{
    ApplicationServices app;
    app.logs         = std::make_shared<util::logger::LogService>();
    app.config       = createLocalFromString(json);
    app.platformName = platformName;
    return app;
}

} // namespace

BOOST_AUTO_TEST_CASE(esplora_board_creates_fixed_io)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "Local"
      },
      "EsploraBoard": {
        "InstanceOf": "driver::board::EsploraBoard",
        "LogLevel": "DEBUG"
      }
    })";

    auto app = makeApp(json, "Local");
    wireEventBus(app, app.config->root()["EventBus"]);

    auto board = createShared<io::board::IBoard>(app, app.config->root()["EsploraBoard"]);
    BOOST_REQUIRE(board != nullptr);
    auto* esplora = dynamic_cast<EsploraBoard*>(board.get());
    BOOST_REQUIRE(esplora != nullptr);
    esplora->launch();

    BOOST_CHECK_EQUAL(static_cast<int>(board->state()), static_cast<int>(io::board::IBoard::State::Ready));
    BOOST_REQUIRE(board->getInput("switch1") != nullptr);
    BOOST_REQUIRE(board->getInput("switch4") != nullptr);
    BOOST_REQUIRE(board->getRGBWLed("rgbLed") != nullptr);
    BOOST_REQUIRE(board->getADC("lightSensor") != nullptr);
    BOOST_CHECK(board->getOutput("x") == nullptr);

    auto* sw = dynamic_cast<EsploraSwitch*>(board->getInput("switch1").get());
    BOOST_REQUIRE(sw != nullptr);
    sw->push(1);
    unsigned int v = 0;
    BOOST_CHECK_EQUAL(sw->get(v), 0);
    BOOST_CHECK_EQUAL(v, 1u);
}

BOOST_AUTO_TEST_CASE(esplora_board_push_io)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "Local"
      },
      "EsploraBoard": {
        "InstanceOf": "driver::board::EsploraBoard",
        "LogLevel": "DEBUG"
      }
    })";

    auto app = makeApp(json, "Local");
    wireEventBus(app, app.config->root()["EventBus"]);

    auto board    = createShared<io::board::IBoard>(app, app.config->root()["EsploraBoard"]);
    auto* esplora = dynamic_cast<EsploraBoard*>(board.get());
    BOOST_REQUIRE(esplora != nullptr);
    esplora->launch();

    auto* sw2   = dynamic_cast<EsploraSwitch*>(board->getInput("switch2").get());
    auto* sw4   = dynamic_cast<EsploraSwitch*>(board->getInput("switch4").get());
    auto* light = dynamic_cast<EsploraLightSensor*>(board->getADC("lightSensor").get());
    BOOST_REQUIRE(sw2 != nullptr);
    BOOST_REQUIRE(sw4 != nullptr);
    BOOST_REQUIRE(light != nullptr);

    sw2->push(1);
    sw4->push(1);
    light->push(1234.);

    unsigned int v2 = 0;
    unsigned int v4 = 0;
    BOOST_REQUIRE(sw2->get(v2) == 0);
    BOOST_REQUIRE(sw4->get(v4) == 0);
    BOOST_CHECK_EQUAL(v2, 1u);
    BOOST_CHECK_EQUAL(v4, 1u);

    double lightVal = 0.;
    BOOST_REQUIRE(light->get(lightVal) == 0);
    BOOST_CHECK_EQUAL(lightVal, 1234.);
}

BOOST_AUTO_TEST_CASE(esplora_board_bridged_overlay)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "Local"
      },
      "EsploraBoard": {
        "InstanceOf": "driver::board::EsploraBoard",
        "Objects": {
          "switch1": {
            "Bridged": "io::in::InBridge"
          }
        }
      }
    })";

    auto app = makeApp(json, "Local");
    wireEventBus(app, app.config->root()["EventBus"]);

    auto board = createShared<io::board::IBoard>(app, app.config->root()["EsploraBoard"]);
    BOOST_REQUIRE(board != nullptr);
    BOOST_REQUIRE(app.instances);
    BOOST_CHECK(app.instances->contains("EsploraBoard.switch1"));
    BOOST_CHECK(app.instances->contains("EsploraBoard.switch1#bridge"));
}

BOOST_AUTO_TEST_CASE(esplora_switch1_ghost_remote)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "utest",
        "PlatformName": "Remote"
      },
      "EsploraBoard": {
        "Objects": {
          "switch1": {
            "InstanceOf": "io::in::InGhost"
          }
        }
      }
    })";

    auto app = makeApp(json, "Remote");
    wireEventBus(app, app.config->root()["EventBus"]);

    auto ghost = createShared<io::in::IIn>(app, app.config->root()["EsploraBoard"], "switch1");
    BOOST_REQUIRE(ghost != nullptr);
    BOOST_CHECK(app.instances->contains("EsploraBoard.switch1"));
}
