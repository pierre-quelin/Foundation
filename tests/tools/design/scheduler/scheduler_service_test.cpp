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
#include "tools/design/factory/Register.hpp"
#include "tools/design/objkit/ObjKit.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/scheduler/SchedulerService.hpp"
#include "tools/os/thread/CpuAffinity.hpp"
#include "tools/os/thread/Thread.h"
#include "tools/os/thread/ThreadPolicy.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <string>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::objkit;
using namespace tools::design::scheduler;
using namespace tools::os::thread;

namespace
{

class SharedKit : public ObjKit
{
public:
    SharedKit(ApplicationServices& app, Node node) : ObjKit(app, std::move(node))
    {
        needScheduler();
    }

    ~SharedKit() override
    {
        (void)drainScheduler();
    }

    [[nodiscard]] EventScheduler* rawScheduler() { return &scheduler(); }

    using ObjKit::drainScheduler;
};

class ExclusiveKit : public ObjKit
{
public:
    ExclusiveKit(ApplicationServices& app, Node node) : ObjKit(app, std::move(node))
    {
        needScheduler(ThreadPriority::Normal, SchedulerShare::Exclusive);
    }

    ~ExclusiveKit() override
    {
        (void)drainScheduler();
    }

    [[nodiscard]] EventScheduler* rawScheduler() { return &scheduler(); }
};

} // namespace

FOUNDATION_FACTORY_REGISTER(SharedKit, "test::SharedKit", test_SharedKit)
FOUNDATION_FACTORY_REGISTER(ExclusiveKit, "test::ExclusiveKit", test_ExclusiveKit)

BOOST_AUTO_TEST_CASE(cpu_affinity_any_is_no_constraint)
{
    const CpuAffinity a = CpuAffinity::any();
    BOOST_CHECK(a.isAny());
    BOOST_CHECK_EQUAL(a.toBitMask(), 0u);
    BOOST_CHECK(a.contains(0));
    BOOST_CHECK(a.contains(63));
}

BOOST_AUTO_TEST_CASE(cpu_affinity_cores_and_range)
{
    const CpuAffinity c = CpuAffinity::cores({0, 2, 2});
    BOOST_CHECK(!c.isAny());
    BOOST_CHECK(c.contains(0));
    BOOST_CHECK(!c.contains(1));
    BOOST_CHECK(c.contains(2));
    BOOST_CHECK_EQUAL(c.toBitMask(), (1ull << 0) | (1ull << 2));

    const CpuAffinity r = CpuAffinity::range(1, 3);
    BOOST_CHECK(r.contains(1));
    BOOST_CHECK(r.contains(3));
    BOOST_CHECK(!r.contains(0));
}

BOOST_AUTO_TEST_CASE(thread_set_affinity_after_start_throws)
{
    Thread t([]() {});
    t.start();
    BOOST_CHECK_THROW(t.setAffinity(CpuAffinity::cores({0})), std::logic_error);
    t.join();
}

BOOST_AUTO_TEST_CASE(scheduler_service_shared_same_instance_with_pool_size_one)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1, "NamePrefix": "T" },
      "A": { "InstanceOf": "test::SharedKit" },
      "B": { "InstanceOf": "test::SharedKit" }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    auto a = create<SharedKit>(app, center->root()["A"]);
    auto b = create<SharedKit>(app, center->root()["B"]);
    BOOST_REQUIRE(a);
    BOOST_REQUIRE(b);
    BOOST_CHECK(a->rawScheduler() == b->rawScheduler());
    BOOST_CHECK_EQUAL(app.schedulerServiceRef().sharedCount(), 1u);

    reset();
}

BOOST_AUTO_TEST_CASE(scheduler_service_exclusive_is_distinct)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1 },
      "Shared": { "InstanceOf": "test::SharedKit" },
      "Excl": { "InstanceOf": "test::ExclusiveKit" }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    auto s = create<SharedKit>(app, center->root()["Shared"]);
    auto e = create<ExclusiveKit>(app, center->root()["Excl"]);
    BOOST_REQUIRE(s);
    BOOST_REQUIRE(e);
    BOOST_CHECK(s->rawScheduler() != e->rawScheduler());
    BOOST_CHECK_EQUAL(app.schedulerServiceRef().exclusiveAliveCount(), 1u);

    e.reset();
    BOOST_CHECK_EQUAL(app.schedulerServiceRef().exclusiveAliveCount(), 0u);

    reset();
}

BOOST_AUTO_TEST_CASE(objkit_drain_scheduler_before_destroy_shared)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1 },
      "K": { "InstanceOf": "test::SharedKit" }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    std::atomic<int> hits{0};
    {
        auto k = create<SharedKit>(app, center->root()["K"]);
        BOOST_REQUIRE(k);
        EventScheduler* const sharedEvt = k->rawScheduler();
        sharedEvt->schedule([&hits]()
                            {
            hits.fetch_add(1);
            Thread::sleep_for(util::chrono::Delay{std::chrono::milliseconds{30}});
            hits.fetch_add(1); });
        BOOST_CHECK(k->drainScheduler());
        BOOST_CHECK_EQUAL(hits.load(), 2);
    }
    // Shared pool still alive after kit destroy — no UAF if drained first.
    auto still = app.schedulerServiceRef().acquireShared();
    BOOST_REQUIRE(still);
    BOOST_CHECK(still->synchronise(ObjKit::defaultDrainTimeout()));
    BOOST_CHECK_EQUAL(hits.load(), 2);

    reset();
}

BOOST_AUTO_TEST_CASE(scheduler_instanceof_still_works)
{
    constexpr const char* json = R"({
      "Component": {
        "InstanceOf": "test::SharedKit",
        "EventScheduler": {
          "InstanceOf": "tools::design::scheduler::EventScheduler",
          "MaxEventCount": 64
        }
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();
    install(app);

    auto c = create<SharedKit>(app, center->root()["Component"]);
    BOOST_REQUIRE(c);
    BOOST_CHECK_EQUAL(c->scheduler().maxPending(), 64u);

    reset();
}
