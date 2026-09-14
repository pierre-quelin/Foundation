/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ILed.h
 * @brief Basic Led Interface
 */
#pragma once

#include <chrono>

namespace io::led
{

class ILed
{
public:
    virtual ~ILed() = default;

    /**
     * @brief Initializes the led
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int init() = 0;

    enum class State
    {
        Off,
        On,
        Blinking
    };
    /**
     * @brief Return the current state
     *
     * @return The state
     */
    [[nodiscard]] virtual State state() const = 0;
    /**
     * @brief Set the Led state.
     *
     * @param state The Led state.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int state(const State& state) = 0;

    class BlinkCtrl
    {
    public:
        float percent                    = 50;                             /**< Percentage of the cycle where the LED is On [0;100] */
        std::chrono::milliseconds period = std::chrono::milliseconds(500); /**< Blinking period - TODO - [min;max] values */
    };
    /**
     * @brief Return the current blink control timing
     *
     * @return The blinkCtrl
     */
    [[nodiscard]] virtual BlinkCtrl blinkCtrl() const = 0;
    /**
     * @brief Set the blink control timing.
     *
     * @param blinkCtrl The blink control timing.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int blinkCtrl(const BlinkCtrl& blinkCtrl) = 0; // TODO - Do a default software implementation : state(On); state(Off); state(On);...
};

} // namespace io::led
