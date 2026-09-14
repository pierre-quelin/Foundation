/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SolenoidByPWM.h
 * @brief Solenoid driven by PWM with power-reduction timer.
 */
#pragma once

#include "actsens/solenoid/ISolenoid.h"
#include "io/pwm/IPWM.h"
#include "tools/design/objkit/ObjKit.hpp"
#include "tools/design/time/TimeRequest.hpp"

#include <memory>
#include <mutex>

namespace actsens::solenoid
{

class SolenoidByPWM : public ISolenoid, public tools::design::objkit::ObjKit
{
public:
    ~SolenoidByPWM() override;

    SolenoidByPWM(tools::design::ApplicationServices& app, tools::design::config::Node node);
    SolenoidByPWM(const SolenoidByPWM&)            = delete;
    SolenoidByPWM& operator=(const SolenoidByPWM&) = delete;
    SolenoidByPWM(SolenoidByPWM&&)                 = delete;
    SolenoidByPWM& operator=(SolenoidByPWM&&)      = delete;

    void on() override;
    void off() override;
    [[nodiscard]] State state() const override;

private:
    void onPowerReduction();
    void powerReduction(float percent);

    std::mutex _mutex;
    std::shared_ptr<io::pwm::IPWM> _pwm;
    tools::design::time::TimeRequest _powerReductionTimeout;
    float _holdDutyPercent;
};

} // namespace actsens::solenoid
