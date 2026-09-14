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
#include "tools/design/factory/Factory.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/design/time/SimpleTimeManager.hpp"
#include "tools/design/time/TimeManagerByTimer.hpp"
#include "tools/design/time/TimeRequest.hpp"
#include "tools/os/timer/Timer.h"
#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

using namespace tools::design;
using namespace tools::design::factory;
using namespace tools::design::scheduler;
using namespace tools::design::time;
using namespace util::chrono;
using namespace util::chrono::literals;
using namespace tools::os::timer;

namespace
{

struct Handler
{
    void onTimeout() { fired.store(true); }

    std::atomic<bool> fired{false};
};

} // namespace

BOOST_AUTO_TEST_CASE(time_request_set_target_fires_on_scheduler)
{
    Handler handler;

    EventScheduler evt(100);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(50_ms);
    req.setTarget(evt, &Handler::onTimeout, handler);
    tm.arm(req);

    tm.sleep(100_ms);
    BOOST_REQUIRE(evt.synchronise(500_ms));
    BOOST_CHECK(handler.fired.load());
}

BOOST_AUTO_TEST_CASE(time_request_cancel_prevents_fire)
{
    Handler handler;

    EventScheduler evt(100);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(200_ms);
    req.setTarget(evt, &Handler::onTimeout, handler);
    tm.arm(req);
    tm.cancel(req);

    tm.sleep(250_ms);
    BOOST_REQUIRE(evt.synchronise(500_ms));

    BOOST_CHECK(!handler.fired.load());
}

BOOST_AUTO_TEST_CASE(time_request_force_arm_restarts)
{
    Handler handler;

    EventScheduler evt(100);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(300_ms);
    req.setTarget(evt, &Handler::onTimeout, handler);
    tm.arm(req);
    tm.forceArm(req);

    tm.sleep(350_ms);
    BOOST_REQUIRE(evt.synchronise(500_ms));
    BOOST_CHECK(handler.fired.load());
}

BOOST_AUTO_TEST_CASE(time_request_retriggerable)
{
    std::atomic<int> count{0};

    EventScheduler evt(100);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(30_ms, true);
    req.setTarget(evt, [&]()
                  { count.fetch_add(1); });
    tm.arm(req);

    tm.sleep(100_ms);
    BOOST_REQUIRE(evt.synchronise(500_ms));

    BOOST_CHECK_GE(count.load(), 2);
}

BOOST_AUTO_TEST_CASE(time_request_arm_twice_throws)
{
    EventScheduler evt(10);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(10_ms);
    req.setTarget(evt, []() {});
    tm.arm(req);
    BOOST_CHECK_THROW(tm.arm(req), std::logic_error);
    tm.cancel(req);
}

BOOST_AUTO_TEST_CASE(application_services_exposes_time_manager)
{
    auto center = config::createLocalFromString(R"({"x":{}})");
    ApplicationServices app;
    app.config      = center;
    app.timeManager = std::make_shared<SimpleTimeManager>();
    install(app);

    BOOST_CHECK(&current().timeManagerService() == app.timeManager.get());
}

BOOST_AUTO_TEST_CASE(application_services_time_manager_requires_service)
{
    ApplicationServices app;
    BOOST_CHECK_THROW((void)app.timeManagerService(), std::logic_error);
}

BOOST_AUTO_TEST_CASE(simple_time_manager_set_date_reanchors)
{
    SimpleTimeManager tm;
    const auto anchor = util::chrono::Date::now() + 10_s;
    tm.setDate(anchor);

    const auto read = tm.getDate();
    BOOST_CHECK((read - anchor) >= -50_ms);
    BOOST_CHECK((read - anchor) <= 50_ms);
}

BOOST_AUTO_TEST_CASE(simple_time_manager_date_advances)
{
    SimpleTimeManager tm;
    const auto t0 = tm.getDate();
    tm.sleep(50_ms);
    const auto elapsed = tm.getDate() - t0;
    BOOST_CHECK(elapsed >= 40_ms);
}

BOOST_AUTO_TEST_CASE(time_manager_by_timer_set_date_reanchors)
{
    Timer timer;
    TimeManagerByTimer tm(timer);
    const auto anchor = util::chrono::Date::now() + 10_s;
    tm.setDate(anchor);

    const auto read = tm.getDate();
    BOOST_CHECK((read - anchor) >= -50_ms);
    BOOST_CHECK((read - anchor) <= 50_ms);
}

