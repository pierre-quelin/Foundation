/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SemB.hpp
 * @brief Binary semaphore.
 */
#pragma once

#include "tools/os/sync/SemC.hpp"

namespace tools::os::sync
{

class SemB
{
public:
    explicit SemB(bool initiallyAvailable = false) : _sem(initiallyAvailable ? 1 : 0, 1)
    {
    }

    void acquire() { _sem.acquire(); }

    [[nodiscard]] bool try_acquire() { return _sem.try_acquire(); }

    [[nodiscard]] bool acquire_for(util::chrono::Delay timeout) { return _sem.acquire_for(timeout); }

    void release() { _sem.release(); }

private:
    SemC _sem;
};

} // namespace tools::os::sync
