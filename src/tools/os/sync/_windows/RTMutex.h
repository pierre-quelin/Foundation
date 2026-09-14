/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file RTMutex.h
 * @brief Non-recursive mutex (std::mutex — no priority inheritance on Windows).
 */
#pragma once

#include <mutex>

namespace tools::os::sync
{

/**
 * @brief Mutex for threads with fixed priorities (process-local std mutex).
 *
 * Use to protect shared data accessed by tools::os::thread workers.
 * Priority inheritance is not available on Windows; this matches std::mutex
 * behaviour (process-local mutual exclusion without priority inheritance).
 *
 * Non-recursive: the same thread must not lock twice. Guard critical sections
 * with std::lock_guard or std::unique_lock.
 *
 * **Limitation:** no priority inheritance on Windows (unlike Linux RTMutex).
 * Mutual exclusion only — same role as std::mutex.
 *
 * **Non-RT targets:** Windows is always non-RT for priority inheritance; use
 * RTMutex only when you need the same API as Linux RT code paths, or prefer
 * SemM / std::mutex for general shared state.
 */
class RTMutex
{
public:
    void lock() { _mutex.lock(); }

    [[nodiscard]] bool try_lock() { return _mutex.try_lock(); }

    void unlock() { _mutex.unlock(); }

private:
    std::mutex _mutex;
};

} // namespace tools::os::sync