BOOST_AUTO_TEST_CASE(time_manager_by_timer_date_follows_os_ticks)
{
    Timer timer;
    timer.start();
    TimeManagerByTimer tm(timer);
    const auto t0 = tm.getDate();

    tm.sleep(100_ms);

    const auto elapsed = tm.getDate() - t0;
    BOOST_CHECK(elapsed >= 90_ms);
}

BOOST_AUTO_TEST_CASE(simple_time_manager_factory_from_config)
{
    constexpr const char* json = R"({
      "Time": {
        "InstanceOf": "tools::design::time::SimpleTimeManager",
        "MaxArmed": 32
      }
    })";
    const auto center          = config::createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    auto tm = create<SimpleTimeManager>(app, center->root()["Time"]);
    BOOST_REQUIRE(tm);
}

BOOST_AUTO_TEST_CASE(time_manager_by_timer_fires_with_os_timer)
{
    Handler handler;

    EventScheduler evt(100);
    evt.start();

    Timer osTimer;
    osTimer.start();
    TimeManagerByTimer tm(osTimer);
    TimeRequest req(50_ms);
    req.setTarget(evt, &Handler::onTimeout, handler);
    tm.arm(req);

    tm.sleep(100_ms);
    BOOST_REQUIRE(evt.synchronise(500_ms));
    BOOST_CHECK(handler.fired.load());
}

BOOST_AUTO_TEST_CASE(time_manager_by_timer_factory_requires_timer_service)
{
    constexpr const char* json = R"({
      "Time": {
        "InstanceOf": "tools::design::time::TimeManagerByTimer"
      }
    })";
    const auto center          = config::createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    BOOST_CHECK_THROW(
        (void)create<TimeManagerByTimer>(app, center->root()["Time"]),
        std::logic_error);
}

BOOST_AUTO_TEST_CASE(time_manager_by_timer_factory_from_config)
{
    constexpr const char* json = R"({
      "Time": {
        "InstanceOf": "tools::design::time::TimeManagerByTimer",
        "MaxArmed": 16
      }
    })";
    const auto center          = config::createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.timer  = std::make_shared<Timer>();
    app.timer->start();
    app.timeManager = std::make_shared<TimeManagerByTimer>(*app.timer);

    auto tm = create<TimeManagerByTimer>(app, center->root()["Time"]);
    BOOST_REQUIRE(tm);
}

BOOST_AUTO_TEST_CASE(time_request_suspend_preserves_remaining_simple)
{
    Handler handler;

    EventScheduler evt(100);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(200_ms);
    req.setTarget(evt, &Handler::onTimeout, handler);
    tm.arm(req);

    tm.sleep(50_ms);
    tm.suspend(req);
    BOOST_CHECK(req.isSuspended());

    tm.sleep(200_ms);
    BOOST_REQUIRE(evt.synchronise(50_ms));
    BOOST_CHECK(!handler.fired.load());

    tm.resume(req);
    tm.sleep(200_ms);
    BOOST_REQUIRE(evt.synchronise(500_ms));
    BOOST_CHECK(handler.fired.load());
}

BOOST_AUTO_TEST_CASE(time_request_cancel_while_suspended)
{
    Handler handler;

    EventScheduler evt(100);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(200_ms);
    req.setTarget(evt, &Handler::onTimeout, handler);
    tm.arm(req);

    tm.sleep(50_ms);
    tm.suspend(req);
    tm.cancel(req);

    tm.sleep(300_ms);
    BOOST_REQUIRE(evt.synchronise(50_ms));
    BOOST_CHECK(!handler.fired.load());
}

BOOST_AUTO_TEST_CASE(time_request_arm_on_suspended_throws)
{
    EventScheduler evt(10);
    evt.start();

    SimpleTimeManager tm;
    TimeRequest req(100_ms);
    req.setTarget(evt, []() {});
    tm.arm(req);
    tm.suspend(req);

    BOOST_CHECK_THROW(tm.arm(req), std::logic_error);
    tm.cancel(req);
}
