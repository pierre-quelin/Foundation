/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/in/IIn.h"
#include "io/out/IOut.h"
#include "sample/statemachine/MyDevice.h"
#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "tools/design/time/TimeManagerByTimer.hpp"
#include "tools/os/timer/Timer.h"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <memory>

using namespace sample::statemachine;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::time;
using namespace util::chrono;
using namespace util::chrono::literals;
using namespace tools::os::timer;

namespace sample::statemachine::test
{

class MockIn : public factory::IObject, public io::in::IIn
{
public:
    MockIn(ApplicationServices& app, Node node)
    {
        (void)app;
        (void)node;
    }

    int init() override { return 0; }

    int get(unsigned int& value) const override
    {
        value = _value.load();
        return 0;
    }

    void setValue(const unsigned int value)
    {
        _value.store(value);
        valueChanged(value);
    }

private:
    std::atomic<unsigned int> _value{0U};
};

class MockOut : public factory::IObject, public io::out::IOut
{
public:
    MockOut(ApplicationServices& app, Node node)
    {
        (void)app;
        (void)node;
    }

    int init() override { return 0; }

    int set(const unsigned int value) override
    {
        _value.store(value);
        return 0;
    }

    int get(unsigned int& value) const override
    {
        value = _value.load();
        return 0;
    }

    [[nodiscard]] unsigned int value() const { return _value.load(); }

private:
    std::atomic<unsigned int> _value{0U};
};

} // namespace sample::statemachine::test

FOUNDATION_FACTORY_REGISTER(sample::statemachine::test::MockIn,
                            "sample::statemachine::test::MockIn",
                            sample_statemachine_test_MockIn)

FOUNDATION_FACTORY_REGISTER(sample::statemachine::test::MockOut,
                            "sample::statemachine::test::MockOut",
                            sample_statemachine_test_MockOut)

namespace
{

constexpr const char* MyDeviceConfigJson = R"({
  "Device": {
    "TransferTimeout": "50ms",
    "InputOccupied": "inOccupied",
    "OutputTransfer": "outTransfer",
    "Objects": {
      "inOccupied": {
        "InstanceOf": "sample::statemachine::test::MockIn"
      },
      "outTransfer": {
        "InstanceOf": "sample::statemachine::test::MockOut"
      }
    }
  }
})";

struct MyDeviceFixture
{
    MyDeviceFixture()
    {
        center    = createLocalFromString(MyDeviceConfigJson);
        app.logs  = std::make_shared<util::logger::LogService>();
        app.timer = std::make_shared<Timer>();
        app.timer->start();
        app.timeManager = std::make_shared<TimeManagerByTimer>(*app.timer);
        app.config      = center;
        install(app);

        device = std::make_unique<MyDevice>(app, center->root()["Device"]);
        BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    }

    ~MyDeviceFixture()
    {
        device.reset();
        reset();
    }

    [[nodiscard]] std::shared_ptr<sample::statemachine::test::MockIn> mockIn()
    {
        return obtain<sample::statemachine::test::MockIn>(app, center->root()["Device"], "InputOccupied");
    }

    [[nodiscard]] std::shared_ptr<sample::statemachine::test::MockOut> mockOut()
    {
        return obtain<sample::statemachine::test::MockOut>(app, center->root()["Device"], "OutputTransfer");
    }

    ApplicationServices app;
    ConfigCenterPtr center;
    std::unique_ptr<MyDevice> device;
};

} // namespace

BOOST_FIXTURE_TEST_CASE(my_device_starts_ready_idle, MyDeviceFixture)
{
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_Idle);
}

BOOST_FIXTURE_TEST_CASE(my_device_transfer_timeout_to_no_tray, MyDeviceFixture)
{
    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_WaitingTray);

    ITimeManager::sleepFor(100_ms);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_NoTray);
}

BOOST_FIXTURE_TEST_CASE(my_device_loop_back_to_waiting_tray, MyDeviceFixture)
{
    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));

    ITimeManager::sleepFor(100_ms);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_NoTray);

    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_WaitingTray);
}

BOOST_FIXTURE_TEST_CASE(my_device_reset_returns_to_idle, MyDeviceFixture)
{
    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_WaitingTray);

    device->reset();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_Idle);
}

BOOST_FIXTURE_TEST_CASE(my_device_publishes_state_changes, MyDeviceFixture)
{
    std::atomic<int> notifications{0};
    MyDevice::State::Id last{MyDevice::State::Id::MAX};

    const auto cnx = device->state.connect([&](const MyDevice::State::Id id)
                                           {
        last = id;
        notifications.fetch_add(1); });

    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK_EQUAL(notifications.load(), 1);
    BOOST_CHECK(last == MyDevice::State::Id::Ready_WaitingTray);

    device->reset();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK_EQUAL(notifications.load(), 2);
    BOOST_CHECK(last == MyDevice::State::Id::Ready_Idle);

    (void)cnx;
}

BOOST_FIXTURE_TEST_CASE(my_device_transfer_denied_goes_to_no_tray, MyDeviceFixture)
{
    device.reset();
    if (app.instances != nullptr)
    {
        app.instances->clear();
    }
    center     = createLocalFromString(R"({
      "Device": {
        "CanTransfer": false,
        "TransferTimeout": "50ms",
        "InputOccupied": "inOccupied",
        "OutputTransfer": "outTransfer",
        "Objects": {
          "inOccupied": { "InstanceOf": "sample::statemachine::test::MockIn" },
          "outTransfer": { "InstanceOf": "sample::statemachine::test::MockOut" }
        }
      }
    })");
    app.config = center;
    device     = std::make_unique<MyDevice>(app, center->root()["Device"]);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));

    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_NoTray);
}

BOOST_FIXTURE_TEST_CASE(my_device_tray_present_reaches_tray, MyDeviceFixture)
{
    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_WaitingTray);
    BOOST_CHECK_EQUAL(mockOut()->value(), 1U);

    mockIn()->setValue(1U);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_Tray);
    BOOST_CHECK_EQUAL(mockOut()->value(), 0U);
}

BOOST_FIXTURE_TEST_CASE(my_device_tray_present_cancels_timeout, MyDeviceFixture)
{
    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));

    mockIn()->setValue(1U);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_Tray);

    ITimeManager::sleepFor(100_ms);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_Tray);
}

BOOST_FIXTURE_TEST_CASE(my_device_tray_absent_from_tray_goes_to_no_tray, MyDeviceFixture)
{
    device->transfer();
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));

    mockIn()->setValue(1U);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_Tray);

    mockIn()->setValue(0U);
    BOOST_REQUIRE(device->scheduler().synchronise(500_ms));
    BOOST_CHECK(device->stateId() == MyDevice::State::Id::Ready_NoTray);
}
