/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "EsploraLightSensor.h"

#include "tools/design/factory/Register.hpp"

using namespace driver::board;

EsploraLightSensor::EsploraLightSensor(tools::design::ApplicationServices& /*app*/,
                                       tools::design::config::Node /*node*/)
{
}

int EsploraLightSensor::init()
{
    _inited = true;
    return 0;
}

int EsploraLightSensor::get(double& value) const
{
    if (!_inited)
    {
        return -1;
    }
    value = _value;
    return 0;
}

void EsploraLightSensor::push(double value)
{
    if (_value == value)
    {
        return;
    }
    _value = value;
    valueChanged(_value);
}

FOUNDATION_FACTORY_REGISTER(driver::board::EsploraLightSensor,
                            "driver::board::EsploraLightSensor",
                            driver_board_EsploraLightSensor)
