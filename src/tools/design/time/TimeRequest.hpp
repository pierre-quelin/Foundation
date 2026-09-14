/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TimeRequest.hpp
 * @brief Software timer request (Foundation Phase 4).
 */
#pragma once

#include "tools/design/scheduler/EventScheduler.hpp"
#include "util/chrono/Delay.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <tuple>
#include <utility>

namespace tools::design::time
{

class ITimeManager;
class SimpleTimeManager;
class TimeManagerByTimer;

/** @brief Lifecycle of a TimeRequest relative to the TimeManager. */
enum class TimeRequestStatus
{
    Stopped,
    Running,
    Suspended
};

/**
 * @brief One-shot or retriggerable delay handled by a TimeManager.
 *
 * Call setTarget() then arm(). On expiration the target is posted to the
 * EventScheduler (not invoked on the timer thread).
 */
class TimeRequest
{
public:
    TimeRequest(util::chrono::Delay delay = util::chrono::Delay{std::chrono::seconds{0}},
                bool retriggerable        = false,
                std::string name          = "");

    ~TimeRequest();

    TimeRequest(const TimeRequest&)            = delete;
    TimeRequest& operator=(const TimeRequest&) = delete;
    TimeRequest(TimeRequest&&)                 = delete;
    TimeRequest& operator=(TimeRequest&&)      = delete;

    [[nodiscard]] util::chrono::Delay delay() const noexcept { return _delay; }
    void setDelay(util::chrono::Delay delay) noexcept;

    [[nodiscard]] bool isRetriggerable() const noexcept { return _retriggerable; }

    [[nodiscard]] const std::string& name() const noexcept { return _name; }

    [[nodiscard]] TimeRequestStatus status() const noexcept { return _status; }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return _status == TimeRequestStatus::Running;
    }

    [[nodiscard]] bool isSuspended() const noexcept
    {
        return _status == TimeRequestStatus::Suspended;
    }

    [[nodiscard]] std::uint64_t ownerId() const noexcept { return _ownerId; }

    void setOwnerId(std::uint64_t id) noexcept { _ownerId = id; }

    /** @brief Runs @p handler on the scheduler thread at expiration. */
    void setTarget(scheduler::EventScheduler& evt, std::function<void()> handler);

    /** @brief Schedules a member call on @p evt at expiration. */
    template <typename C, typename M, typename... Args>
    void setTarget(scheduler::EventScheduler& evt, M C::* method, C& obj, Args&&... args)
    {
        setTarget(evt, [&, method, bound = std::make_tuple(std::forward<Args>(args)...)]() mutable
                  { std::apply(
                        [&](auto&&... a)
                        { evt.schedule(method, obj, std::forward<decltype(a)>(a)...); },
                        bound); });
    }

private:
    friend class ITimeManager;
    friend class SimpleTimeManager;
    friend class TimeManagerByTimer;

    void attachManager(ITimeManager* manager) noexcept { _manager = manager; }

    void setStatus(TimeRequestStatus status) noexcept { _status = status; }

    [[nodiscard]] util::chrono::Delay remaining() const noexcept { return _remaining; }

    void setRemaining(util::chrono::Delay remaining) noexcept { _remaining = remaining; }

    void dispatch();

    [[nodiscard]] bool hasTarget() const noexcept { return static_cast<bool>(_onExpire); }

    util::chrono::Delay _delay;
    bool _retriggerable;
    std::string _name;
    TimeRequestStatus _status{TimeRequestStatus::Stopped};
    util::chrono::Delay _remaining{std::chrono::nanoseconds{0}};
    std::uint64_t _ownerId{0};
    ITimeManager* _manager{nullptr};
    std::function<void()> _onExpire;
};

} // namespace tools::design::time
