/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SemC.hpp
 * @brief Counting semaphore (C++17 — std::mutex + condition_variable).
 */
#pragma once

#include "util/chrono/Delay.hpp"

#include <condition_variable>
#include <cstddef>
#include <limits>
#include <mutex>

namespace tools::os::sync
{

class SemC
{
public:
    explicit SemC(std::ptrdiff_t initial  = 0,
                  std::ptrdiff_t maxCount = std::numeric_limits<std::ptrdiff_t>::max()) : _count(initial), _maxCount(maxCount)
    {
    }

    SemC(const SemC&)            = delete;
    SemC& operator=(const SemC&) = delete;

    void acquire()
    {
        std::unique_lock lock(_mutex);
        _cv.wait(lock, [this]()
                 { return _count > 0; });
        --_count;
    }

    [[nodiscard]] bool try_acquire()
    {
        std::lock_guard lock(_mutex);
        if (_count == 0)
        {
            return false;
        }
        --_count;
        return true;
    }

    [[nodiscard]] bool acquire_for(util::chrono::Delay timeout)
    {
        if (isInfinite(timeout))
        {
            acquire();
            return true;
        }

        std::unique_lock lock(_mutex);
        if (!_cv.wait_for(lock, timeout.toNanoseconds(), [this]()
                          { return _count > 0; }))
        {
            return false;
        }
        --_count;
        return true;
    }

    void release(std::ptrdiff_t update = 1)
    {
        if (update <= 0)
        {
            return;
        }

        std::lock_guard lock(_mutex);
        const std::ptrdiff_t headroom = _maxCount - _count;
        if (update > headroom)
        {
            update = headroom;
        }
        _count += update;
        if (update == 1)
        {
            _cv.notify_one();
        }
        else
        {
            _cv.notify_all();
        }
    }

    [[nodiscard]] std::ptrdiff_t available() const
    {
        std::lock_guard lock(_mutex);
        return _count;
    }

private:
    static bool isInfinite(util::chrono::Delay timeout)
    {
        return timeout.toNanoseconds().count() >= util::chrono::Delay::max().toNanoseconds().count();
    }

    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::ptrdiff_t _count;
    std::ptrdiff_t _maxCount;
};

} // namespace tools::os::sync
