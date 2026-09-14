/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file RGBWLedByPCA9633.h
 * @brief A PCA9633 RGBWLed. Can use less than 4 colors.
 */
#pragma once

#include "driver/chip/PCA9633.h"
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

class RGBWLedByPCA9633 : public tools::design::factory::IObject, public IRGBWLed
{
public:
    ~RGBWLedByPCA9633() override;
    RGBWLedByPCA9633(tools::design::ApplicationServices& app,
                     tools::design::config::Node node);
    RGBWLedByPCA9633(const RGBWLedByPCA9633&)            = delete;
    RGBWLedByPCA9633& operator=(const RGBWLedByPCA9633&) = delete;
    RGBWLedByPCA9633(RGBWLedByPCA9633&&)                 = delete;
    RGBWLedByPCA9633& operator=(RGBWLedByPCA9633&&)      = delete;

    int init() override;
    [[nodiscard]] State state() const override;
    int state(const State& state) override;
    [[nodiscard]] BlinkCtrl blinkCtrl() const override;
    int blinkCtrl(const BlinkCtrl& blinkCtrl) override;

    [[nodiscard]] Color brightness() const override;
    int brightness(const Color& color) override;

private:
    RGBWLedByPCA9633(const std::shared_ptr<driver::chip::PCA9633> device,
                     const std::map<IRGBWLed::ColorId, driver::chip::PCA9633::Led>& mapping);

    std::shared_ptr<driver::chip::PCA9633> _device;
    std::map<IRGBWLed::ColorId, driver::chip::PCA9633::Led> _mapping;

    ILed::State _state;
    Color _color;
    BlinkCtrl _blinkCtrl;
    mutable std::shared_mutex _mutex;
};

} // namespace io::led
