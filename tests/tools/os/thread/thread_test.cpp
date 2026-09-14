/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/thread/Thread.h"

#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>

using namespace tools::os::thread;
using namespace util::chrono;
using namespace util::chrono::literals;

BOOST_AUTO_TEST_CASE(thread_start_join_increments_counter)
{
    std::atomic<int> counter{0};

    Thread worker([&counter]()
                  { ++counter; });
    worker.start();
    worker.join();

    BOOST_CHECK_EQUAL(counter.load(), 1);
}

BOOST_AUTO_TEST_CASE(thread_sleep_for_runs_after_delay)
{
    std::atomic<bool> done{false};

    Thread worker([&done]()
                  {
        Thread::sleep_for(20_ms);
        done.store(true); });
    worker.start();
    worker.join();

    BOOST_CHECK(done.load());
}

BOOST_AUTO_TEST_CASE(thread_join_for_times_out_then_joins)
{
    std::atomic<bool> done{false};

    Thread worker([&done]()
                  {
        Thread::sleep_for(50_ms);
        done.store(true); });
    worker.start();

    BOOST_CHECK(!worker.join_for(5_ms));
    BOOST_CHECK(!done.load());

    worker.join();
    BOOST_CHECK(done.load());
}

namespace
{

class CountingThread : public Thread
{
public:
    explicit CountingThread(std::atomic<int>& counter) : _counter(counter)
    {
    }

protected:
    void body() override { ++_counter; }

private:
    std::atomic<int>& _counter;
};

} // namespace

BOOST_AUTO_TEST_CASE(thread_subclass_overrides_body)
{
    std::atomic<int> counter{0};

    CountingThread worker(counter);
    worker.start();
    worker.join();

    BOOST_CHECK_EQUAL(counter.load(), 1);
}
