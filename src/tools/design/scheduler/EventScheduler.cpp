/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 */

#include "tools/design/scheduler/EventScheduler.hpp"

#include "tools/design/config/Node.hpp"
#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Register.hpp"

#include <chrono>

namespace tools::design::scheduler
{

EventScheduler::EventScheduler(std::size_t maxPending, std::string name) : _maxPending(maxPending)
{
    setName(name);
}

EventScheduler::EventScheduler(ApplicationServices& app, config::Node node) : _maxPending(node.contains("MaxEventCount") ? node["MaxEventCount"].value<unsigned int>()
                                                                                                                         : 256u)
{
    (void)app;
    const std::string logical = config::instancePath(node.path());
    setName(logical.empty() ? "EventScheduler" : logical);

    if (node.contains("Priority"))
    {
        setPriority(node["Priority"].value<int>());
    }

    start();
}

EventScheduler::~EventScheduler()
{
    quit();
}

void EventScheduler::enqueue(std::function<void()> task)
{
    {
        std::lock_guard lock(_mutex);
        if (_quit)
        {
            throw std::logic_error("EventScheduler::schedule: scheduler stopped");
        }
        if (_queue.size() >= _maxPending)
        {
            throw std::runtime_error("EventScheduler::schedule: queue full");
        }
        _queue.push_back(std::move(task));
    }
    _cv.notify_one();
}

bool EventScheduler::synchronise(util::chrono::Delay timeout)
{
    std::unique_lock lock(_mutex);
    const auto isDone = [this]()
    { return isDrained(); };

    if (timeout == util::chrono::Delay::max())
    {
        _cv.wait(lock, isDone);
        return true;
    }

    const auto deadline =
        std::chrono::steady_clock::now() + timeout.toNanoseconds();
    return _cv.wait_until(lock, deadline, isDone);
}

void EventScheduler::quit()
{
    {
        std::lock_guard lock(_mutex);
        _quit = true;
    }
    _cv.notify_all();
}

void EventScheduler::body()
{
    while (true)
    {
        std::function<void()> work;
        {
            std::unique_lock lock(_mutex);
            _cv.wait(lock, [this]()
                     { return _quit || !_queue.empty(); });
            if (_quit && _queue.empty())
            {
                return;
            }
            if (_queue.empty())
            {
                continue;
            }
            work = std::move(_queue.front());
            _queue.pop_front();
            _processing = true;
        }

        // Always clear _processing so synchronise() cannot hang if work throws.
        try
        {
            work();
        }
        catch (...)
        {
        }

        {
            std::lock_guard lock(_mutex);
            _processing = false;
        }
        _cv.notify_all();
    }
}

} // namespace tools::design::scheduler

FOUNDATION_FACTORY_REGISTER(tools::design::scheduler::EventScheduler,
                            "tools::design::scheduler::EventScheduler",
                            tools_design_scheduler_EventScheduler)
