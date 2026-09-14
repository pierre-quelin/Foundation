/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EsploraLightSensor.h
 * @brief Stub light sensor ADC for Arduino Esplora (instance lightSensor).
 */
#pragma once

#include "io/adc/IADC.h"
#include "tools/design/factory/IObject.hpp"

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace driver::board
{

class EsploraLightSensor : public tools::design::factory::IObject, public io::adc::IADC
{
public:
    ~EsploraLightSensor() override = default;
    EsploraLightSensor(tools::design::ApplicationServices& app,
                       tools::design::config::Node node);
    EsploraLightSensor(const EsploraLightSensor&)            = delete;
    EsploraLightSensor& operator=(const EsploraLightSensor&) = delete;
    EsploraLightSensor(EsploraLightSensor&&)                 = delete;
    EsploraLightSensor& operator=(EsploraLightSensor&&)      = delete;

    int init() override;
    int get(double& value) const override;

    /** @brief HID pump — updates cache and emits valueChanged when changed. */
    void push(double value);

private:
    double _value{0.};
    bool _inited{false};
};

} // namespace driver::board
