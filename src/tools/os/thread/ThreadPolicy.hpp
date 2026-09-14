/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ThreadPolicy.hpp
 * @brief Portable thread priority and scheduling policy enums.
 */
#pragma once

#include "tools/os/thread/IThread.h"

namespace tools::os::thread
{

/** @brief Six Windows-aligned relative priorities (Idle … Highest). */
enum class ThreadPriority
{
    Idle,
    Lowest,
    BelowNormal,
    Normal,
    AboveNormal,
    Highest
};

/**
 * @brief Portable scheduling policy intention (Linux-first).
 *
 * Maps to @ref SchedPolicy. Under Windows, RoundRobin/Fifo may fall back to Other.
 */
enum class SchedulingPolicy
{
    Other,
    RoundRobin,
    Fifo
};

[[nodiscard]] inline SchedPolicy toSchedPolicy(SchedulingPolicy policy) noexcept
{
    switch (policy)
    {
        case SchedulingPolicy::Fifo:
            return SchedPolicy::Fifo;
        case SchedulingPolicy::RoundRobin:
            return SchedPolicy::RoundRobin;
        case SchedulingPolicy::Other:
        default:
            return SchedPolicy::Other;
    }
}

/** @brief Maps @p priority to the POSIX-style scale used by @ref Thread::setPriority. */
[[nodiscard]] inline int toPosixPriority(ThreadPriority priority) noexcept
{
    switch (priority)
    {
        case ThreadPriority::Highest:
            return 20;
        case ThreadPriority::AboveNormal:
            return 35;
        case ThreadPriority::Normal:
            return 50;
        case ThreadPriority::BelowNormal:
            return 65;
        case ThreadPriority::Lowest:
            return 80;
        case ThreadPriority::Idle:
        default:
            return 90;
    }
}

} // namespace tools::os::thread
