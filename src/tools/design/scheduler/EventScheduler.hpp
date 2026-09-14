/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EventScheduler.hpp
 * @brief FIFO event scheduler on a dedicated thread (Foundation Phase 3).
 */
#pragma once

#include "tools/design/factory/IObject.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Delay.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

namespace tools::design::config
{
class Node;
}

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::scheduler
{

/**
 * @brief Serializes asynchronous work (state-machine events, handlers) on a dedicated thread.
 *
 * Call start() then schedule() from any thread. The destructor calls quit() and
 * joins the worker (via tools::os::thread::Thread).
 *
 * @note When @c maxPending() tasks are already queued, schedule() throws
 *       std::runtime_error (back-pressure). Call synchronise() or increase
 *       MaxEventCount before posting more work.
 */
class EventScheduler : public tools::design::factory::IObject, public tools::os::thread::Thread
{
public:
    explicit EventScheduler(std::size_t maxPending = 256, std::string name = "EventScheduler");
    EventScheduler(tools::design::ApplicationServices& app,
                   tools::design::config::Node node);
    ~EventScheduler() override;

    EventScheduler(const EventScheduler&)            = delete;
    EventScheduler& operator=(const EventScheduler&) = delete;
    EventScheduler(EventScheduler&&)                 = delete;
    EventScheduler& operator=(EventScheduler&&)      = delete;

    using Thread::start;

    template <typename F>
    void schedule(F&& work)
    {
        enqueue(std::function<void()>(std::forward<F>(work)));
    }

    /** @brief Schedules a call to @p obj.*method with bound arguments. */
    template <typename C, typename M, typename... Args>
    void schedule(M C::* method, C& obj, Args&&... args)
    {
        enqueue([&, method, bound = std::make_tuple(std::forward<Args>(args)...)]() mutable
                { std::apply(
                      [&](auto&&... a)
                      { (obj.*method)(std::forward<decltype(a)>(a)...); },
                      bound); });
    }

    /**
     * @brief Waits until the queue is empty and no callback is running.
     * @return false on timeout.
     */
    [[nodiscard]] bool synchronise(util::chrono::Delay timeout = util::chrono::Delay::max());

    /** @brief Stops accepting work and ends the worker loop. */
    void quit();

    [[nodiscard]] std::size_t maxPending() const noexcept { return _maxPending; }

protected:
    void body() override;

private:
    void enqueue(std::function<void()> task);

    [[nodiscard]] bool isDrained() const
    {
        return _queue.empty() && !_processing;
    }

    std::size_t _maxPending;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::deque<std::function<void()>> _queue;
    bool _quit{false};
    bool _processing{false};
};

} // namespace tools::design::scheduler
