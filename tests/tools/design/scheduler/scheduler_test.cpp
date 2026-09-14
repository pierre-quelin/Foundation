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
#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <stdexcept>
#include <vector>

using namespace tools::design;
using namespace tools::design::factory;
using namespace tools::design::scheduler;
using namespace tools::os::thread;
using namespace util::chrono;
using namespace util::chrono::literals;

BOOST_AUTO_TEST_CASE(scheduler_schedule_runs_on_worker_thread)
{
    std::atomic<bool> flag{false};

    EventScheduler evt(100);
    evt.start();
    evt.schedule([&]()
                 { flag.store(true); });
    BOOST_REQUIRE(evt.synchronise());

    BOOST_CHECK(flag.load());
}

BOOST_AUTO_TEST_CASE(scheduler_fifo_order)
{
    EventScheduler evt(100);
    evt.start();

    std::vector<int> order;
    evt.schedule([&]()
                 { order.push_back(1); });
    evt.schedule([&]()
                 { order.push_back(2); });
    evt.schedule([&]()
                 { order.push_back(3); });
    BOOST_REQUIRE(evt.synchronise());

    BOOST_REQUIRE_EQUAL(order.size(), 3u);
    BOOST_CHECK_EQUAL(order[0], 1);
    BOOST_CHECK_EQUAL(order[1], 2);
    BOOST_CHECK_EQUAL(order[2], 3);
}

BOOST_AUTO_TEST_CASE(scheduler_queue_full_throws)
{
    EventScheduler evt(1);

    evt.schedule([]() {});
    BOOST_CHECK_THROW(evt.schedule([]() {}), std::runtime_error);

    evt.start();
    BOOST_REQUIRE(evt.synchronise());
}

BOOST_AUTO_TEST_CASE(scheduler_synchronise_times_out)
{
    EventScheduler evt(100);
    evt.start();

    evt.schedule([&]()
                 { Thread::sleep_for(50_ms); });
    BOOST_CHECK(!evt.synchronise(5_ms));
    BOOST_REQUIRE(evt.synchronise(200_ms));
}

BOOST_AUTO_TEST_CASE(scheduler_destructor_stops_worker)
{
    std::atomic<bool> started{false};
    std::atomic<bool> done{false};

    {
        EventScheduler evt(100);
        evt.start();
        evt.schedule([&]()
                     {
            started.store(true);
            Thread::sleep_for(20_ms);
            done.store(true); });
        while (!started.load())
        {
            Thread::sleep_for(1_ms);
        }
    }

    BOOST_CHECK(done.load());
}

namespace
{

struct Counter
{
    void add(int value) { sum += value; }

    int sum = 0;
};

} // namespace

BOOST_AUTO_TEST_CASE(scheduler_schedule_member_function)
{
    Counter counter;

    EventScheduler evt(100);
    evt.start();
    evt.schedule(&Counter::add, counter, 42);
    BOOST_REQUIRE(evt.synchronise());

    BOOST_CHECK_EQUAL(counter.sum, 42);
}

BOOST_AUTO_TEST_CASE(scheduler_factory_from_config)
{
    constexpr const char* json = R"({
      "Sched": {
        "InstanceOf": "tools::design::scheduler::EventScheduler",
        "MaxEventCount": 50,
        "Priority": 10
      }
    })";
    const auto center          = config::createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    auto evt = create<EventScheduler>(app, center->root()["Sched"]);
    BOOST_REQUIRE(evt);
    BOOST_CHECK_EQUAL(evt->maxPending(), 50u);

    std::atomic<bool> flag{false};
    evt->schedule([&]()
                  { flag.store(true); });
    BOOST_REQUIRE(evt->synchronise());
    BOOST_CHECK(flag.load());
}
