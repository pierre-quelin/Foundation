/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "actsens/solenoid/SolenoidByPWM.h"
#include "io/pwm/IPWM.h"
#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/InstanceRegistry.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"
#include "tools/design/time/SimpleTimeManager.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>

using namespace actsens::solenoid;
using namespace io::pwm;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::scheduler;
using namespace tools::design::time;
using namespace util::chrono::literals;

namespace
{

class FakePwm final : public factory::IObject, public IPWM
{
public:
    FakePwm(tools::design::ApplicationServices& /*app*/, tools::design::config::Node /*node*/) {}

    int init() override { return 0; }

    [[nodiscard]] State state() const override
    {
        const std::lock_guard lock(_mutex);
        return _state;
    }

    int state(const State& state) override
    {
        const std::lock_guard lock(_mutex);
        _state = state;
        return 0;
    }

    [[nodiscard]] DutyCycleCtrl dutyCycleCtrl() const override
    {
        const std::lock_guard lock(_mutex);
        return _duty;
    }

    int dutyCycleCtrl(const DutyCycleCtrl& value) override
    {
        const std::lock_guard lock(_mutex);
        _duty = value;
        return 0;
    }

private:
    mutable std::mutex _mutex;
    State _state{State::Off};
    DutyCycleCtrl _duty{};
};

} // namespace

FOUNDATION_FACTORY_REGISTER(FakePwm, "test::FakePwm", test_FakePwm)

BOOST_AUTO_TEST_CASE(solenoid_by_pwm_reduces_duty_after_delay)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1, "MaxEventCount": 32 },
      "Solenoid": {
        "InstanceOf": "actsens::solenoid::SolenoidByPWM",
        "PWM": {
          "InstanceOf": "test::FakePwm"
        },
        "PowerReductionDelay": "50ms",
        "HoldDutyPercent": 40
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config      = center;
    app.logs        = std::make_shared<util::logger::LogService>();
    app.timeManager = std::make_shared<SimpleTimeManager>();
    app.instances   = std::make_shared<InstanceRegistry>();
    install(app);

    auto solenoid = create<SolenoidByPWM>(app, center->root()["Solenoid"]);
    BOOST_REQUIRE(solenoid);

    auto pwm = std::dynamic_pointer_cast<FakePwm>(
        obtain<IPWM>(app, center->root()["Solenoid"], "PWM"));
    BOOST_REQUIRE(pwm);

    solenoid->on();
    BOOST_CHECK(solenoid->state() == ISolenoid::State::On);
    BOOST_CHECK(std::abs(pwm->dutyCycleCtrl().percent - 100.f) < 0.1f);

    app.timeManagerService().sleep(80_ms);
    BOOST_REQUIRE(solenoid->scheduler().synchronise(500_ms));

    BOOST_CHECK(solenoid->state() == ISolenoid::State::On);
    BOOST_CHECK(std::abs(pwm->dutyCycleCtrl().percent - 40.f) < 0.1f);

    solenoid->off();
    BOOST_CHECK(solenoid->state() == ISolenoid::State::Off);
    reset();
}

BOOST_AUTO_TEST_CASE(solenoid_by_pwm_off_cancels_power_reduction)
{
    constexpr const char* json = R"({
      "SchedulerPool": { "SharedSchedulerCount": 1, "MaxEventCount": 32 },
      "Solenoid": {
        "InstanceOf": "actsens::solenoid::SolenoidByPWM",
        "PWM": {
          "InstanceOf": "test::FakePwm"
        },
        "PowerReductionDelay": "200ms",
        "HoldDutyPercent": 50
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config      = center;
    app.logs        = std::make_shared<util::logger::LogService>();
    app.timeManager = std::make_shared<SimpleTimeManager>();
    app.instances   = std::make_shared<InstanceRegistry>();
    install(app);

    auto solenoid = create<SolenoidByPWM>(app, center->root()["Solenoid"]);
    BOOST_REQUIRE(solenoid);

    auto pwm = std::dynamic_pointer_cast<FakePwm>(
        obtain<IPWM>(app, center->root()["Solenoid"], "PWM"));
    BOOST_REQUIRE(pwm);

    solenoid->on();
    BOOST_CHECK(std::abs(pwm->dutyCycleCtrl().percent - 100.f) < 0.1f);
    solenoid->off();

    app.timeManagerService().sleep(250_ms);
    BOOST_REQUIRE(solenoid->scheduler().synchronise(500_ms));

    BOOST_CHECK(solenoid->state() == ISolenoid::State::Off);
    BOOST_CHECK(std::abs(pwm->dutyCycleCtrl().percent - 100.f) < 0.1f);
    reset();
}
