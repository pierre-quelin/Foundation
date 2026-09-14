/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/out/OutByMCP2221.h"

#include "driver/chip/MCP2221Json.hpp"
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

vector<MCP2221::GPPin> readGpPins(const Node& node)
{
    vector<MCP2221::GPPin> pins;
    const Node gpPins = node.at("GPPins");
    for (std::size_t i = 0; i < gpPins.size(); ++i)
    {
        pins.emplace_back(gpPins[i].at("GPPin").as<MCP2221::GPPin>());
    }
    return pins;
}

} // namespace

OutByMCP2221::OutByMCP2221(const shared_ptr<MCP2221> device,
                           const vector<MCP2221::GPPin>& pins) :
    IOut(),
    _device(device),
    _pins(pins),
    _value(0)
{
}

OutByMCP2221::OutByMCP2221(ApplicationServices& app, Node node) :
    OutByMCP2221(obtain<MCP2221>(app, node, "MCP2221"), readGpPins(node))
{
}

OutByMCP2221::~OutByMCP2221()
{
    set(0);
}

int OutByMCP2221::init()
{
    // Restore the previous state before setting the pin mode to avoid unwanted switching.
    if (-1 == set(_value))
    {
        return -1;
    }

    for (const auto& pin : _pins)
    {
        if ((-1 == _device->setGPPinMode(pin, MCP2221::GPMode::GPIO)) &&
            (-1 == _device->setGPIODirection(pin, MCP2221::GPIODirection::OUTPUT)))
        {
            return -1;
        }
    }
    return 0;
}

int OutByMCP2221::set(unsigned int value)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _value = value;

    // Apply
    unsigned int offset = 0;
    for (const auto& pin : _pins)
    {
        MCP2221::GPIOState state = (((value >> offset) & 1) == 1) ? MCP2221::GPIOState::HIGH : MCP2221::GPIOState::LOW;
        if (-1 == _device->setGPIOState(pin, state))
        {
            return -1;
        }
        offset++;
    }

    return 0;
}

int OutByMCP2221::get(unsigned int& value) const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    value = _value;
    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::out::OutByMCP2221,
                            "io::out::OutByMCP2221",
                            io_out_OutByMCP2221)
