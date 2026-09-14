/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file RGBWLedByPCA9635.h
 * @brief A PCA9635 RGBWLed. Can use less than 4 colors.
 */
#pragma once

#include "driver/chip/PCA9635.h"
#include "io/led/IRGBWLed.h"
#include "tools/design/factory/IObject.hpp"

#include <map>
#include <memory>
#include <shared_mutex>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace io::led
{

class RGBWLedByPCA9635 : public tools::design::factory::IObject, public IRGBWLed
{
public:
    ~RGBWLedByPCA9635() override;
    RGBWLedByPCA9635(tools::design::ApplicationServices& app,
                     tools::design::config::Node node);
    RGBWLedByPCA9635(const RGBWLedByPCA9635&)            = delete;
    RGBWLedByPCA9635& operator=(const RGBWLedByPCA9635&) = delete;
    RGBWLedByPCA9635(RGBWLedByPCA9635&&)                 = delete;
    RGBWLedByPCA9635& operator=(RGBWLedByPCA9635&&)      = delete;

    int init() override;
    [[nodiscard]] State state() const override;
    int state(const State& state) override;
    [[nodiscard]] BlinkCtrl blinkCtrl() const override;
    int blinkCtrl(const BlinkCtrl& blinkCtrl) override;

    [[nodiscard]] Color brightness() const override;
    int brightness(const Color& color) override;

private:
    RGBWLedByPCA9635(const std::shared_ptr<driver::chip::PCA9635> device,
                     const std::map<IRGBWLed::ColorId, driver::chip::PCA9635::Led>& mapping);

    std::shared_ptr<driver::chip::PCA9635> _device;
    std::map<IRGBWLed::ColorId, driver::chip::PCA9635::Led> _mapping;

    ILed::State _state;
    Color _color;
    BlinkCtrl _blinkCtrl;
    mutable std::shared_mutex _mutex;
};

} // namespace io::led
