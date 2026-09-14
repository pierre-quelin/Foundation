/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "actsens/solenoid/SolenoidByPWM.h"

#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <chrono>
#include <exception>

using namespace actsens::solenoid;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace util::chrono::literals;
using namespace util::logger;

SolenoidByPWM::SolenoidByPWM(ApplicationServices& app, Node node) : ObjKit(app, node), _powerReductionTimeout(config().value_or("PowerReductionDelay", 2_s),
                                                                                                              false,
                                                                                                              "SolenoidByPWMPowerReduction"),
                                                                    _holdDutyPercent(static_cast<float>(config().value_or("HoldDutyPercent", 50.0)))
{
    needLogger();
    needScheduler();

    _pwm = obtain<io::pwm::IPWM>(services(), config(), "PWM", /*createIfMissing=*/true);

    _powerReductionTimeout.setTarget(scheduler(), &SolenoidByPWM::onPowerReduction, *this);
}

SolenoidByPWM::~SolenoidByPWM()
{
    try
    {
        if (_powerReductionTimeout.isRunning())
        {
            services().timeManagerService().cancel(_powerReductionTimeout);
        }
    }
    catch (const std::exception&)
    {
        // TimeManager may already be torn down at process shutdown.
    }

    (void)drainScheduler();

    const std::lock_guard<std::mutex> lock(_mutex);

    if (_pwm != nullptr && 0 != _pwm->state(io::pwm::IPWM::State::Off))
    {
        logger().log(LogService::LogLevel::ERROR, "SolenoidByPWM::~SolenoidByPWM - state Off error");
        return;
    }

    logger().log(LogService::LogLevel::INFO, "SolenoidByPWM - Off");
}

void SolenoidByPWM::on()
{
    {
        const std::lock_guard<std::mutex> lock(_mutex);

        io::pwm::IPWM::DutyCycleCtrl dcy = _pwm->dutyCycleCtrl();
        dcy.percent                      = 100.f;
        if (0 != _pwm->dutyCycleCtrl(dcy))
        {
            logger().log(LogService::LogLevel::ERROR, "SolenoidByPWM::on - dutyCycleCtrl error");
            return;
        }
        if (0 != _pwm->state(io::pwm::IPWM::State::On))
        {
            logger().log(LogService::LogLevel::ERROR, "SolenoidByPWM::on - state On error");
            return;
        }

        logger().log(LogService::LogLevel::INFO, "SolenoidByPWM - On");
    }

    services().timeManagerService().forceArm(_powerReductionTimeout);
}

void SolenoidByPWM::off()
{
    if (_powerReductionTimeout.isRunning())
    {
        services().timeManagerService().cancel(_powerReductionTimeout);
    }

    const std::lock_guard<std::mutex> lock(_mutex);

    if (0 != _pwm->state(io::pwm::IPWM::State::Off))
    {
        logger().log(LogService::LogLevel::ERROR, "SolenoidByPWM::off - state Off error");
        return;
    }

    logger().log(LogService::LogLevel::INFO, "SolenoidByPWM - Off");
}

SolenoidByPWM::State SolenoidByPWM::state() const
{
    return static_cast<SolenoidByPWM::State>(_pwm->state());
}

void SolenoidByPWM::onPowerReduction()
{
    const std::lock_guard<std::mutex> lock(_mutex);
    powerReduction(_holdDutyPercent);
}

void SolenoidByPWM::powerReduction(float percent)
{
    io::pwm::IPWM::DutyCycleCtrl dcy = _pwm->dutyCycleCtrl();
    dcy.percent                      = percent;
    if (0 != _pwm->dutyCycleCtrl(dcy))
    {
        logger().log(LogService::LogLevel::ERROR, "SolenoidByPWM::powerReduction - dutyCycleCtrl error");
        return;
    }

    logger().log(LogService::LogLevel::INFO, "SolenoidByPWM - Power reduction");
}

FOUNDATION_FACTORY_REGISTER(actsens::solenoid::SolenoidByPWM,
                            "actsens::solenoid::SolenoidByPWM",
                            actsens_solenoid_SolenoidByPWM)
