/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EsploraSwitch.h
 * @brief Stub digital input for Arduino Esplora (PCB switch1..switch4).
 */
#pragma once

#include "io/in/IIn.h"
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

class EsploraSwitch : public tools::design::factory::IObject, public io::in::IIn
{
public:
    ~EsploraSwitch() override = default;
    EsploraSwitch(tools::design::ApplicationServices& app,
                  tools::design::config::Node node);
    EsploraSwitch(const EsploraSwitch&)            = delete;
    EsploraSwitch& operator=(const EsploraSwitch&) = delete;
    EsploraSwitch(EsploraSwitch&&)                 = delete;
    EsploraSwitch& operator=(EsploraSwitch&&)      = delete;

    int init() override;
    int get(unsigned int& value) const override;

    /** @brief HID pump — updates cache and emits valueChanged when changed. */
    void push(unsigned int value);

private:
    unsigned int _value{0};
    bool _inited{false};
};

} // namespace driver::board
