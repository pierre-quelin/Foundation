/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/objkit/ObjKit.hpp"

#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/scheduler/SchedulerService.hpp"
#include "tools/os/thread/ThreadPolicy.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <string>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::objkit;
using namespace tools::design::scheduler;
using namespace tools::os::thread;

namespace tools::design::test
{

class KitComponent : public ObjKit
{
public:
    KitComponent(ApplicationServices& app, Node node) : ObjKit(app, node)
    {
        needLogger();
        needScheduler();
    }

    [[nodiscard]] std::string configPath() const
    {
        return config().path();
    }

    [[nodiscard]] std::string instanceName() const
    {
        return shortName();
    }
};

} // namespace tools::design::test

FOUNDATION_FACTORY_REGISTER(tools::design::test::KitComponent,
                            "tools::design::test::KitComponent",
                            tools_design_test_KitComponent)

BOOST_AUTO_TEST_CASE(objkit_exposes_config_and_logger)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1, "MaxEventCount": 128 },
      "Component": {
        "InstanceOf": "tools::design::test::KitComponent",
        "LogLevel": "DEBUG"
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    auto component = create<test::KitComponent>(app, center->root()["Component"]);
    BOOST_REQUIRE(component);
    BOOST_CHECK_EQUAL(component->configPath(), "Component");
    BOOST_CHECK_EQUAL(component->instanceName(), "Component");
    BOOST_CHECK_EQUAL(component->scheduler().maxPending(), 128u);

    const auto names = app.logs->registeredLoggerNames();
    BOOST_CHECK(std::find(names.begin(), names.end(), "Component") != names.end());
    reset();
}

BOOST_AUTO_TEST_CASE(objkit_need_logger_uses_instance_short_name)
{
    constexpr const char* json = R"({
      "Board": {
        "Device": {
          "InstanceOf": "tools::design::test::KitComponent"
        }
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    struct LocalKit : ObjKit
    {
        using ObjKit::ObjKit;
        void requireLogger() { needLogger(); }
        using ObjKit::logger;
        using ObjKit::shortName;
    };

    LocalKit kit(app, center->root()["Board"]["Device"]);
    BOOST_CHECK_THROW((void)kit.logger(), std::logic_error);
    kit.requireLogger();
    BOOST_CHECK_EQUAL(kit.shortName(), "Device");
    BOOST_CHECK_NO_THROW((void)kit.logger());

    const auto names = app.logs->registeredLoggerNames();
    BOOST_CHECK(std::find(names.begin(), names.end(), "Device") != names.end());
    reset();
}

BOOST_AUTO_TEST_CASE(objkit_need_logger_uniquifies_with_tilde_index)
{
    constexpr const char* json = R"({
      "Left": {
        "Device": {
          "InstanceOf": "tools::design::test::KitComponent"
        }
      },
      "Right": {
        "Device": {
          "InstanceOf": "tools::design::test::KitComponent"
        }
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    struct LocalKit : ObjKit
    {
        using ObjKit::ObjKit;
        void requireLogger() { needLogger(); }
        using ObjKit::logger;
        using ObjKit::shortName;
    };

    LocalKit left(app, center->root()["Left"]["Device"]);
    LocalKit right(app, center->root()["Right"]["Device"]);
    left.requireLogger();
    right.requireLogger();

    BOOST_CHECK_EQUAL(left.shortName(), "Device");
    BOOST_CHECK_EQUAL(right.shortName(), "Device");
    BOOST_CHECK_EQUAL(left.logger().name(), "Device");
    BOOST_CHECK_EQUAL(right.logger().name(), "Device~1");
    reset();
}

BOOST_AUTO_TEST_CASE(objkit_need_scheduler_checks_max_event_count)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1, "MaxEventCount": 8 },
      "Component": {
        "InstanceOf": "tools::design::test::KitComponent"
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    struct LocalKit : ObjKit
    {
        using ObjKit::ObjKit;
        void requireScheduler() { needScheduler(64); }
    };

    const auto node = center->root()["Component"];
    LocalKit kit(app, node);
    BOOST_CHECK_THROW(kit.requireScheduler(), std::runtime_error);
    reset();
}

BOOST_AUTO_TEST_CASE(objkit_need_state_machine_requires_scheduler)
{
    constexpr const char* json = R"({
      "Component": {
        "InstanceOf": "tools::design::test::KitComponent"
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    struct DummySm
    {
        void start() {}
    };

    struct SmPilot : ObjKit
    {
        using ObjKit::ObjKit;
        std::unique_ptr<DummySm> sm;

        void wire()
        {
            needStateMachine(sm);
            startStateMachine(*sm);
        }
    };

    SmPilot pilot(app, center->root()["Component"]);
    BOOST_REQUIRE(pilot.sm == nullptr);
    pilot.wire();
    BOOST_REQUIRE(pilot.sm != nullptr);
    reset();
}

BOOST_AUTO_TEST_CASE(objkit_need_scheduler_code_first_shared_without_event_scheduler_key)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1 },
      "A": { "InstanceOf": "tools::design::test::KitComponent" },
      "B": { "InstanceOf": "tools::design::test::KitComponent" }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    auto a = create<test::KitComponent>(app, center->root()["A"]);
    auto b = create<test::KitComponent>(app, center->root()["B"]);
    BOOST_REQUIRE(a);
    BOOST_REQUIRE(b);
    BOOST_CHECK(&a->scheduler() == &b->scheduler());
    reset();
}
