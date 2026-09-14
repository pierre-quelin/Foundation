/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PCA9539.h
 * @brief The PCA9539 is a remote 16-Bit I2C and SMBus Low-Power I/O Expander With Interrupt
 *
 * Output, Reset, and Configuration Registers.
 */
#pragma once

#include "protocol/i2c/I2C.h"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/signal/Signal.hpp"
#include "util/logger/Logger.hpp"

#include <cstdint>
#include <memory>
#include <mutex>

namespace tools::design
{
struct ApplicationServices;

namespace config
{
class Node;
} // namespace config
} // namespace tools::design

namespace driver::chip
{

class PCA9539 final : public tools::design::factory::IObject, public protocol::i2c::I2CSlave
{
public:
    ~PCA9539() override = default;
    PCA9539(tools::design::ApplicationServices& app,
            tools::design::config::Node node);
    PCA9539(const PCA9539&)            = delete;
    PCA9539& operator=(const PCA9539&) = delete;
    PCA9539(PCA9539&&)                 = delete;
    PCA9539& operator=(PCA9539&&)      = delete;

    // int init() override { return 0; }
    /**
     * @brief Read the register and informs observers of any changes.
     *
     * @return 0 if successful, -1 otherwise.
     */
    int busCycleBegin() override;

    enum Pin
    {
        P00 = 0x0001,
        P01 = 0x0002,
        P02 = 0x0004,
        P03 = 0x0008,
        P04 = 0x0010,
        P05 = 0x0020,
        P06 = 0x0040,
        P07 = 0x0080,
        P10 = 0x0100,
        P11 = 0x0200,
        P12 = 0x0400,
        P13 = 0x0800,
        P14 = 0x1000,
        P15 = 0x2000,
        P16 = 0x4000,
        P17 = 0x8000
    };

    enum class Mode
    {
        OUTPUT = 0b0,
        INPUT  = 0b1
    };

    /**
     * @brief Set the pins to the specified modes.
     *
     * @param modes Modes register.
     * @return 0 if successful, -1 otherwise.
     */
    int pinMode(uint16_t modes = 0xFFFF);
    /**
     * @brief Set the pins to the same specified mode.
     *
     * @param pins The pins.
     * @param mode The mode
     * @return 0 if successful, -1 otherwise.
     */
    int pinMode(uint16_t pins, Mode mode);

    enum Polarity
    {
        NORMAL   = 0b0,
        INVERTED = 0b1
    };

    /**
     * @brief Set the pins to the specified polarities
     *
     * @param polarities Polarities register.
     * @return 0 if successful, -1 otherwise.
     */
    int pinPolarity(uint16_t polarities = 0x0000);
    /**
     * @brief Set the pins to the specified polarity
     *
     * @param pins The pins
     * @param polarity The polarity
     * @return 0 if successful, -1 otherwise.
     */
    int pinPolarity(uint16_t pins, Polarity polarity);

    enum State
    {
        LOW  = 0x00,
        HIGH = 0x01
    };

    /**
     * @brief Reads all input pins.
     *
     * @param states Input pins states
     * @return 0 if successful, -1 otherwise.
     */
    int digitalRead(uint16_t& states);
    /**
     * @brief Reads the specified input pin.
     *
     * @param pin The pin
     * @param state The pin state
     * @return 0 if successful, -1 otherwise.
     */
    int digitalRead(Pin pin, State& state);

    /**
     * @brief Writes all output pins.
     *
     * @param states Output pins states
     * @return 0 if successful, -1 otherwise.
     */
    int digitalWrite(uint16_t states);
    /**
     * @brief Writes the specified output state
     *
     * @param pin The pin
     * @param state The pin state
     * @return 0 if successful, -1 otherwise.
     */
    int digitalWrite(Pin pin, State state);

    /**
     * @brief Cycle Observers
     * @note For RAII pattern see boost::signals2::scoped_connection
     */
    tools::design::signal::Signal<void(uint16_t /* values */)> update;

private:
    PCA9539(const std::shared_ptr<protocol::i2c::I2CMaster>& master,
            const std::shared_ptr<util::logger::Logger>& logger,
            uint8_t addr);

    [[nodiscard]] util::logger::Logger& logger() const { return *_logger; }

    std::shared_ptr<util::logger::Logger> _logger;

    enum class Register
    {
        INPUT        = 0x00, // read Input Port
        OUTPUT       = 0x02, // read/write Output Port
        POLARITY_INV = 0x04, // read/write Polarity Inversion Port
        CONFIG       = 0x06, // read/write Configuration Port
    };

    mutable std::mutex _mutex;
};

} // namespace driver::chip
