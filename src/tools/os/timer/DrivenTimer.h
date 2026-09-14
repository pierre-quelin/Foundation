/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file DrivenTimer.h
 * @brief Externally driven timer — functional time advances on tick() only.
 */
#pragma once

#include "tools/os/timer/ITimer.h"

#include <atomic>
#include <mutex>

namespace tools::os::timer
{

/**
 * @brief Timer driven by an external simulation or protocol driver.
 *
 * Each tick() announces the functional elapsed time; listeners are notified
 * while the timer is running.
 */
class DrivenTimer : public ITimer
{
public:
    DrivenTimer();

    void tick(util::chrono::Delay step);

    void start() override;
    void stop() override;

    [[nodiscard]] bool running() const noexcept override;

    void setPeriod(util::chrono::Delay period) override;
    [[nodiscard]] util::chrono::Delay period() const noexcept override;

private:
    mutable std::mutex _mutex;
    util::chrono::Delay _period{std::chrono::nanoseconds{0}};
    std::atomic<bool> _running{false};
};

} // namespace tools::os::timer
