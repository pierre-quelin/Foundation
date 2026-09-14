/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/in/InByPCA9539.h"

#include "driver/chip/PCA9539Json.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

using namespace driver::chip;
using namespace io::in;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

namespace
{

vector<pair<PCA9539::Pin, PCA9539::Polarity>> readPins(Node node)
{
    vector<pair<PCA9539::Pin, PCA9539::Polarity>> confInput;
    const Node pinList = node.at("Pins");
    for (std::size_t i = 0; i < pinList.size(); ++i)
    {
        const Node pin = pinList[i];
        confInput.emplace_back(pin.at("Pin").as<PCA9539::Pin>(), pin.at("Polarity").as<PCA9539::Polarity>());
    }
    return confInput;
}

} // namespace

InByPCA9539::InByPCA9539(const shared_ptr<PCA9539> device,
                         const vector<pair<PCA9539::Pin, PCA9539::Polarity>>& pins) :
    IIn(),
    _device(device),
    _pins(pins),
    _value(0)
{
    _cnx = _device->update.connect([&](auto state)
                                   { refreshValues(state); });
}

InByPCA9539::InByPCA9539(ApplicationServices& app, Node node) :
    InByPCA9539(obtain<PCA9539>(app, node, "PCA9539"), readPins(node))
{
}

int InByPCA9539::init()
{
    uint16_t p = 0;
    for (const auto& [pin, polarity] : _pins)
    {
        if (-1 == _device->pinPolarity(static_cast<uint16_t>(pin), polarity))
        {
            return -1;
        }
        p += static_cast<uint16_t>(pin);
    }
    if (-1 == _device->pinMode(p, PCA9539::Mode::INPUT))
    {
        return -1;
    }

    return 0;
}

int InByPCA9539::get(unsigned int& value) const
{
    uint16_t reg = 0;
    if (-1 == _device->digitalRead(reg))
    {
        return -1;
    }
    value = computeIn(reg);

    return 0;
}

int InByPCA9539::refreshValues(uint16_t reg)
{
    unsigned int value = computeIn(reg);

    // Has changed since last refreshValue
    if (value != _value)
    {
        _value = value;
        // Informs observers
        valueChanged(_value);
    }

    return 0;
}

unsigned int InByPCA9539::computeIn(uint16_t reg) const
{
    unsigned int value  = 0;
    unsigned int offset = 0;
    for (const auto& [pin, polarity] : _pins)
    {
        value += ((reg & pin) != 0 ? 1 : 0) << offset;
        offset++;
    }
    return value;
}

FOUNDATION_FACTORY_REGISTER(io::in::InByPCA9539,
                            "io::in::InByPCA9539",
                            io_in_InByPCA9539)
