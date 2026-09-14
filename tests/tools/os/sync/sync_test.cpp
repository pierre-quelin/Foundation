/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <system_error>

#if defined(__linux__)
#include <sched.h>
#endif

#include "tools/os/sync/RTMutex.h"
#include "tools/os/sync/SemC.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Delay.hpp"

using namespace tools::os::sync;
using namespace tools::os::thread;
using namespace util::chrono;
using namespace util::chrono::literals;

BOOST_AUTO_TEST_CASE(semc_acquire_release)
{
    SemC sem{0};
    sem.release();
    sem.acquire();
    BOOST_CHECK_EQUAL(sem.available(), 0);
}

BOOST_AUTO_TEST_CASE(semc_producer_consumer)
{
    constexpr int MessageCount = 16;
    SemC sem{0, MessageCount};
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    Thread producer([&]()
                    {
        for (int i = 0; i < MessageCount; ++i)
        {
            sem.release();
            ++produced;
        } });
    Thread consumer([&]()
                    {
        for (int i = 0; i < MessageCount; ++i)
        {
            sem.acquire();
            ++consumed;
        } });

    producer.start();
    consumer.start();
    producer.join();
    consumer.join();

    BOOST_CHECK_EQUAL(produced.load(), MessageCount);
    BOOST_CHECK_EQUAL(consumed.load(), MessageCount);
}

BOOST_AUTO_TEST_CASE(semc_acquire_for_times_out)
{
    SemC sem{0};

    BOOST_CHECK(!sem.try_acquire());
    BOOST_CHECK(!sem.acquire_for(5_ms));

    sem.release();
    BOOST_CHECK(sem.acquire_for(50_ms));
}

BOOST_AUTO_TEST_CASE(rtmutex_lock_guard)
{
    RTMutex mutex;
    std::lock_guard<RTMutex> lock(mutex);
}

BOOST_AUTO_TEST_CASE(rtmutex_try_lock_contended)
{
    RTMutex mutex;
    std::atomic<bool> holder_ready{false};
    std::atomic<bool> try_failed{false};

    Thread holder([&]()
                  {
        std::lock_guard<RTMutex> lock(mutex);
        holder_ready.store(true);
        Thread::sleep_for(50_ms); });
    Thread contender([&]()
                     {
        while (!holder_ready.load())
            Thread::sleep_for(1_ms);
        try_failed.store(!mutex.try_lock()); });

    holder.start();
    contender.start();
    holder.join();
    contender.join();

    BOOST_CHECK(try_failed.load());
}

BOOST_AUTO_TEST_CASE(rtmutex_two_threads)
{
    RTMutex mutex;
    std::atomic<int> counter{0};

    Thread worker([&]()
                  {
        std::lock_guard<RTMutex> lock(mutex);
        counter.store(42); });
    worker.start();
    worker.join();

    BOOST_CHECK_EQUAL(counter.load(), 42);
}

// ---------------------------------------------------------------------------
// RTMutex — priority inheritance (Linux) vs documented limitation (Windows)
// ---------------------------------------------------------------------------

#if defined(_WIN32)

BOOST_AUTO_TEST_CASE(rtmutex_windows_no_priority_inheritance)
{
    BOOST_TEST_MESSAGE(
        "RTMutex on Windows uses std::mutex: mutual exclusion only, "
        "no priority inheritance. For PTHREAD_PRIO_INHERIT use Linux.");

    RTMutex mutex;
    std::lock_guard<RTMutex> lock(mutex);
}

#elif defined(__linux__)

namespace
{

[[nodiscard]] bool canUseRealtimeScheduling()
{
    try
    {
        Thread probe([]() {}, "rt_probe");
        probe.setSchedPolicy(SchedPolicy::Fifo);
        probe.setPriority(sched_get_priority_min(SCHED_FIFO));
        probe.start();
        probe.join();
        return true;
    }
    catch (const std::system_error& ex)
    {
        BOOST_TEST_MESSAGE("skip RT priority test: " << ex.what());
        return false;
    }
}

void spinFor(std::chrono::milliseconds duration)
{
    const auto deadline         = std::chrono::steady_clock::now() + duration;
    volatile std::uint64_t sink = 0;
    while (std::chrono::steady_clock::now() < deadline)
    {
        sink += sink + 1U;
    }
    (void)sink;
}

} // namespace

