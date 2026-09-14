/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/out/OutByPCA9635.h"

#include "driver/chip/PCA9635Json.hpp"
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

vector<PCA9635::Led> readLeds(const Node& node)
{
    vector<PCA9635::Led> leds;
    const Node ledList = node.at("Leds");
    for (std::size_t i = 0; i < ledList.size(); ++i)
    {
        leds.emplace_back(ledList[i].at("Led").as<PCA9635::Led>());
    }
    return leds;
}

} // namespace

OutByPCA9635::OutByPCA9635(const shared_ptr<PCA9635> device,
                           const vector<PCA9635::Led>& pins) :
    IOut(),
    _device(device),
    _leds(pins),
    _value(0)
{
}

OutByPCA9635::OutByPCA9635(ApplicationServices& app, Node node) :
    OutByPCA9635(obtain<PCA9635>(app, node, "PCA9635"), readLeds(node))
{
}

OutByPCA9635::~OutByPCA9635()
{
    set(0);
}

int OutByPCA9635::init()
{
    // Restore previous state
    if (-1 == set(_value))
    {
        return -1;
    }

    return 0;
}

int OutByPCA9635::set(unsigned int value)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _value = value;

    // Apply
    unsigned int offset = 0;
    for (const auto& led : _leds)
    {
        PCA9635::LedDriver drv = (((value >> offset) & 1) == 1) ? PCA9635::LedDriver::LDRx_ON : PCA9635::LedDriver::LDRx_OFF;
        if (-1 == _device->setLedDrvOut(led, drv))
        {
            return -1;
        }
        offset++;
    }

    return 0;
}

int OutByPCA9635::get(unsigned int& value) const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    value = _value;
    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::out::OutByPCA9635,
                            "io::out::OutByPCA9635",
                            io_out_OutByPCA9635)
