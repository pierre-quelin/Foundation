/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/pwm/PWMByPCA9633.h"

#include "driver/chip/PCA9633Json.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

using namespace driver::chip;
using namespace io::pwm;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace std::chrono_literals;

PWMByPCA9633::PWMByPCA9633(const shared_ptr<PCA9633> device,
                           const PCA9633::Led pin) :
    IPWM(),
    _device(device),
    _pin(pin),
    _dutyCycleCtrl({0, 0ms}),
    _state(State::Off)
{
}

PWMByPCA9633::PWMByPCA9633(ApplicationServices& app, Node node) :
    PWMByPCA9633(obtain<PCA9633>(app, node, "PCA9633"), node.at("Led").as<PCA9633::Led>())
{
}

PWMByPCA9633::~PWMByPCA9633()
{
    (void)state(State::Off);
}

int PWMByPCA9633::init()
{
    // Restore previous state
    if ((-1 == dutyCycleCtrl(_dutyCycleCtrl)) &&
        (-1 == state(_state)))
    {
        return -1;
    }

    return 0;
}

IPWM::DutyCycleCtrl PWMByPCA9633::dutyCycleCtrl() const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    return _dutyCycleCtrl;
}

int PWMByPCA9633::dutyCycleCtrl(const DutyCycleCtrl& ctrl)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _dutyCycleCtrl.percent = ctrl.percent;

    // Apply
    if (-1 == _device->setBrightness(_pin, static_cast<uint8_t>(_dutyCycleCtrl.percent * 255 / 100)))
    {
        return -1;
    }

    return 0;
}

IPWM::State PWMByPCA9633::state() const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    return _state;
}

int PWMByPCA9633::state(const State& state)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _state = state;

    // Apply
    switch (state)
    {
        case State::On:
            if (_dutyCycleCtrl.percent <= 0.0f)
            {
                if (-1 == _device->setLedDrvOut(_pin, PCA9633::LedDriver::LDRx_OFF))
                {
                    return -1;
                }
            }
            else if (_dutyCycleCtrl.percent >= 100.0f)
            {
                if (-1 == _device->setLedDrvOut(_pin, PCA9633::LedDriver::LDRx_ON))
                {
                    return -1;
                }
            }
            else
            {
                if (-1 == _device->setLedDrvOut(_pin, PCA9633::LedDriver::LDRx_BRI))
                {
                    return -1;
                }
            }
            break;

        case State::Off:
            [[fallthrough]];
        default:
            if (-1 == _device->setLedDrvOut(_pin, PCA9633::LedDriver::LDRx_OFF))
            {
                return -1;
            }
            break;
    }

    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::pwm::PWMByPCA9633,
                            "io::pwm::PWMByPCA9633",
                            io_pwm_PWMByPCA9633)
