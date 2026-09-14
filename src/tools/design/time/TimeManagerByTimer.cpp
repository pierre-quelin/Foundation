/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/time/TimeManagerByTimer.hpp"

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Register.hpp"
#include "util/chrono/Delay.hpp"

#include <stdexcept>

namespace tools::design::time
{

TimeManagerByTimer::TimeManagerByTimer(tools::os::timer::ITimer& timer, std::size_t maxArmed) : _timer(timer), _maxArmed(maxArmed), _midIncrement(std::chrono::nanoseconds{0})
{
    refreshMidIncrement();
    _timer.addListener(*this);
    startWorker();
}

TimeManagerByTimer::TimeManagerByTimer(ApplicationServices& app, config::Node node) : TimeManagerByTimer(
                                                                                          ([&app]() -> tools::os::timer::ITimer&
                                                                                           {
              if (app.timer == nullptr)
              {
                  throw std::logic_error(
                      "TimeManagerByTimer: ApplicationServices::timer not configured");
              }
              return *app.timer; }()),
                                                                                          node.contains("MaxArmed") ? node["MaxArmed"].value<unsigned int>() : 256u)
{
}

TimeManagerByTimer::~TimeManagerByTimer()
{
    stopWorker();
    _timer.removeListener(*this);
}

void TimeManagerByTimer::refreshMidIncrement()
{
    _midIncrement = util::chrono::Delay{_timer.period().toNanoseconds() / 2};
}

void TimeManagerByTimer::startWorker()
{
    setName("TimeManagerByTimer");
    start();
}

void TimeManagerByTimer::stopWorker()
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

void TimeManagerByTimer::arm(TimeRequest& request)
{
    ensureStopped(request);
    ensureHasTarget(request);
    armInternal(request, request.delay());
}

void TimeManagerByTimer::forceArm(TimeRequest& request)
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

void TimeManagerByTimer::cancel(TimeRequest& request)
{
    std::lock_guard lock(_mutex);

    if (!request.isRunning() && !request.isSuspended())
    {
        throw std::logic_error("ITimeManager::cancel: request not active");
    }

    ++_generations[&request];
    _deadlines.erase(&request);
    request.setStatus(TimeRequestStatus::Stopped);
    _cv.notify_all();
}

void TimeManagerByTimer::suspend(TimeRequest& request)
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

    const auto nowDate = getDate();
    auto remaining =
        deadlineIt->second > nowDate ? deadlineIt->second - nowDate
                                     : util::chrono::Delay{std::chrono::nanoseconds{0}};

    ++_generations[&request];
    _deadlines.erase(deadlineIt);
    request.setRemaining(remaining);
    request.setStatus(TimeRequestStatus::Suspended);
    _cv.notify_all();
}

void TimeManagerByTimer::resume(TimeRequest& request)
{
    if (!request.isSuspended())
    {
        throw std::logic_error("ITimeManager::resume: request not suspended");
    }

    ensureHasTarget(request);
    armInternal(request, request.remaining());
}

util::chrono::Date TimeManagerByTimer::getDate() const
{
    const std::lock_guard lock(_dateMutex);
    return _currentDate;
}

void TimeManagerByTimer::setDate(util::chrono::Date date)
{
    const std::lock_guard lock(_dateMutex);
    _currentDate = date;
    _dateCv.notify_all();
}

void TimeManagerByTimer::sleep(util::chrono::Delay delay)
{
    std::unique_lock lock(_dateMutex);
    const auto deadline = _currentDate + delay;
    _dateCv.wait(lock, [this, deadline]()
                 { return _currentDate >= deadline; });
}

void TimeManagerByTimer::onTick(util::chrono::Delay elapsed)
{
    {
        const std::lock_guard lock(_dateMutex);
        _currentDate += elapsed;
        _dateCv.notify_all();
    }
    _cv.notify_all();
}

void TimeManagerByTimer::armInternal(TimeRequest& request, util::chrono::Delay delay)
{
    const util::chrono::Date deadline = getDate() + delay;

    std::lock_guard lock(_mutex);

    if (_queue.size() >= _maxArmed)
    {
        throw std::runtime_error("TimeManagerByTimer::arm: timer pool saturated");
    }

    request.attachManager(this);
    request.setStatus(TimeRequestStatus::Running);

    const auto generation = _generations[&request];
    _deadlines.insert_or_assign(&request, deadline);
    _queue.push(ArmedEntry{&request, deadline, generation});
    _cv.notify_all();
}

void TimeManagerByTimer::expireRequest(TimeRequest& request)
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

void TimeManagerByTimer::body()
{
    while (true)
    {
        ArmedEntry entry{nullptr, util::chrono::Date{std::chrono::system_clock::time_point{}}, 0};
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

            entry = _queue.top();
            if (entry.deadline > getDate() + _midIncrement)
            {
                _cv.wait(lock, [this, &entry]()
                         { return _quit || getDate() + _midIncrement >= entry.deadline; });
                if (_quit && _queue.empty())
                {
                    return;
                }
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

FOUNDATION_FACTORY_REGISTER(tools::design::time::TimeManagerByTimer,
                            "tools::design::time::TimeManagerByTimer",
                            tools_design_time_TimeManagerByTimer)
