/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Thread.h
 * @brief pthread thread implementation.
 */
#pragma once

#include "tools/os/thread/CpuAffinity.hpp"
#include "tools/os/thread/IThread.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <pthread.h>
#include <string>

namespace tools::os::thread
{

/**
 * @brief Joinable pthread thread.
 */
class Thread : public IThread
{
public:
    Thread();
    explicit Thread(std::function<void()> fn, std::string name = "<unnamed>");
    ~Thread() override;

    void setName(std::string_view name) override;
    void setSchedPolicy(SchedPolicy policy) override;
    void setPriority(int posixPriority) override;
    void setAffinity(const CpuAffinity& affinity) override;
    void start() override;
    void join() override;
    bool join_for(util::chrono::Delay timeout) override;

protected:
    void body() override;

private:
    void ensureJoined();
    int clampRealtimePriority(int policy, int posixPriority) const;
    void completeRun();
    void runEntry();

    std::function<void()> _fn;
    std::string _name;
    SchedPolicy _policy{SchedPolicy::Other};
    int _posixPriority{0};
    CpuAffinity _affinity{CpuAffinity::any()};
    pthread_t _thread{};
    bool _threadValid{false};
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _started{false};
    std::atomic<bool> _running{false};
    std::atomic<bool> _joined{false};
};

} // namespace tools::os::thread
