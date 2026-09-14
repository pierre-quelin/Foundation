/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/EventBusBoot.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/os/serport/ISerport.h"
#include "tools/os/serport/SerportGhost.hpp"
#include "util/chrono/Delay.hpp"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::design::ipc;
using namespace util::chrono::literals;

namespace tools::os::serport::test
{

class FakeSerport : public factory::IObject, public ISerport
{
public:
    FakeSerport(ApplicationServices& /*app*/, Node /*node*/) {}

    [[nodiscard]] int read(char* buffer, unsigned int size) override
    {
        return read(buffer, size, 0_ms);
    }

    [[nodiscard]] int read(char* buffer, unsigned int size, util::chrono::Delay delay) override
    {
        const auto deadline =
            std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::milliseconds>(delay.toNanoseconds());
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (!_rx.empty())
                {
                    const unsigned int n =
                        static_cast<unsigned int>(std::min(_rx.size(), static_cast<std::size_t>(size)));
                    for (unsigned int i = 0; i < n; ++i)
                    {
                        buffer[i] = _rx.front();
                        _rx.pop_front();
                    }
                    return static_cast<int>(n);
                }
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    [[nodiscard]] int write(const char* buffer, unsigned int size) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _rx.insert(_rx.end(), buffer, buffer + size);
        _ready = true;
        return static_cast<int>(size);
    }

    [[nodiscard]] int getNRead() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return static_cast<int>(_rx.size());
    }

    [[nodiscard]] int getNWrite() override { return 0; }
    int flush() override { return 0; }
    int wflush() override { return 0; }
    int rflush() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _rx.clear();
        return 0;
    }
    int cancel() override { return 0; }

    int setParams(BitRate bRate, DataBit nData, Parity parity, StopBit nStop) override
    {
        _bitrate   = bRate;
        _databit   = nData;
        _parity    = parity;
        _stopBit   = nStop;
        _paramsSet = true;
        return 0;
    }

    int setFlowCtrl(FlowCtrl flowCtrl) override
    {
        _flowctrl = flowCtrl;
        return 0;
    }

    [[nodiscard]] bool isReady() const override { return _ready; }
    int reset() override { return 0; }

    [[nodiscard]] bool paramsSet() const { return _paramsSet; }

private:
    mutable std::mutex _mutex;
    std::deque<char> _rx;
    bool _ready     = true;
    bool _paramsSet = false;
};

} // namespace tools::os::serport::test

FOUNDATION_FACTORY_REGISTER(tools::os::serport::test::FakeSerport,
                            "tools::os::serport::test::FakeSerport",
                            tools_os_serport_test_FakeSerport)

namespace
{

[[nodiscard]] ApplicationServices makeApp(const std::string& json)
{
    ApplicationServices app;
    app.logs   = std::make_shared<util::logger::LogService>();
    app.config = createLocalFromString(json);
    return app;
}

constexpr const char* EchoJson = R"({
  "EventBus": {
    "InstanceOf": "tools::design::ipc::EventBus",
    "AppName": "Sample",
    "PlatformName": "MachineA"
  },
  "Board": {
    "Objects": {
      "ComPort": {
        "InstanceOf": "tools::os::serport::test::FakeSerport",
        "Bridged": "tools::os::serport::SerportBridge"
      }
    }
  }
})";

} // namespace

BOOST_AUTO_TEST_CASE(serport_ghost_bridge_echo)
{
    auto app = makeApp(EchoJson);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto port  = createShared<tools::os::serport::ISerport>(app, app.config->root()["Board"], "ComPort");
    auto ghost = createAs<tools::os::serport::SerportGhost>(
        app, app.config->root()["Board"]["Objects"]["ComPort"], "tools::os::serport::SerportGhost");
    auto* fake = dynamic_cast<tools::os::serport::test::FakeSerport*>(port.get());
    BOOST_REQUIRE(fake != nullptr);
    BOOST_REQUIRE(app.instances);
    BOOST_CHECK(app.instances->contains("Board.ComPort"));
    BOOST_CHECK(app.instances->contains(derivedInstanceKey("Board.ComPort", "bridge")));

    ghost->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const char* msg = "AT";
    BOOST_CHECK_EQUAL(ghost->write(msg, 2), 2);

    char buf[8] = {};
    int n       = 0;
    for (int i = 0; i < 40 && n <= 0; ++i)
    {
        n = ghost->read(buf, sizeof(buf), 50_ms);
    }
    BOOST_CHECK_EQUAL(n, 2);
    BOOST_CHECK_EQUAL(std::string(buf, buf + n), "AT");

    BOOST_CHECK_EQUAL(ghost->setParams(tools::os::serport::ISerport::BitRate::BITRATE_115200,
                                       tools::os::serport::ISerport::DataBit::DATABIT_8,
                                       tools::os::serport::ISerport::Parity::PARITY_NONE,
                                       tools::os::serport::ISerport::StopBit::STOPBIT_1),
                      0);
    BOOST_CHECK(fake->paramsSet());

    ghost->stop();
}

BOOST_AUTO_TEST_CASE(serport_ghost_offline_write_fails)
{
    constexpr const char* json = R"({
      "EventBus": {
        "InstanceOf": "tools::design::ipc::EventBus",
        "AppName": "Sample",
        "PlatformName": "MachineA"
      },
      "ComPortGhost": {
        "InstanceOf": "tools::os::serport::SerportGhost"
      }
    })";

    auto app = makeApp(json);
    wireEventBus(app, app.config->root()["EventBus"]);

    auto ghost = createShared<tools::os::serport::SerportGhost>(app, app.config->root()["ComPortGhost"]);
    ghost->start();
    app.eventBus->stop();
    BOOST_CHECK_EQUAL(ghost->write("x", 1), -1);
    ghost->stop();
}
