/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/adc/ADCByMCP2221.h"

#include "driver/chip/MCP2221Json.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

#include <limits>

using namespace driver::chip;
using namespace io::adc;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

ADCByMCP2221::ADCByMCP2221(const shared_ptr<MCP2221> device,
                           const MCP2221::GPPin pin) :
    IADC(),
    _device(device),
    _gpPin(pin)
{
    _cnx = _device->update.connect([&](auto adcDatas)
                                   { refreshValues(adcDatas); });
}

ADCByMCP2221::ADCByMCP2221(ApplicationServices& app, Node node) :
    ADCByMCP2221(obtain<MCP2221>(app, node, "MCP2221"), node.at("GPPin").as<MCP2221::GPPin>())
{
}

int ADCByMCP2221::init()
{
    if ((-1 == _device->setGPPinMode(_gpPin, MCP2221::GPMode::ADC)) ||
        (-1 == _device->setADCVRef(MCP2221::GPVRef::VREF_VDD)))
    {
        return -1;
    }
    return 0;
}

int ADCByMCP2221::get(double& value) const
{
    unsigned int tmp = std::numeric_limits<unsigned int>::max();
    if (-1 == _device->getADCData(_gpPin, tmp))
    {
        return -1;
    }
    value = static_cast<double>(tmp);
    return 0;
}

int ADCByMCP2221::refreshValues(std::array<unsigned int, 3>& adcDatas)
{
    unsigned int value = adcDatas[static_cast<int>(_gpPin) - 1];

    // Has changed since last refreshValue
    if (value != _value)
    {
        _value = value;
        // Informs observers
        valueChanged(static_cast<double>(_value));
    }

    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::adc::ADCByMCP2221,
                            "io::adc::ADCByMCP2221",
                            io_adc_ADCByMCP2221)
