/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/timer/Timer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "util/chrono/Delay.hpp"

#include <system_error>
#include <windows.h>

namespace tools::os::timer
{

namespace
{

using namespace util::chrono::literals;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

[[nodiscard]] LARGE_INTEGER qpcFrequency()
{
    LARGE_INTEGER freq{};
    if (!QueryPerformanceFrequency(&freq))
    {
        throw std::system_error(static_cast<int>(GetLastError()),
                                std::generic_category(),
                                "QueryPerformanceFrequency");
    }
    return freq;
}

[[nodiscard]] std::chrono::nanoseconds qpcCounterToNs(LONGLONG counter, const LARGE_INTEGER& frequency)
{
    const LONGLONG quot = counter / frequency.QuadPart;
    const LONGLONG rem  = counter % frequency.QuadPart;
    return std::chrono::nanoseconds{quot * 1'000'000'000LL + (rem * 1'000'000'000LL) / frequency.QuadPart};
}

[[nodiscard]] std::chrono::nanoseconds qpcNow()
{
    static const LARGE_INTEGER frequency = qpcFrequency();

    LARGE_INTEGER counter{};
    if (!QueryPerformanceCounter(&counter))
    {
        throw std::system_error(static_cast<int>(GetLastError()),
                                std::generic_category(),
                                "QueryPerformanceCounter");
    }

    return qpcCounterToNs(counter.QuadPart, frequency);
}

[[nodiscard]] HANDLE waitableTimerHandle()
{
    static thread_local const HANDLE timer = []() -> HANDLE
    {
#if !defined(WINVER) || (WINVER >= 0x0600)
        HANDLE handle = CreateWaitableTimerExW(nullptr,
                                               nullptr,
                                               CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                               TIMER_MODIFY_STATE | SYNCHRONIZE);
        if (handle != nullptr)
        {
            return handle;
        }
#endif
        HANDLE fallback = CreateWaitableTimerW(nullptr, TRUE, nullptr);
        if (fallback == nullptr)
        {
            throw std::system_error(static_cast<int>(GetLastError()),
                                    std::generic_category(),
                                    "CreateWaitableTimer");
        }
        return fallback;
    }();
    return timer;
}

void waitRelativeNanoseconds(std::chrono::nanoseconds ns)
{
    if (ns.count() <= 0)
    {
        return;
    }

    const LONGLONG interval100ns = -static_cast<LONGLONG>(ns.count() / 100);
    if (interval100ns == 0)
    {
        return;
    }

    HANDLE handle = waitableTimerHandle();
    LARGE_INTEGER due{};
    due.QuadPart = interval100ns;
    if (!SetWaitableTimer(handle, &due, 0, nullptr, nullptr, FALSE))
    {
        throw std::system_error(static_cast<int>(GetLastError()),
                                std::generic_category(),
                                "SetWaitableTimer");
    }

    const DWORD waitResult = WaitForSingleObject(handle, INFINITE);
    if (waitResult != WAIT_OBJECT_0)
    {
        throw std::system_error(static_cast<int>(GetLastError()),
                                std::generic_category(),
                                "WaitForSingleObject");
    }
}

void waitUntilQpc(std::chrono::nanoseconds target)
{
    while (qpcNow() < target)
    {
        const auto remaining = target - qpcNow();
        waitRelativeNanoseconds(remaining);
    }
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
    std::chrono::nanoseconds deadlineNs = qpcNow();
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
        waitUntilQpc(deadlineNs);

        const std::chrono::nanoseconds currentNs = qpcNow();
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
