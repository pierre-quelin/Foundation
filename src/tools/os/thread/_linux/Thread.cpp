/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Thread.cpp
 * @brief pthread thread implementation.
 */
#include "tools/os/thread/Thread.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sched.h>
#include <stdexcept>
#include <system_error>
#include <time.h>
#include <unistd.h>

namespace tools::os::thread
{

namespace
{

int toPosixPolicy(SchedPolicy policy)
{
    switch (policy)
    {
        case SchedPolicy::Fifo:
            return SCHED_FIFO;
        case SchedPolicy::RoundRobin:
            return SCHED_RR;
        case SchedPolicy::Other:
        default:
            return SCHED_OTHER;
    }
}

void throwPthreadError(const char* what, int err)
{
    throw std::system_error(err, std::generic_category(), what);
}

} // namespace

void IThread::sleep_for(util::chrono::Delay d)
{
    auto remaining = d.toNanoseconds();
    while (remaining.count() > 0)
    {
        timespec request{};
        request.tv_sec  = static_cast<time_t>(remaining.count() / 1'000'000'000LL);
        request.tv_nsec = static_cast<long>(remaining.count() % 1'000'000'000LL);

        timespec leftover{};
        if (nanosleep(&request, &leftover) == 0)
        {
            return;
        }

        if (errno != EINTR)
        {
            throw std::system_error(errno, std::generic_category(), "nanosleep");
        }

        remaining = std::chrono::seconds{leftover.tv_sec} + std::chrono::nanoseconds{leftover.tv_nsec};
    }
}

Thread::Thread() = default;

Thread::Thread(std::function<void()> fn, std::string name) : _fn(std::move(fn)), _name(std::move(name))
{
}

Thread::~Thread()
{
    if (_started.load() && !_joined.load())
    {
        join();
    }
}

void Thread::setName(std::string_view name)
{
    _name.assign(name);
}

void Thread::setSchedPolicy(SchedPolicy policy)
{
    if (_started.load())
    {
        throw std::logic_error("tools::os::thread::Thread::setSchedPolicy after start()");
    }
    _policy = policy;
}

void Thread::setPriority(int posixPriority)
{
    if (_started.load())
    {
        throw std::logic_error("tools::os::thread::Thread::setPriority after start()");
    }
    _posixPriority = posixPriority;
}

void Thread::setAffinity(const CpuAffinity& affinity)
{
    if (_started.load())
    {
        throw std::logic_error("tools::os::thread::Thread::setAffinity after start()");
    }
    _affinity = affinity;
}

void Thread::runEntry()
{
    body();
    completeRun();
}

void Thread::completeRun()
{
    {
        std::lock_guard lock(_mutex);
        _running.store(false);
    }
    _cv.notify_all();
}

void Thread::start()
{
    if (_started.exchange(true))
    {
        throw std::logic_error("tools::os::thread::Thread::start called twice");
    }

    _running.store(true);

    pthread_attr_t attr{};
    int err = pthread_attr_init(&attr);
    if (err != 0)
    {
        _started.store(false);
        _running.store(false);
        throwPthreadError("pthread_attr_init", err);
    }

    const int policy = toPosixPolicy(_policy);
    if (policy != SCHED_OTHER)
    {
        err = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        if (err == 0)
        {
            err = pthread_attr_setschedpolicy(&attr, policy);
        }
    }

    sched_param param{};
    param.sched_priority = clampRealtimePriority(policy, _posixPriority);
    if (err == 0)
    {
        err = pthread_attr_setschedparam(&attr, &param);
    }

    if (err == 0 && !_affinity.isAny())
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        bool anyValid = false;
        for (const unsigned int id : _affinity.coreIds())
        {
            if (id < CPU_SETSIZE)
            {
                CPU_SET(static_cast<int>(id), &cpuset);
                anyValid = true;
            }
        }
        if (anyValid)
        {
            err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
        }
    }

    if (err == 0)
    {
        err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    }

    if (err == 0)
    {
        err = pthread_create(&_thread, &attr, [](void* arg) -> void*
                             {
            static_cast<Thread*>(arg)->runEntry();
            return nullptr; },
                             this);
        if (err == 0)
        {
            _threadValid = true;
        }
    }

    pthread_attr_destroy(&attr);

    if (err != 0)
    {
        _started.store(false);
        _running.store(false);
        throwPthreadError("pthread_create", err);
    }

    if (!_name.empty())
    {
#if defined(__linux__)
        pthread_setname_np(_thread, _name.substr(0, 15).c_str());
#endif
    }
}

void Thread::join()
{
    if (!_started.load() || _joined.load())
    {
        return;
    }

    {
        std::unique_lock lock(_mutex);
        _cv.wait(lock, [this]()
                 { return !_running.load(); });
    }

    ensureJoined();
}

bool Thread::join_for(util::chrono::Delay timeout)
{
    if (!_started.load() || _joined.load())
    {
        return true;
    }

    {
        std::unique_lock lock(_mutex);
        const auto deadline = std::chrono::steady_clock::now() + timeout.toNanoseconds();
        if (!_cv.wait_until(lock, deadline, [this]()
                            { return !_running.load(); }))
        {
            return false;
        }
    }

    ensureJoined();
    return true;
}

void Thread::body()
{
    if (_fn)
    {
        _fn();
    }
}

void Thread::ensureJoined()
{
    if (_joined.exchange(true))
    {
        return;
    }

    if (!_threadValid)
    {
        return;
    }

    const int err = pthread_join(_thread, nullptr);
    if (err != 0)
    {
        throwPthreadError("pthread_join", err);
    }
    _threadValid = false;
}

int Thread::clampRealtimePriority(int policy, int posixPriority) const
{
    const int minPrio = sched_get_priority_min(policy);
    const int maxPrio = sched_get_priority_max(policy);
    int effectiveMax  = maxPrio;

#if defined(__linux__)
    if (policy == SCHED_FIFO || policy == SCHED_RR)
    {
        effectiveMax = 50;
        if (effectiveMax > maxPrio)
        {
            effectiveMax = maxPrio;
        }
    }
#endif

    if (posixPriority < minPrio)
    {
        return minPrio;
    }
    if (posixPriority > effectiveMax)
    {
        return effectiveMax;
    }
    return posixPriority;
}

} // namespace tools::os::thread
