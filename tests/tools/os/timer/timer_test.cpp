/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/timer/Timer.h"

#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace tools::os::timer;
using namespace util::chrono;
using namespace util::chrono::literals;

namespace
{

struct TickCounter : ITimerListener
{
    void onTick(Delay elapsed) override
    {
        tickCount.fetch_add(1);
        totalElapsed += elapsed;
    }

    std::atomic<int> tickCount{0};
    Delay totalElapsed{std::chrono::nanoseconds{0}};
};

} // namespace

BOOST_AUTO_TEST_CASE(timer_notifies_while_running)
{
    Timer timer;
    TickCounter counter;
    timer.setPeriod(20_ms);
    timer.addListener(counter);
    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds{70});

    timer.stop();
    BOOST_CHECK_GE(counter.tickCount.load(), 2);
    BOOST_CHECK_GE(counter.totalElapsed.toNanoseconds().count(), (40_ms).toNanoseconds().count());
}

BOOST_AUTO_TEST_CASE(timer_stop_suppresses_notifications)
{
    Timer timer;
    TickCounter counter;
    timer.setPeriod(10_ms);
    timer.addListener(counter);
    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds{35});
    timer.stop();

    const int ticksAfterStop = counter.tickCount.load();
    std::this_thread::sleep_for(std::chrono::milliseconds{40});
    BOOST_CHECK_EQUAL(counter.tickCount.load(), ticksAfterStop);
}

BOOST_AUTO_TEST_CASE(timer_period_is_configurable)
{
    Timer timer;
    timer.setPeriod(5_ms);
    BOOST_CHECK_GE(timer.period().toNanoseconds().count(), (5_ms).toNanoseconds().count());
}