BOOST_AUTO_TEST_CASE(rtmutex_non_rt_mutual_exclusion)
{
    BOOST_TEST_MESSAGE(
        "RTMutex on SCHED_OTHER: mutual exclusion only; "
        "PTHREAD_PRIO_INHERIT has no practical RT effect.");

    RTMutex mutex;
    std::atomic<bool> holder_ready{false};
    std::atomic<bool> try_failed{false};
    std::atomic<int> counter{0};

    Thread holder([&]()
                  {
        std::lock_guard<RTMutex> lock(mutex);
        holder_ready.store(true);
        counter.store(1);
        Thread::sleep_for(50_ms); },
                  "rt_other_holder");
    holder.setSchedPolicy(SchedPolicy::Other);

    Thread contender([&]()
                     {
        while (!holder_ready.load())
        {
            Thread::sleep_for(1_ms);
        }
        try_failed.store(!mutex.try_lock());
        std::lock_guard<RTMutex> lock(mutex);
        counter.store(2); },
                     "rt_other_contender");
    contender.setSchedPolicy(SchedPolicy::Other);

    holder.start();
    contender.start();
    holder.join();
    contender.join();

    BOOST_CHECK(try_failed.load());
    BOOST_CHECK_EQUAL(counter.load(), 2);
}

BOOST_AUTO_TEST_CASE(rtmutex_priority_inheritance_limits_inversion)
{
    if (!canUseRealtimeScheduling())
    {
        return;
    }

    const int lowPrio    = sched_get_priority_min(SCHED_FIFO) + 5;
    const int mediumPrio = sched_get_priority_min(SCHED_FIFO) + 25;
    const int highPrio   = sched_get_priority_min(SCHED_FIFO) + 45;

    RTMutex mutex;
    std::atomic<bool> lowHolding{false};
    std::atomic<bool> highWaiting{false};

    std::atomic<std::int64_t> highWaitNs{0};

    Thread low([&]()
               {
        std::lock_guard<RTMutex> lock(mutex);
        lowHolding.store(true, std::memory_order_release);
        Thread::sleep_for(80_ms); },
               "rt_low");
    low.setSchedPolicy(SchedPolicy::Fifo);
    low.setPriority(lowPrio);

    Thread high([&]()
                {
        while (!lowHolding.load(std::memory_order_acquire))
        {
            Thread::sleep_for(1_ms);
        }
        highWaiting.store(true, std::memory_order_release);
        const auto start = std::chrono::steady_clock::now();
        std::lock_guard<RTMutex> lock(mutex);
        const auto end = std::chrono::steady_clock::now();
        highWaitNs.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()); },
                "rt_high");
    high.setSchedPolicy(SchedPolicy::Fifo);
    high.setPriority(highPrio);

    Thread medium([&]()
                  {
        while (!highWaiting.load(std::memory_order_acquire))
        {
            Thread::sleep_for(1_ms);
        }
        spinFor(std::chrono::milliseconds{150}); },
                  "rt_medium");
    medium.setSchedPolicy(SchedPolicy::Fifo);
    medium.setPriority(mediumPrio);

    low.start();
    high.start();

    while (!highWaiting.load(std::memory_order_acquire))
    {
        Thread::sleep_for(1_ms);
    }
    medium.start();

    low.join();
    high.join();
    medium.join();

    const auto highWaitMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::nanoseconds{highWaitNs.load()});

    BOOST_TEST_MESSAGE("high thread blocked for " << highWaitMs.count() << " ms");

    // Without inheritance, medium (prio 25) preempts low (10) while high (45) waits:
    // expect well over ~150 ms. With PTHREAD_PRIO_INHERIT, low is boosted and releases
    // within ~80 ms.
    BOOST_CHECK_LT(highWaitMs.count(), 130);
}

#endif
