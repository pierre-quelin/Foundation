/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IPWM.h
 * @brief PWM - Pulse Width Modulators Interface
 */
#pragma once

#include <chrono>

namespace io::pwm
{

class IPWM
{
public:
    virtual ~IPWM() = default;

    /**
     * @brief Initialize the PWM.
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int init() = 0;

    enum class State
    {
        Off,
        On
    };
    /**
     * @brief Return the current state
     *
     * @return The state
     */
    [[nodiscard]] virtual State state() const = 0;
    /**
     * @brief Set the PWM state
     * On : Starts the PWM port for an indefinite amount of time.
     * Off : Stops the PWM port.
     *
     * @param state The state
     * @return 0 if successful, -1 otherwise.
     */
    virtual int state(const State& state) = 0;

    class DutyCycleCtrl
    {
    public:
        float percent                    = 0;                             /**< The duty cycle of the pulse as a percent of the cycle. [0;100] */
        std::chrono::milliseconds period = std::chrono::milliseconds(10); /**< Duty cycle period - TODO - [min;max] values */
    };

    /**
     * @brief Return the current duty cycle used.
     *
     * @return The dutyCycleCtrl parameter
     */
    [[nodiscard]] virtual DutyCycleCtrl dutyCycleCtrl() const = 0;
    /**
     * @brief Defines the duty cycle
     *
     * @param ctrl The dutyCycleCtrl parameter
     * @return 0 if successful, -1 otherwise.
     */
    virtual int dutyCycleCtrl(const DutyCycleCtrl& value) = 0;
};

} // namespace io::pwm
