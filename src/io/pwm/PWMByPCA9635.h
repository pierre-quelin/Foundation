/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PWMByPCA9635.h
 * @brief A PCA9635 Pulse Width Modulators. Use a led driver as a PWM.
 */
#pragma once

#include "driver/chip/PCA9635.h"
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

class PWMByPCA9635 : public tools::design::factory::IObject, public IPWM
{
public:
    ~PWMByPCA9635() override;
    PWMByPCA9635(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    PWMByPCA9635(const PWMByPCA9635&)            = delete;
    PWMByPCA9635& operator=(const PWMByPCA9635&) = delete;
    PWMByPCA9635(PWMByPCA9635&&)                 = delete;
    PWMByPCA9635& operator=(PWMByPCA9635&&)      = delete;

    int init() override;

    [[nodiscard]] State state() const override;
    int state(const State& state) override;
    [[nodiscard]] DutyCycleCtrl dutyCycleCtrl() const override;
    int dutyCycleCtrl(const DutyCycleCtrl& /* DutyCycleCtrl.period fixed at 97kHz */ ctrl) override;

private:
    PWMByPCA9635(const std::shared_ptr<driver::chip::PCA9635> device,
                 const driver::chip::PCA9635::Led pin);

    std::shared_ptr<driver::chip::PCA9635> _device;
    driver::chip::PCA9635::Led _pin;

    DutyCycleCtrl _dutyCycleCtrl;
    State _state;
    mutable std::shared_mutex _mutex;
};

} // namespace io::pwm
