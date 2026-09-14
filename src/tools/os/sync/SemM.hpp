/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SemM.hpp
 * @brief Recursive mutex.
 */
#pragma once

#include <mutex>

namespace tools::os::sync
{

/**
 * @brief Mutual exclusion lock (recursive).
 *
 * Intended for critical sections guarded with std::lock_guard or std::unique_lock:
 *
 * @code
 * tools::os::sync::SemM mutex;
 * std::lock_guard<tools::os::sync::SemM> lock(mutex);
 * @endcode
 *
 * Uses std::recursive_mutex so the same thread may lock more than once.
 * Prefer plain std::mutex (and std::lock_guard<std::mutex>) when re-entry
 * is impossible — it is typically lighter than a recursive mutex.
 */
class SemM
{
public:
    void lock() { _mutex.lock(); }
    void unlock() { _mutex.unlock(); }
    [[nodiscard]] bool try_lock() { return _mutex.try_lock(); }

private:
    std::recursive_mutex _mutex;
};

} // namespace tools::os::sync
