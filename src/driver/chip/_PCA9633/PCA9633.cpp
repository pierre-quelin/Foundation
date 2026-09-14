/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "driver/chip/PCA9633.h"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

#include <cstdint>
#include <ios>

using namespace driver::chip;
using namespace protocol::i2c;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace util::logger;

PCA9633::PCA9633(const shared_ptr<I2CMaster>& master,
                 const shared_ptr<Logger>& logger,
                 const uint8_t addr) :
    I2CSlave(master, addr),
    _logger(logger)
{
}

PCA9633::PCA9633(ApplicationServices& app, Node node) :
    PCA9633(obtain<I2CMaster>(app, node, "I2CMaster"),
            app.loggerPtr(node),
            node["Address"].as<std::uint8_t>())
{
}

int PCA9633::wakeUp()
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE1), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE1), msg[1]))
    {
        return -1;
    }
    msg[1] &= ~PCA9633_MODE1_SLEEP;
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "wakeUp() - write(MODE1, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::sleep()
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE1), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE1), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "readByte(MODE1, msg) : Error");
        return -1;
    }
    msg[1] |= PCA9633_MODE1_SLEEP;
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "sleep() - write(MODE1, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setModeGrpCtrl(GrpCtrl grpCtrl)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE2), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE2), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeGrpCtrl() - readByte(MODE2, msg) : Error");
        return -1;
    }
    if (GrpCtrl::BLINKING == grpCtrl)
    {
        // Set new state to specified drv
        msg[1] |= PCA9633_MODE2_DMBLNK;
    }
    else // GrpCtrl::DIMMING
    {
        // Clear the DMBLNK bit
        msg[1] &= ~PCA9633_MODE2_DMBLNK;
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeGrpCtrl() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setModeOutLogicState(LogicState state)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE2), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE2), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutLogicState() - readByte(MODE2, msg) : Error");
        return -1;
    }
    if (LogicState::INVERTED == state)
    {
        // Set new state to specified drv
        msg[1] |= PCA9633_MODE2_INVRT;
    }
    else
    {
        // Clear the INVRT bit
        msg[1] &= ~PCA9633_MODE2_INVRT;
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutLogicState() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setModeOutDrv(Driver driver)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE2), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE2), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutDrv() - readByte(MODE2, msg) : Error");
        return -1;
    }
    if (Driver::TOTEM_POLE == driver)
    {
        // Set new state to specified drv
        msg[1] |= PCA9633_MODE2_OUTDRV;
    }
    else
    {
        // Clear the OUTDRV bit
        msg[1] &= ~PCA9633_MODE2_OUTDRV;
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutDrv() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setModeOutNE(OELogicState state)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE2), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE2), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutNE() - readByte(MODE2, msg) : Error");
        return -1;
    }
    // Clear all OUTNE bits
    msg[1] &= ~PCA9633_MODE2_OUTNE;
    // Set OUTNE bits to the specified value
    msg[1] |= PCA9633_MODE2_OUTNE & static_cast<uint8_t>(state);
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutNE() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setDimmingGrpCtrl(uint8_t pwm)
{
    const std::lock_guard lock(_mutex); // TODO - review whether lock is needed

    const uint8_t msg[] = {static_cast<uint8_t>(Register::GRPPWM), pwm};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setDimmingGrpCtrl() - write(GRPPWM, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setBlinkingGrpCtrl(uint8_t pwm, uint8_t freq)
{
    const std::lock_guard lock(_mutex); // TODO - review whether lock is needed

    const uint8_t msg[] = {static_cast<uint8_t>(Register::GRPPWM) | static_cast<uint8_t>(PCA9633_INC_AUTO_GRP), pwm, freq};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setBlinkingGrpCtrl() - write(GRPPWM, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setLedDrvOut(Led led, LedDriver driver)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::LEDOUT), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::LEDOUT), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - readByte(LEDOUT, msg) : Error");
        return -1;
    }
    // Clear both bits
    msg[1] &= ~(static_cast<uint8_t>(0x03) << (static_cast<int>(led) * 2));
    // Set new driver to specified led
    msg[1] |= (static_cast<uint8_t>(driver) << (static_cast<int>(led) * 2));
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - write(LEDOUT, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setLedDrvOut(LedDriver driver)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::LEDOUT), 0};
    msg[1]        = (static_cast<uint8_t>(driver) << (static_cast<int>(Led::LED3) * 2)) |
                    (static_cast<uint8_t>(driver) << (static_cast<int>(Led::LED2) * 2)) |
                    (static_cast<uint8_t>(driver) << (static_cast<int>(Led::LED1) * 2)) |
                    (static_cast<uint8_t>(driver) << (static_cast<int>(Led::LED0) * 2));
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - write(LEDOUT, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setLedDrvOut(const vector<pair<Led, LedDriver>>& drivers)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::LEDOUT), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::LEDOUT), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - readByte(LEDOUT, msg) : Error");
        return -1;
    }
    for (const auto& [led, driver] : drivers)
    {
        // Clear both bits
        msg[1] &= ~(static_cast<uint8_t>(0x03) << (static_cast<int>(led) * 2));
        // Set new driver to specified led
        msg[1] |= (static_cast<uint8_t>(driver) << (static_cast<int>(led) * 2));
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - write(LEDOUT, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }

    return 0;
}

int PCA9633::setBrightness(Led led, uint8_t pwm)
{
    const std::lock_guard lock(_mutex); // TODO - review whether lock is needed

    uint8_t msg[] = {static_cast<uint8_t>(static_cast<uint8_t>(Register::PWM0) + static_cast<uint8_t>(led)), pwm};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setBrightness() - write(PWM {}, 0x{:02x}) : Error", static_cast<int>(led), static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9633::setBrightness(uint8_t pwm0, uint8_t pwm1, uint8_t pwm2, uint8_t pwm3)
{
    const std::lock_guard lock(_mutex); // TODO - review whether lock is needed

    const uint8_t msg[] = {static_cast<uint8_t>(Register::PWM0) | static_cast<uint8_t>(PCA9633_INC_AUTO_PWM), pwm0, pwm1, pwm2, pwm3};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setBrightness() - write(PWM0, [0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x}]) : Error", static_cast<int>(msg[1]), static_cast<int>(msg[2]), static_cast<int>(msg[3]), static_cast<int>(msg[4]));
        return -1;
    }
    return 0;
}

FOUNDATION_FACTORY_REGISTER(driver::chip::PCA9633,
                            "driver::chip::PCA9633",
                            driver_chip_PCA9633)
