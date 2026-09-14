/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/out/OutByPCA9539.h"

#include "driver/chip/PCA9539Json.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

using namespace driver::chip;
using namespace io::out;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

namespace
{

vector<PCA9539::Pin> readOutputPins(const Node& node)
{
    vector<PCA9539::Pin> pins;
    const Node pinList = node.at("Pins");
    for (std::size_t i = 0; i < pinList.size(); ++i)
    {
        pins.emplace_back(pinList[i].at("Pin").as<PCA9539::Pin>());
    }
    return pins;
}

} // namespace

OutByPCA9539::OutByPCA9539(const shared_ptr<PCA9539> device,
                           const vector<PCA9539::Pin>& pins) :
    IOut(),
    _device(device),
    _pins(pins),
    _value(0)
{
}

OutByPCA9539::OutByPCA9539(ApplicationServices& app, Node node) :
    OutByPCA9539(obtain<PCA9539>(app, node, "PCA9539"), readOutputPins(node))
{
}

OutByPCA9539::~OutByPCA9539()
{
    set(0);
}

int OutByPCA9539::init()
{
    // Restore the previous state before setting the pin mode to avoid unwanted switching.
    // Default register value is 0xFFFF
    if (-1 == set(_value))
    {
        return -1;
    }

    uint16_t p = 0;
    for (const auto& pin : _pins)
    {
        p += static_cast<uint16_t>(pin);
    }
    if (-1 == _device->pinMode(p, PCA9539::Mode::OUTPUT))
    {
        return -1;
    }
    return 0;
}

int OutByPCA9539::set(unsigned int value)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _value = value;

    // Apply
    unsigned int offset = 0;
    for (const auto& pin : _pins)
    {
        PCA9539::State state = (((value >> offset) & 1) == 1) ? PCA9539::State::HIGH : PCA9539::State::LOW;
        if (-1 == _device->digitalWrite(pin, state))
        {
            return -1;
        }
        offset++;
    }

    return 0;
}

int OutByPCA9539::get(unsigned int& value) const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    value = _value;
    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::out::OutByPCA9539,
                            "io::out::OutByPCA9539",
                            io_out_OutByPCA9539)
