/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IBoard.h
 * @brief Generic I/O board Interface
 */
#pragma once

#include "io/adc/IADC.h"
#include "io/in/IIn.h"
#include "io/led/ILed.h"
#include "io/led/IRGBWLed.h"
#include "io/out/IOut.h"
#include "io/pwm/IPWM.h"
#include "tools/design/signal/Signal.hpp"

#include <memory>
#include <string>

namespace io::board
{

class IBoard
{
public:
    virtual ~IBoard() = default;

    /**
     * @brief Initializes the board
     *
     * @return 0 if successful, -1 otherwise.
     */
    [[nodiscard]] virtual int init() = 0;

    /**
     * @brief Returns the object with the specified name
     *
     * @param name The input name
     * @return The object or nullptr
     */
    // template<typename T>
    // [[nodiscard]] virtual std::shared_ptr<T> get<T>(const std::string& name) const = 0;
    // [[nodiscard]] virtual std::shared_ptr<T> obtainObject<T>(const std::string& name) const = 0;

    /**
     * @brief Returns the input with the specified name
     *
     * @param name The input name
     * @return The input or nullptr
     */
    [[nodiscard]] virtual std::shared_ptr<io::in::IIn> getInput(const std::string& name) const = 0;
    /**
     * @brief Returns the output with the specified name
     *
     * @param name The output name
     * @return The output or nullptr
     */
    [[nodiscard]] virtual std::shared_ptr<io::out::IOut> getOutput(const std::string& name) const = 0;
    /**
     * @brief Returns the Led with the specified name
     *
     * @param name The Led name
     * @return The Led or nullptr
     */
    [[nodiscard]] virtual std::shared_ptr<io::led::ILed> getLed(const std::string& name) const = 0;
    /**
     * @brief Returns the RGBWLed with the specified name
     *
     * @param name The RGBWLed name
     * @return The RGBWLed or nullptr
     */
    [[nodiscard]] virtual std::shared_ptr<io::led::IRGBWLed> getRGBWLed(const std::string& name) const = 0;
    /**
     * @brief Returns the ADC with the specified name
     *
     * @param name The ADC name
     * @return The ADC or nullptr
     */
    [[nodiscard]] virtual std::shared_ptr<io::adc::IADC> getADC(const std::string& name) const = 0;
    /**
     * @brief Returns the PWM with the specified name
     *
     * @param name The PWM name
     * @return The PWM or nullptr
     */
    [[nodiscard]] virtual std::shared_ptr<io::pwm::IPWM> getPWM(const std::string& name) const = 0;

    /**
     * @brief The IBoard state
     */
    enum class State
    {
        Ready,   /**< Ready */
        NotReady /**< NotReady */
    };
    virtual State state() = 0;

    /**
     * @brief IBoard Observers
     * @note For RAII pattern see boost::signals2::scoped_connection
     */
    tools::design::signal::Signal<void(State)> stateChanged;
};

} // namespace io::board
