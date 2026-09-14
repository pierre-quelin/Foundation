/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ADCByMCP2221.h
 * @brief MCP2221 Analog Digital Converter input which used the internal 10-bit ADC. (GP1, GP2 or GP3)
 */
#pragma once

#include "driver/chip/MCP2221.h"
#include "io/adc/IADC.h"
#include "tools/design/factory/IObject.hpp"

#include <array>
#include <memory>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace io::adc
{

class ADCByMCP2221 : public tools::design::factory::IObject, public IADC
{
public:
    ~ADCByMCP2221() override = default;
    ADCByMCP2221(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    ADCByMCP2221(const ADCByMCP2221&)            = delete;
    ADCByMCP2221& operator=(const ADCByMCP2221&) = delete;
    ADCByMCP2221(ADCByMCP2221&&)                 = delete;
    ADCByMCP2221& operator=(ADCByMCP2221&&)      = delete;

    int init() override;

    int get(double& value) const override;

    int refreshValues(std::array<unsigned int, 3>& adcDatas);

private:
    ADCByMCP2221(const std::shared_ptr<driver::chip::MCP2221> device,
                 const driver::chip::MCP2221::GPPin pin);

    std::shared_ptr<driver::chip::MCP2221> _device;
    driver::chip::MCP2221::GPPin _gpPin;

    unsigned int _value;
    boost::signals2::scoped_connection _cnx; // RAII
};

} // namespace io::adc
