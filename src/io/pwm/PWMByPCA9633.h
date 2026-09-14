/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PWMByPCA9633.h
 * @brief A PCA9633 Pulse Width Modulators. Use a led driver as a PWM.
 */
#pragma once

#include "driver/chip/PCA9633.h"
#include "io/pwm/IPWM.h"
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

namespace io::pwm
{

class PWMByPCA9633 : public tools::design::factory::IObject, public IPWM
{
public:
    ~PWMByPCA9633() override;
    PWMByPCA9633(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    PWMByPCA9633(const PWMByPCA9633&)            = delete;
    PWMByPCA9633& operator=(const PWMByPCA9633&) = delete;
    PWMByPCA9633(PWMByPCA9633&&)                 = delete;
    PWMByPCA9633& operator=(PWMByPCA9633&&)      = delete;

    int init() override;

    [[nodiscard]] State state() const override;
    int state(const State& state) override;
    [[nodiscard]] DutyCycleCtrl dutyCycleCtrl() const override;
    int dutyCycleCtrl(const DutyCycleCtrl& /* DutyCycleCtrl.period fixed at 97kHz */ ctrl) override;

private:
    PWMByPCA9633(const std::shared_ptr<driver::chip::PCA9633> device,
                 const driver::chip::PCA9633::Led pin);

    std::shared_ptr<driver::chip::PCA9633> _device;
    driver::chip::PCA9633::Led _pin;

    DutyCycleCtrl _dutyCycleCtrl;
    State _state;
    mutable std::shared_mutex _mutex;
};

} // namespace io::pwm
