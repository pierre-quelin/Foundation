/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "driver/chip/PCA9635.h"

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

uint8_t PCA9635::ledoutRegister(Led led)
{
    return static_cast<uint8_t>(Register::LEDOUT0) + (static_cast<uint8_t>(led) / 4);
}

int PCA9635::ledoutShift(Led led)
{
    return (static_cast<int>(led) % 4) * 2;
}

uint8_t PCA9635::packLedDriver(LedDriver driver)
{
    const auto value = static_cast<uint8_t>(driver);
    return static_cast<uint8_t>((value << 6) | (value << 4) | (value << 2) | value);
}

PCA9635::PCA9635(const shared_ptr<I2CMaster>& master,
                 const shared_ptr<Logger>& logger,
                 const uint8_t addr) :
    I2CSlave(master, addr),
    _logger(logger)
{
}

PCA9635::PCA9635(ApplicationServices& app, Node node) :
    PCA9635(obtain<I2CMaster>(app, node, "I2CMaster"),
            app.loggerPtr(node),
            node["Address"].as<std::uint8_t>())
{
}

int PCA9635::wakeUp()
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE1), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE1), msg[1]))
    {
        return -1;
    }
    msg[1] &= ~PCA9635_MODE1_SLEEP;
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "wakeUp() - write(MODE1, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::sleep()
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE1), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE1), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "readByte(MODE1, msg) : Error");
        return -1;
    }
    msg[1] |= PCA9635_MODE1_SLEEP;
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "sleep() - write(MODE1, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setModeGrpCtrl(GrpCtrl grpCtrl)
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
        msg[1] |= PCA9635_MODE2_DMBLNK;
    }
    else // GrpCtrl::DIMMING
    {
        msg[1] &= ~PCA9635_MODE2_DMBLNK;
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeGrpCtrl() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setModeOutLogicState(LogicState state)
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
        msg[1] |= PCA9635_MODE2_INVRT;
    }
    else
    {
        msg[1] &= ~PCA9635_MODE2_INVRT;
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutLogicState() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setModeOutDrv(Driver driver)
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
        msg[1] |= PCA9635_MODE2_OUTDRV;
    }
    else
    {
        msg[1] &= ~PCA9635_MODE2_OUTDRV;
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutDrv() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setModeOutNE(OELogicState state)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(Register::MODE2), 0};
    if (-1 == readByte(static_cast<uint8_t>(Register::MODE2), msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutNE() - readByte(MODE2, msg) : Error");
        return -1;
    }
    msg[1] &= ~PCA9635_MODE2_OUTNE;
    msg[1] |= PCA9635_MODE2_OUTNE & static_cast<uint8_t>(state);
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setModeOutNE() - write(MODE2, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setDimmingGrpCtrl(uint8_t pwm)
{
    const std::lock_guard lock(_mutex);

    const uint8_t msg[] = {static_cast<uint8_t>(Register::GRPPWM), pwm};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setDimmingGrpCtrl() - write(GRPPWM, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setBlinkingGrpCtrl(uint8_t pwm, uint8_t freq)
{
    const std::lock_guard lock(_mutex);

    const uint8_t msg[] = {static_cast<uint8_t>(Register::GRPPWM) | static_cast<uint8_t>(PCA9635_INC_AUTO_GRP), pwm, freq};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setBlinkingGrpCtrl() - write(GRPPWM, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setLedDrvOut(Led led, LedDriver driver)
{
    const std::lock_guard lock(_mutex);

    const uint8_t reg = ledoutRegister(led);
    const int shift   = ledoutShift(led);
    uint8_t msg[]     = {reg, 0};
    if (-1 == readByte(reg, msg[1]))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - readByte(LEDOUT, msg) : Error");
        return -1;
    }
    msg[1] &= ~(static_cast<uint8_t>(0x03) << shift);
    msg[1] |= (static_cast<uint8_t>(driver) << shift);
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - write(LEDOUT, 0x{:02x}) : Error", static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setLedDrvOut(LedDriver driver)
{
    const std::lock_guard lock(_mutex);

    const uint8_t packed = packLedDriver(driver);
    const uint8_t msg[]  = {
        static_cast<uint8_t>(Register::LEDOUT0) | static_cast<uint8_t>(PCA9635_INC_AUTO_ALL),
        packed,
        packed,
        packed,
        packed};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - write(LEDOUT0..3, 0x{:02x}) : Error", static_cast<int>(packed));
        return -1;
    }
    return 0;
}

int PCA9635::setLedDrvOut(const vector<pair<Led, LedDriver>>& drivers)
{
    const std::lock_guard lock(_mutex);

    uint8_t ledout[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i)
    {
        const uint8_t reg = static_cast<uint8_t>(Register::LEDOUT0) + static_cast<uint8_t>(i);
        if (-1 == readByte(reg, ledout[i]))
        {
            logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - readByte(LEDOUT{}, msg) : Error", i);
            return -1;
        }
    }

    for (const auto& [led, driver] : drivers)
    {
        const int index = static_cast<int>(led) / 4;
        const int shift = ledoutShift(led);
        ledout[index] &= ~(static_cast<uint8_t>(0x03) << shift);
        ledout[index] |= (static_cast<uint8_t>(driver) << shift);
    }

    const uint8_t msg[] = {
        static_cast<uint8_t>(Register::LEDOUT0) | static_cast<uint8_t>(PCA9635_INC_AUTO_ALL),
        ledout[0],
        ledout[1],
        ledout[2],
        ledout[3]};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setLedDrvOut() - write(LEDOUT0..3) : Error");
        return -1;
    }

    return 0;
}

int PCA9635::setBrightness(Led led, uint8_t pwm)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[] = {static_cast<uint8_t>(static_cast<uint8_t>(Register::PWM0) + static_cast<uint8_t>(led)), pwm};
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setBrightness() - write(PWM {}, 0x{:02x}) : Error", static_cast<int>(led), static_cast<int>(msg[1]));
        return -1;
    }
    return 0;
}

int PCA9635::setBrightness(const array<uint8_t, 16>& pwms)
{
    const std::lock_guard lock(_mutex);

    uint8_t msg[17];
    msg[0] = static_cast<uint8_t>(Register::PWM0) | static_cast<uint8_t>(PCA9635_INC_AUTO_PWM);
    for (size_t i = 0; i < pwms.size(); ++i)
    {
        msg[i + 1] = pwms[i];
    }
    if (-1 == write(msg, sizeof(msg)))
    {
        logger().log(LogService::LogLevel::ERROR, "setBrightness() - write(PWM0..PWM15) : Error");
        return -1;
    }
    return 0;
}

FOUNDATION_FACTORY_REGISTER(driver::chip::PCA9635,
                            "driver::chip::PCA9635",
                            driver_chip_PCA9635)
