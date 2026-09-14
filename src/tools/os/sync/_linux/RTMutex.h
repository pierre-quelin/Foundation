/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file RTMutex.h
 * @brief Non-recursive mutex with priority inheritance (pthread).
 */
#pragma once

#include <pthread.h>

namespace tools::os::sync
{

/**
 * @brief Mutex for real-time threads (priority inheritance on Linux).
 *
 * Use to protect shared data accessed by pthread threads with fixed priorities
 * (tools::os::thread::SchedPolicy). Priority inheritance raises the priority of
 * the thread holding the lock while a higher-priority thread is blocked, which
 * limits priority inversion.
 *
 * Non-recursive: the same thread must not lock twice. Guard critical sections
 * with std::lock_guard or std::unique_lock.
 *
 * Linux: PTHREAD_PRIO_INHERIT limits priority inversion between SCHED_FIFO/RR
 * threads (see sync_test rtmutex_priority_inheritance_limits_inversion).
 *
 * **Non-RT targets:** On Windows, RTMutex is std::mutex (mutual exclusion only).
 * On Linux with SchedPolicy::Other (default) or without SCHED_FIFO privileges,
 * RTMutex remains a valid non-recursive mutex; priority inheritance has no
 * practical effect under SCHED_OTHER. Prefer SemM for general shared state.
 */
class RTMutex
{
public:
    RTMutex()
    {
        pthread_mutexattr_t attr{};
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        pthread_mutex_init(&_mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~RTMutex() { pthread_mutex_destroy(&_mutex); }

    RTMutex(const RTMutex&)            = delete;
    RTMutex& operator=(const RTMutex&) = delete;

    void lock() { pthread_mutex_lock(&_mutex); }

    [[nodiscard]] bool try_lock() { return pthread_mutex_trylock(&_mutex) == 0; }

    void unlock() { pthread_mutex_unlock(&_mutex); }

private:
    pthread_mutex_t _mutex{};
};

} // namespace tools::os::sync
