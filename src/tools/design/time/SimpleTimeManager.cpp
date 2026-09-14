/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
*/

#include "tools/design/time/SimpleTimeManager.hpp"

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Register.hpp"

#include <stdexcept>
#include <unordered_map>

namespace tools::design::time
{

SimpleTimeManager::SimpleTimeManager(std::size_t maxArmed) : _maxArmed(maxArmed)
{
    startWorker();
}

SimpleTimeManager::SimpleTimeManager(ApplicationServices& app, config::Node node) : _maxArmed(node.contains("MaxArmed") ? node["MaxArmed"].value<unsigned int>() : 256u)
{
    (void)app;
    startWorker();
}

SimpleTimeManager::~SimpleTimeManager()
{
    stopWorker();
}

void SimpleTimeManager::startWorker()
{
    setName("SimpleTimeManager");
    start();
}

void SimpleTimeManager::stopWorker()
{
    {
        std::lock_guard lock(_mutex);
        _quit = true;
        while (!_queue.empty())
        {
            auto* const req = _queue.top().request;
            if (req->isRunning() || req->isSuspended())
            {
                req->setStatus(TimeRequestStatus::Stopped);
            }
            _deadlines.erase(req);
            _queue.pop();
        }
    }
    _cv.notify_all();
    join();
}

void SimpleTimeManager::arm(TimeRequest& request)
{
    ensureStopped(request);
    ensureHasTarget(request);
    armInternal(request, request.delay());
}

void SimpleTimeManager::forceArm(TimeRequest& request)
{
    ensureHasTarget(request);
    if (request.isRunning())
    {
        cancel(request);
    }
    else if (request.isSuspended())
    {
        std::lock_guard lock(_mutex);
        ++_generations[&request];
        _deadlines.erase(&request);
        request.setStatus(TimeRequestStatus::Stopped);
    }
    armInternal(request, request.delay());
}

void SimpleTimeManager::cancel(TimeRequest& request)
{
    std::lock_guard lock(_mutex);

    if (!request.isRunning() && !request.isSuspended())
    {
        throw std::logic_error("TimeManager::cancel: request not active");
    }

    ++_generations[&request];
    _deadlines.erase(&request);
    request.setStatus(TimeRequestStatus::Stopped);
    _cv.notify_all();
}

void SimpleTimeManager::suspend(TimeRequest& request)
{
    std::lock_guard lock(_mutex);

    if (!request.isRunning())
    {
        throw std::logic_error("ITimeManager::suspend: request not running");
    }

    const auto deadlineIt = _deadlines.find(&request);
    if (deadlineIt == _deadlines.end())
    {
        throw std::logic_error("ITimeManager::suspend: request not armed");
    }

    const auto now = std::chrono::steady_clock::now();
    auto remaining =
        deadlineIt->second > now
            ? util::chrono::Delay{std::chrono::duration_cast<std::chrono::nanoseconds>(
                  deadlineIt->second - now)}
            : util::chrono::Delay{std::chrono::nanoseconds{0}};

    ++_generations[&request];
    _deadlines.erase(deadlineIt);
    request.setRemaining(remaining);
    request.setStatus(TimeRequestStatus::Suspended);
    _cv.notify_all();
}

void SimpleTimeManager::resume(TimeRequest& request)
{
    if (!request.isSuspended())
    {
        throw std::logic_error("ITimeManager::resume: request not suspended");
    }

    ensureHasTarget(request);
    armInternal(request, request.remaining());
}

util::chrono::Date SimpleTimeManager::getDate() const
{
    const std::lock_guard lock(_dateMutex);
    const auto elapsed = std::chrono::steady_clock::now() - _epochSteady;
    return _epochDate + util::chrono::Delay{
                            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)};
}

void SimpleTimeManager::setDate(util::chrono::Date date)
{
    const std::lock_guard lock(_dateMutex);
    _epochDate   = date;
    _epochSteady = std::chrono::steady_clock::now();
}

void SimpleTimeManager::sleep(util::chrono::Delay delay)
{
    tools::os::thread::Thread::sleep_for(delay);
}

void SimpleTimeManager::armInternal(TimeRequest& request, util::chrono::Delay delay)
{
    const auto deadline = std::chrono::steady_clock::now() + delay.toNanoseconds();

    std::lock_guard lock(_mutex);

    if (_queue.size() >= _maxArmed)
    {
        throw std::runtime_error("SimpleTimeManager::arm: timer pool saturated");
    }

    request.attachManager(this);
    request.setStatus(TimeRequestStatus::Running);

    const auto generation = _generations[&request];
    _deadlines.insert_or_assign(&request, deadline);
    _queue.push(ArmedEntry{&request, deadline, generation});
    _cv.notify_all();
}

void SimpleTimeManager::expireRequest(TimeRequest& request)
{
    request.dispatch();

    if (request.isRetriggerable())
    {
        request.setStatus(TimeRequestStatus::Stopped);
        armInternal(request, request.delay());
        return;
    }

    _deadlines.erase(&request);
    request.setStatus(TimeRequestStatus::Stopped);
}

void SimpleTimeManager::body()
{
    while (true)
    {
        ArmedEntry entry{};
        bool fire = false;

        {
            std::unique_lock lock(_mutex);

            if (_quit && _queue.empty())
            {
                return;
            }

            if (_queue.empty())
            {
                _cv.wait(lock, [this]()
                         { return _quit || !_queue.empty(); });
                if (_quit && _queue.empty())
                {
                    return;
                }
                continue;
            }

            entry          = _queue.top();
            const auto now = std::chrono::steady_clock::now();
            if (entry.deadline > now)
            {
                _cv.wait_until(lock, entry.deadline);
                continue;
            }

            _queue.pop();

            if (!entry.request->isRunning())
            {
                continue;
            }

            const auto genIt = _generations.find(entry.request);
            if (genIt == _generations.end() || genIt->second != entry.generation)
            {
                continue;
            }

            fire = true;
        }

        if (fire)
        {
            expireRequest(*entry.request);
        }
    }
}

} // namespace tools::design::time

FOUNDATION_FACTORY_REGISTER(tools::design::time::SimpleTimeManager,
                            "tools::design::time::SimpleTimeManager",
                            tools_design_time_SimpleTimeManager)
