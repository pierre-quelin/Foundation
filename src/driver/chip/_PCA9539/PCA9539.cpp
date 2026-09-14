/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "driver/chip/PCA9539.h"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

#include <ios>

using namespace driver::chip;
using namespace protocol::i2c;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace util::logger;

PCA9539::PCA9539(const shared_ptr<I2CMaster>& master,
                 const shared_ptr<Logger>& logger,
                 uint8_t addr) :
    I2CSlave(master, addr),
    _logger(logger)
{
}

PCA9539::PCA9539(ApplicationServices& app, Node node) :
    PCA9539(obtain<I2CMaster>(app, node, "I2CMaster"),
            app.loggerPtr(node),
            node["Address"].as<std::uint8_t>())
{
}

int PCA9539::pinMode(uint16_t modes)
{
    const std::lock_guard lock(_mutex);

    if (-1 == writeWord(static_cast<uint8_t>(Register::CONFIG), modes))
    {
        logger().log(LogService::LogLevel::ERROR, "writeWord(CONFIG, 0x{:04x}) : Error", static_cast<int>(modes));
        return -1;
    }
    return 0;
}

int PCA9539::pinMode(uint16_t pins, Mode mode)
{
    const std::lock_guard lock(_mutex);

    uint16_t modes = 0;
    if (-1 == readWord(static_cast<uint8_t>(Register::CONFIG), modes))
    {
        logger().log(LogService::LogLevel::ERROR, "readWord(CONFIG, modes) : Error");
        return -1;
    }

    if (mode == Mode::OUTPUT)
    {
        // Clear pins
        modes &= ~(pins & 0xFFFF);
    }
    else
    {
        // Set pins
        modes |= (pins & 0xFFFF);
    }

    if (-1 == writeWord(static_cast<uint8_t>(Register::CONFIG), modes))
    {
        logger().log(LogService::LogLevel::ERROR, "writeWord(CONFIG, 0x{:04x}) : Error", static_cast<int>(modes));
        return -1;
    }
    return 0;
}

int PCA9539::pinPolarity(uint16_t polarities)
{
    const std::lock_guard lock(_mutex);

    if (-1 == writeWord(static_cast<uint8_t>(Register::POLARITY_INV), polarities))
    {
        logger().log(LogService::LogLevel::ERROR, "writeWord(POLARITY_INV, 0x{:04x}) : Error", static_cast<int>(polarities));
        return -1;
    }
    return 0;
}

int PCA9539::pinPolarity(uint16_t pins, Polarity polarity)
{
    const std::lock_guard lock(_mutex);

    uint16_t polarities = 0;
    if (-1 == readWord(static_cast<uint8_t>(Register::POLARITY_INV), polarities))
    {
        logger().log(LogService::LogLevel::ERROR, "readWord(POLARITY_INV, polarities) : Error");
        return -1;
    }

    if (polarity == Polarity::NORMAL)
    {
        // Clear pins
        polarities &= ~(pins & 0xFFFF);
    }
    else
    {
        // Set pins
        polarities |= (pins & 0xFFFF);
    }

    if (-1 == writeWord(static_cast<uint8_t>(Register::POLARITY_INV), polarities))
    {
        logger().log(LogService::LogLevel::ERROR, "writeWord(POLARITY_INV, 0x{:04x}) : Error", static_cast<int>(polarities));
        return -1;
    }
    return 0;
}

int PCA9539::digitalRead(uint16_t& states)
{
    const std::lock_guard lock(_mutex); // TODO - review whether lock is needed

    if (-1 == readWord(static_cast<uint8_t>(Register::INPUT), states))
    {
        logger().log(LogService::LogLevel::ERROR, "readWord(INPUT, inputs) : Error");
        return -1;
    }
    return 0;
}

int PCA9539::digitalRead(Pin pin, State& state)
{
    const std::lock_guard lock(_mutex); // TODO - review whether lock is needed

    uint16_t inputs = 0;
    if (-1 == readWord(static_cast<uint8_t>(Register::INPUT), inputs))
    {
        logger().log(LogService::LogLevel::ERROR, "readWord(INPUT, inputs) : Error");
        return -1;
    }
    state = (inputs & pin) != 0 ? State::HIGH : State::LOW;
    return 0;
}

int PCA9539::digitalWrite(uint16_t states)
{
    const std::lock_guard lock(_mutex);

    if (-1 == writeWord(static_cast<uint8_t>(Register::OUTPUT), states))
    {
        logger().log(LogService::LogLevel::ERROR, "writeWord(OUTPUT, 0x{:04x}) : Error", static_cast<int>(states));
        return -1;
    }
    return 0;
}

int PCA9539::digitalWrite(Pin pin, State state)
{
    const std::lock_guard lock(_mutex);

    uint16_t states = 0;
    if (-1 == readWord(static_cast<uint8_t>(Register::OUTPUT), states))
    {
        logger().log(LogService::LogLevel::ERROR, "readWord(OUTPUT, states) : Error");
        return -1;
    }

    if (state == State::LOW)
    {
        // Clear pins
        states &= ~(static_cast<uint16_t>(pin) & 0xFFFF);
    }
    else
    {
        // Set pins
        states |= static_cast<uint16_t>(pin) & 0xFFFF;
    }

    if (-1 == writeWord(static_cast<uint8_t>(Register::OUTPUT), states))
    {
        logger().log(LogService::LogLevel::ERROR, "writeWord(OUTPUT, 0x{:04x}) : Error", static_cast<int>(states));
        return -1;
    }
    return 0;
}

int PCA9539::busCycleBegin()
{
    uint16_t states = 0;
    if (-1 == digitalRead(states))
    {
        return -1;
    }
    update(states);

    return 0;
}

FOUNDATION_FACTORY_REGISTER(driver::chip::PCA9539,
                            "driver::chip::PCA9539",
                            driver_chip_PCA9539)
