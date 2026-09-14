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

#include <errno.h>
#include <time.h>

namespace tools::os::timer
{

namespace
{

using namespace util::chrono::literals;

[[nodiscard]] std::chrono::nanoseconds timespecToNs(const timespec& ts)
{
    return std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec};
}

} // namespace

Timer::Timer() : _period(20_ms), _periodOverflow(40_ms)
{
    setName("Timer");
    tools::os::thread::Thread::start();
}

Timer::~Timer()
{
    _quit.store(true);
    join();
}

void Timer::start()
{
    _running.store(true);
}

void Timer::stop()
{
    _running.store(false);
}

bool Timer::running() const noexcept
{
    return _running.load();
}

void Timer::setPeriod(util::chrono::Delay period)
{
    const std::lock_guard lock(_mutex);
    _period         = period;
    _periodOverflow = util::chrono::Delay{period.toNanoseconds() * 2};
}

util::chrono::Delay Timer::period() const noexcept
{
    const std::lock_guard lock(_mutex);
    return _period;
}

void Timer::body()
{
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return;
    }

    std::chrono::nanoseconds deadlineNs = timespecToNs(ts);
    deadlineNs += std::chrono::nanoseconds{999'999};
    deadlineNs =
        std::chrono::nanoseconds{(deadlineNs.count() / 1'000'000) * 1'000'000};

    std::chrono::nanoseconds previousNs = deadlineNs;

    while (!_quit.load())
    {
        util::chrono::Delay period{std::chrono::nanoseconds{0}};
        util::chrono::Delay periodOverflow{std::chrono::nanoseconds{0}};
        {
            const std::lock_guard lock(_mutex);
            period         = _period;
            periodOverflow = _periodOverflow;
        }

        deadlineNs += period.toNanoseconds();
        timespec request{};
        request.tv_sec  = static_cast<time_t>(deadlineNs.count() / 1'000'000'000LL);
        request.tv_nsec = static_cast<long>(deadlineNs.count() % 1'000'000'000LL);

        const int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &request, nullptr);
        if (rc != 0 && rc != EINTR)
        {
            return;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        {
            return;
        }

        const std::chrono::nanoseconds currentNs = timespecToNs(ts);
        const util::chrono::Delay elapsed{currentNs - previousNs};
        previousNs = currentNs;

        if (_running.load())
        {
            notifyListeners(elapsed);
        }

        if (elapsed > periodOverflow)
        {
            const auto skip = elapsed.toNanoseconds().count() / period.toNanoseconds().count() - 1;
            if (skip > 0)
            {
                deadlineNs += period.toNanoseconds() * skip;
            }
        }
    }
}

} // namespace tools::os::timer
