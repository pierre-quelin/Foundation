/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LedByPCA9539.h
 * @brief A PCA9539 I/O Output used as a Led driver.
 */
#pragma once

#include "driver/chip/PCA9539.h"
#include "io/led/ILed.h"
#include "tools/design/factory/IObject.hpp"

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

class LedByPCA9539 : public tools::design::factory::IObject, public ILed
{
public:
    ~LedByPCA9539() override;
    LedByPCA9539(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    LedByPCA9539(const LedByPCA9539&)            = delete;
    LedByPCA9539& operator=(const LedByPCA9539&) = delete;
    LedByPCA9539(LedByPCA9539&&)                 = delete;
    LedByPCA9539& operator=(LedByPCA9539&&)      = delete;

    int init() override;

    [[nodiscard]] State state() const override;
    int state(const State& state) override;
    [[nodiscard]] BlinkCtrl blinkCtrl() const override;
    int blinkCtrl(const BlinkCtrl& blinkCtrl) override;

private:
    LedByPCA9539(const std::shared_ptr<driver::chip::PCA9539> device,
                 const driver::chip::PCA9539::Pin pin);

    std::shared_ptr<driver::chip::PCA9539> _device;
    driver::chip::PCA9539::Pin _pin;

    State _state;
    mutable std::shared_mutex _mutex;
};

} // namespace io::led
