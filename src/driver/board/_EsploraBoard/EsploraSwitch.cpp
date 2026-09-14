/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 */

#include "EsploraSwitch.h"

#include "tools/design/factory/Register.hpp"

using namespace driver::board;

EsploraSwitch::EsploraSwitch(tools::design::ApplicationServices& /*app*/,
                             tools::design::config::Node /*node*/)
{
}

int EsploraSwitch::init()
{
    _inited = true;
    return 0;
}

int EsploraSwitch::get(unsigned int& value) const
{
    if (!_inited)
    {
        return -1;
    }
    value = _value;
    return 0;
}

void EsploraSwitch::push(unsigned int value)
{
    if (_value == value)
    {
        return;
    }
    _value = value;
    valueChanged(_value);
}

FOUNDATION_FACTORY_REGISTER(driver::board::EsploraSwitch,
                            "driver::board::EsploraSwitch",
                            driver_board_EsploraSwitch)
