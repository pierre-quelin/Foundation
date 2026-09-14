/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/led/LedByPCA9539.h"

#include "driver/chip/PCA9539Json.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

using namespace driver::chip;
using namespace io::led;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

LedByPCA9539::LedByPCA9539(const shared_ptr<PCA9539> device,
                           const PCA9539::Pin pin) :
    ILed(),
    _device(device),
    _pin(pin),
    _state(State::Off)
{
}

LedByPCA9539::LedByPCA9539(ApplicationServices& app, Node node) :
    LedByPCA9539(obtain<PCA9539>(app, node, "PCA9539"), node.at("Pin").as<PCA9539::Pin>())
{
}

LedByPCA9539::~LedByPCA9539()
{
    (void)state(State::Off);
}

int LedByPCA9539::init()
{
    // Restore the previous state before setting the pin mode to avoid unwanted switching.
    // Default register value is 0xFFFF
    if (-1 == state(_state))
    {
        return -1;
    }

    if (-1 == _device->pinMode(static_cast<uint16_t>(_pin), PCA9539::Mode::OUTPUT))
    {
        return -1;
    }

    return 0;
}

ILed::State LedByPCA9539::state() const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    return _state;
}

int LedByPCA9539::state(const State& state)
{
    if (state == State::Blinking)
    {
        // TODO - cf. blinkCtrl()
        return -1;
    }

    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _state = state;

    // Apply
    PCA9539::State s = state == State::On ? PCA9539::State::HIGH : PCA9539::State::LOW;
    if (-1 == _device->digitalWrite(_pin, s))
    {
        return -1;
    }

    return 0;
}

ILed::BlinkCtrl LedByPCA9539::blinkCtrl() const
{
    BlinkCtrl blinkCtrl = {0, 0ms};
    return blinkCtrl;
}

int LedByPCA9539::blinkCtrl(const BlinkCtrl& blinkCtrl)
{
    (void)blinkCtrl;
    // TODO - Do a default software implementation : state(On); state(Off); state(On);...
    return -1;
}

FOUNDATION_FACTORY_REGISTER(io::led::LedByPCA9539,
                            "io::led::LedByPCA9539",
                            io_led_LedByPCA9539)
