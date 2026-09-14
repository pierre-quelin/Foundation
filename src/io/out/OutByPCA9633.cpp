/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/out/OutByPCA9633.h"

#include "driver/chip/PCA9633Json.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

#include <set>

using namespace driver::chip;
using namespace io::out;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

namespace
{

vector<PCA9633::Led> readLeds(const Node& node)
{
    vector<PCA9633::Led> leds;
    const Node ledList = node.at("Leds");
    for (std::size_t i = 0; i < ledList.size(); ++i)
    {
        leds.emplace_back(ledList[i].at("Led").as<PCA9633::Led>());
    }
    return leds;
}

} // namespace

OutByPCA9633::OutByPCA9633(const shared_ptr<PCA9633> device,
                           const vector<PCA9633::Led>& pins) :
    IOut(),
    _device(device),
    _leds(pins),
    _value(0)
{
}

OutByPCA9633::OutByPCA9633(ApplicationServices& app, Node node) :
    OutByPCA9633(obtain<PCA9633>(app, node, "PCA9633"), readLeds(node))
{
}

OutByPCA9633::~OutByPCA9633()
{
    set(0);
}

int OutByPCA9633::init()
{
    // Restore previous state
    if (-1 == set(_value))
    {
        return -1;
    }

    return 0;
}

int OutByPCA9633::set(unsigned int value)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _value = value;

    // Apply
    unsigned int offset = 0;
    for (const auto& led : _leds)
    {
        PCA9633::LedDriver drv = (((value >> offset) & 1) == 1) ? PCA9633::LedDriver::LDRx_ON : PCA9633::LedDriver::LDRx_OFF;
        if (-1 == _device->setLedDrvOut(led, drv))
        {
            return -1;
        }
        offset++;
    }

    return 0;
}

int OutByPCA9633::get(unsigned int& value) const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    value = _value;
    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::out::OutByPCA9633,
                            "io::out::OutByPCA9633",
                            io_out_OutByPCA9633)
