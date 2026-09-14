/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/out/IOut.h"
#include "io/out/OutGhost.hpp"
#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/EventBusBoot.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <memory>
#include <thread>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::ipc;
using namespace util::chrono::literals;

namespace io::out::test
{

class FakeOut : public factory::IObject, public IOut
{
public:
    FakeOut(ApplicationServices& /*app*/, Node /*node*/) {}

    int init() override
    {
        _inited = true;
        return 0;
    }

    int set(unsigned int value) override
    {
        _value = value;
        return 0;
    }

    int get(unsigned int& value) const override
    {
        value = _value;
        return 0;
    }

    [[nodiscard]] unsigned int raw() const { return _value; }
    [[nodiscard]] bool inited() const { return _inited; }

private:
    unsigned int _value = 0;
    bool _inited        = false;
};

} // namespace io::out::test

FOUNDATION_FACTORY_REGISTER(io::out::test::FakeOut, "io::out::test::FakeOut", io_out_test_FakeOut)

namespace
{

[[nodiscard]] ApplicationServices makeApp(const std::string& json)
{
    ApplicationServices app;
    app.logs   = std::make_shared<util::logger::LogService>();
    app.config = createLocalFromString(json);
    return app;
}

constexpr const char* SetGetJson = R"({
  "EventBus": {
    "InstanceOf": "tools::design::ipc::EventBus",
    "AppName": "Sample",
    "PlatformName": "MachineA"
  },
  "Board": {
    "Objects": {
      "relay1": {
        "InstanceOf": "io::out::test::FakeOut",
        "Bridged": "io::out::OutBridge"
      }
    }
  }
})";

} // namespace

BOOST_AUTO_TEST_CASE(out_ghost_bridge_set_get)
{
    auto app = makeApp(SetGetJson);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto device = createShared<io::out::IOut>(app, app.config->root()["Board"], "relay1");
    auto ghost =
        createAs<io::out::OutGhost>(app, app.config->root()["Board"]["Objects"]["relay1"], "io::out::OutGhost");
    auto* fake = dynamic_cast<io::out::test::FakeOut*>(device.get());
    BOOST_REQUIRE(fake != nullptr);
    BOOST_REQUIRE(app.instances);
    BOOST_CHECK(app.instances->contains("Board.relay1"));
    BOOST_CHECK(app.instances->contains(derivedInstanceKey("Board.relay1", "bridge")));

    ghost->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    BOOST_CHECK_EQUAL(ghost->init(), 0);
    BOOST_CHECK(fake->inited());
    BOOST_CHECK_EQUAL(ghost->set(1), 0);

    for (int i = 0; i < 40 && fake->raw() != 1u; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    BOOST_CHECK_EQUAL(fake->raw(), 1u);

    unsigned int got = 0;
    BOOST_CHECK_EQUAL(ghost->get(got), 0);
    BOOST_CHECK_EQUAL(got, 1u);

    ghost->stop();
}

BOOST_AUTO_TEST_CASE(out_ghost_offline_set_fails)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "Sample",
        "PlatformName": "MachineA"
      },
      "relay1Ghost": {
        "InstanceOf": "io::out::OutGhost"
      }
    })";

    auto app = makeApp(json);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto ghost = createShared<io::out::OutGhost>(app, app.config->root()["relay1Ghost"]);
    ghost->start();
    app.eventBus->stop();
    BOOST_CHECK_EQUAL(ghost->set(1), -1);
    ghost->stop();
}
