/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ISolenoid.h
 * @brief Solenoid Interface
 */
#pragma once

namespace actsens::solenoid
{

class ISolenoid
{
public:
    virtual ~ISolenoid() = default;

    /**
     * @brief Turns on the solenoid.
     */
    virtual void on() = 0;
    /**
     * @brief Turns off the solenoid.
     */
    virtual void off() = 0;

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
};

} // namespace actsens::solenoid
