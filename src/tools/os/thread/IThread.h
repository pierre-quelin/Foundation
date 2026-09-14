/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IThread.h
 * @brief Portable thread interface (Foundation Phase 0).
 */
#pragma once

#include "tools/os/thread/CpuAffinity.hpp"
#include "util/chrono/Delay.hpp"

#include <string>
#include <string_view>

namespace tools::os::thread
{

/** @brief POSIX-style scheduling policy. */
enum class SchedPolicy
{
    Other,
    Fifo,
    RoundRobin
};

/**
 * @brief Abstract thread — start/join lifecycle.
 *
 * Concrete platform implementations live in _linux and _windows backends.
 */
class IThread
{
public:
    virtual ~IThread() = default;

    IThread()                          = default;
    IThread(const IThread&)            = delete;
    IThread& operator=(const IThread&) = delete;
    IThread(IThread&&)                 = delete;
    IThread& operator=(IThread&&)      = delete;

    /** @brief Set thread name (effective before start()). */
    virtual void setName(std::string_view name) = 0;

    /** @brief Set scheduling policy (effective before start()). */
    virtual void setSchedPolicy(SchedPolicy policy) = 0;

    /** @brief Set POSIX priority (effective before start()). */
    virtual void setPriority(int posixPriority) = 0;

    /** @brief Restrict to selected cores (effective before start()). @c any() = no constraint. */
    virtual void setAffinity(const CpuAffinity& affinity) = 0;

    /** @brief Start the thread; throws std::system_error on failure. */
    virtual void start() = 0;

    /** @brief Block until the thread finishes. No-op if never started. */
    virtual void join() = 0;

    /**
     * @brief Wait up to @p timeout for the thread to finish.
     * @return true if joined, false on timeout.
     */
    virtual bool join_for(util::chrono::Delay timeout) = 0;

    /** @brief Sleep the calling thread. */
    static void sleep_for(util::chrono::Delay d);

protected:
    /** @brief Thread entry point — override in derived classes. */
    virtual void body() {}
};

} // namespace tools::os::thread
