/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/in/IIn.h"
#include "io/in/InGhost.hpp"
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

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::ipc;
using namespace util::chrono::literals;

namespace io::in::test
{

class FakeIn : public factory::IObject, public IIn
{
public:
    FakeIn(ApplicationServices& /*app*/, Node /*node*/) {}

    int init() override
    {
        _inited = true;
        return 0;
    }

    int get(unsigned int& value) const override
    {
        value = _value;
        return 0;
    }

    void push(unsigned int v)
    {
        _value = v;
        valueChanged(v);
    }

    [[nodiscard]] bool inited() const { return _inited; }

private:
    unsigned int _value = 0;
    bool _inited        = false;
};

} // namespace io::in::test

FOUNDATION_FACTORY_REGISTER(io::in::test::FakeIn, "io::in::test::FakeIn", io_in_test_FakeIn)

namespace
{

[[nodiscard]] ApplicationServices makeApp(const std::string& json)
{
    ApplicationServices app;
    app.logs   = std::make_shared<util::logger::LogService>();
    app.config = createLocalFromString(json);
    return app;
}

constexpr const char* ValueJson = R"({
  "EventBus": {
    "InstanceOf": "tools::design::ipc::EventBus",
    "AppName": "Sample",
    "PlatformName": "MachineA"
  },
  "Board": {
    "Objects": {
      "pres24v": {
        "InstanceOf": "io::in::test::FakeIn",
        "Bridged": "io::in::InBridge"
      }
    }
  }
})";

} // namespace

BOOST_AUTO_TEST_CASE(in_ghost_bridge_value_changed)
{
    auto app = makeApp(ValueJson);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto device = createShared<io::in::IIn>(app, app.config->root()["Board"], "pres24v");
    auto ghost =
        createAs<io::in::InGhost>(app, app.config->root()["Board"]["Objects"]["pres24v"], "io::in::InGhost");
    auto* fake = dynamic_cast<io::in::test::FakeIn*>(device.get());
    BOOST_REQUIRE(fake != nullptr);
    BOOST_REQUIRE(app.instances);
    BOOST_CHECK(app.instances->contains("Board.pres24v"));
    BOOST_CHECK(app.instances->contains(derivedInstanceKey("Board.pres24v", "bridge")));

    std::atomic<int> hits{0};
    unsigned int seen = 0;
    auto conn         = ghost->valueChanged.connect([&](unsigned int v)
                                                    {
        seen = v;
        hits.fetch_add(1); });

    ghost->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    BOOST_CHECK_EQUAL(ghost->init(), 0);
    BOOST_CHECK(fake->inited());

    hits.store(0);
    fake->push(42);
    for (int i = 0; i < 40 && hits.load() == 0; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    BOOST_CHECK_EQUAL(hits.load(), 1);
    BOOST_CHECK_EQUAL(seen, 42u);

    unsigned int got = 0;
    BOOST_CHECK_EQUAL(ghost->get(got), 0);
    BOOST_CHECK_EQUAL(got, 42u);

    ghost->stop();
    (void)conn;
}

BOOST_AUTO_TEST_CASE(in_ghost_offline_get_fails)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "Sample",
        "PlatformName": "MachineA"
      },
      "pres24vGhost": {
        "InstanceOf": "io::in::InGhost"
      }
    })";

    auto app = makeApp(json);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto ghost = createShared<io::in::InGhost>(app, app.config->root()["pres24vGhost"]);
    ghost->start();
    app.eventBus->stop();
    unsigned int v = 0;
    BOOST_CHECK_EQUAL(ghost->get(v), -1);
    ghost->stop();
}
