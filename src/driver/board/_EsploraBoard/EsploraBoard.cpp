/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
*
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "EsploraBoard.h"

#include "EsploraLightSensor.h"
#include "EsploraRGBLed.h"
#include "EsploraSwitch.h"

#include "tools/design/config/ResolvePlatform.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "util/chrono/Delay.hpp"
#include "util/json/Json.hpp"
#include "util/logger/Logger.hpp"

#include <esplora/hid/esplora_hid.hpp>

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace driver::board;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace util::json;
using namespace util::logger;
using util::chrono::literals::operator""_ms;

namespace
{

template <typename Interface, typename Concrete>
[[nodiscard]] shared_ptr<Interface> createFixedIo(ApplicationServices& app,
                                                  const Node& boardCfg,
                                                  string_view name)
{
    if (boardCfg.contains("Objects") && boardCfg["Objects"].contains(name))
    {
        const Node overlay = boardCfg["Objects"][name];
        PlatformResolvedNode resolved(overlay, app.platformName);
        const Node effective = resolved.get();
        if (effective.contains("InstanceOf"))
        {
            return createShared<Interface>(app, boardCfg, name);
        }
        return createSharedNamed<Concrete>(app, boardCfg, name, effective.toJson());
    }
    return createSharedNamed<Concrete>(app, boardCfg, name);
}

template <typename T>
void launchChild(const shared_ptr<T>& obj)
{
    if (obj == nullptr)
    {
        return;
    }
    if (auto* launchable = dynamic_cast<ILaunchable*>(obj.get()))
    {
        launchable->launch();
    }
}

} // namespace

EsploraBoard::EsploraBoard(ApplicationServices& app, Node node) :
    ObjKit(app, std::move(node)),
    _state(State::NotReady)
{
    needLogger();

    try
    {
        auto& svc      = services();
        const Node cfg = config();

        static constexpr string_view switches[] = {"switch1", "switch2", "switch3", "switch4"};
        for (const string_view name : switches)
        {
            _inputs.emplace(string{name},
                            createFixedIo<io::in::IIn, EsploraSwitch>(svc, cfg, name));
        }

        _RGBWLeds.emplace(
            "rgbLed", createFixedIo<io::led::IRGBWLed, EsploraRGBLed>(svc, cfg, "rgbLed"));
        _ADCs.emplace(
            "lightSensor",
            createFixedIo<io::adc::IADC, EsploraLightSensor>(svc, cfg, "lightSensor"));

        wireRgbSink();
    }
    catch (const runtime_error& e)
    {
        logger().log(LogService::LogLevel::ERROR, "EsploraBoard - {}", e.what());
        throw;
    }
}

EsploraBoard::~EsploraBoard()
{
    stopHidPump();
    clearRgbSink();
    if (_hid && _hid->isOpen())
    {
        _hid->close();
    }
}

void EsploraBoard::wireRgbSink()
{
    const auto it = _RGBWLeds.find("rgbLed");
    if (it == _RGBWLeds.end())
    {
        return;
    }
    auto* led = dynamic_cast<EsploraRGBLed*>(it->second.get());
    if (led == nullptr)
    {
        return;
    }
    led->setRgbSink([this](std::uint8_t red, std::uint8_t green, std::uint8_t blue)
                    {
        if (_hid && _hid->isOpen())
        {
            (void)_hid->writeRgb(red, green, blue);
        } });
}

void EsploraBoard::clearRgbSink()
{
    const auto it = _RGBWLeds.find("rgbLed");
    if (it == _RGBWLeds.end())
    {
        return;
    }
    if (auto* led = dynamic_cast<EsploraRGBLed*>(it->second.get()))
    {
        led->setRgbSink({});
    }
}

void EsploraBoard::startHidPump()
{
    if (_hidPumpRunning.exchange(true))
    {
        return;
    }
    // Dedicated OS thread: blocking interrupt IN must not sit on Shared EventScheduler.
    _hidPump = make_unique<tools::os::thread::Thread>([this]
                                                      { hidPumpLoop(); },
                                                      "EsploraHid");
    _hidPump->start();
}

void EsploraBoard::stopHidPump()
{
    const bool wasRunning = _hidPumpRunning.exchange(false);
    // Unblock a pending infinite interrupt IN (if any).
    if (_hid && _hid->isOpen())
    {
        _hid->close();
    }
    if (_hidPump)
    {
        _hidPump->join();
        _hidPump.reset();
    }
    (void)wasRunning;
}

void EsploraBoard::hidPumpLoop()
{
    while (_hidPumpRunning.load(memory_order_relaxed))
    {
        if (!_hid || !_hid->isOpen())
        {
            break;
        }

        esplora::hid::InputSnapshot snap;
        // Event-driven firmware: block until IN (switch change or OUT sync).
        // timeout 0 = wait forever in kernel — no PC poll / CPU spin.
        if (_hid->readInput(snap, 0))
        {
            auto pushSwitch = [this](string_view name, bool pressed)
            {
                const auto it = _inputs.find(string{name});
                if (it == _inputs.end())
                {
                    return;
                }
                if (auto* sw = dynamic_cast<EsploraSwitch*>(it->second.get()))
                {
                    sw->push(pressed ? 1u : 0u);
                }
            };
            pushSwitch("switch1", snap.switch1);
            pushSwitch("switch2", snap.switch2);
            pushSwitch("switch3", snap.switch3);
            pushSwitch("switch4", snap.switch4);

            const auto adcIt = _ADCs.find("lightSensor");
            if (adcIt != _ADCs.end())
            {
                if (auto* light = dynamic_cast<EsploraLightSensor*>(adcIt->second.get()))
                {
                    light->push(static_cast<double>(snap.lightSensor));
                }
            }
            continue;
        }

        if (!_hidPumpRunning.load(memory_order_relaxed))
        {
            break;
        }

        const auto err = _hid->lastError();
        if (err == esplora::hid::Error::Timeout)
        {
            continue;
        }
        if (err == esplora::hid::Error::NotConnected)
        {
            break;
        }
        if (err != esplora::hid::Error::None)
        {
            logger().log(LogService::LogLevel::ERROR,
                         "EsploraBoard HID pump error {} ({}) — stopping",
                         static_cast<int>(err),
                         _hid->lastLibusbErrorName());
            break;
        }
        tools::os::thread::Thread::sleep_for(10_ms);
    }
    _hidPumpRunning.store(false, memory_order_relaxed);
}

void EsploraBoard::launch()
{
    if (init() != 0)
    {
        throw runtime_error("EsploraBoard::launch - init failed");
    }
    for (const auto& [_, in] : _inputs)
    {
        launchChild(in);
    }
    for (const auto& [_, led] : _RGBWLeds)
    {
        launchChild(led);
    }
    for (const auto& [_, adc] : _ADCs)
    {
        launchChild(adc);
    }
}

io::board::IBoard::State EsploraBoard::state()
{
    return _state;
}

shared_ptr<io::in::IIn> EsploraBoard::getInput(const string& name) const
{
    const auto it = _inputs.find(name);
    if (it == _inputs.end())
    {
        logger().log(LogService::LogLevel::ERROR, "EsploraBoard::getInput - {} Not found", name);
        return nullptr;
    }
    return it->second;
}

shared_ptr<io::out::IOut> EsploraBoard::getOutput(const string& /*name*/) const
{
    return nullptr;
}

shared_ptr<io::led::ILed> EsploraBoard::getLed(const string& /*name*/) const
{
    return nullptr;
}

shared_ptr<io::led::IRGBWLed> EsploraBoard::getRGBWLed(const string& name) const
{
    const auto it = _RGBWLeds.find(name);
    if (it == _RGBWLeds.end())
    {
        logger().log(LogService::LogLevel::INFO, "EsploraBoard::getRGBWLed - {} Not found", name);
        return nullptr;
    }
    return it->second;
}

shared_ptr<io::pwm::IPWM> EsploraBoard::getPWM(const string& /*name*/) const
{
    return nullptr;
}

shared_ptr<io::adc::IADC> EsploraBoard::getADC(const string& name) const
{
    const auto it = _ADCs.find(name);
    if (it == _ADCs.end())
    {
        logger().log(LogService::LogLevel::INFO, "EsploraBoard::getADC - {} Not found", name);
        return nullptr;
    }
    return it->second;
}

int EsploraBoard::init()
{
    logger().log(LogService::LogLevel::INFO, "EsploraBoard::init");

    for (const auto& [name, in] : _inputs)
    {
        if (in->init() != 0)
        {
            logger().log(LogService::LogLevel::ERROR, "EsploraBoard::init - {} failed", name);
            return -1;
        }
    }
    for (const auto& [name, led] : _RGBWLeds)
    {
        if (led->init() != 0)
        {
            logger().log(LogService::LogLevel::ERROR, "EsploraBoard::init - {} failed", name);
            return -1;
        }
    }
    for (const auto& [name, adc] : _ADCs)
    {
        if (adc->init() != 0)
        {
            logger().log(LogService::LogLevel::ERROR, "EsploraBoard::init - {} failed", name);
            return -1;
        }
    }

    _hid        = make_unique<esplora::hid::Device>();
    const int n = _hid->countDevices();
    if (n <= 0)
    {
        logger().log(LogService::LogLevel::WARNING,
                     "EsploraBoard::init - no Esplora HID device (count={}); Ready without pump",
                     n);
    }
    else if (!_hid->openByIndex(esplora::hid::DefaultVendorId,
                                esplora::hid::DefaultProductId,
                                0))
    {
        logger().log(LogService::LogLevel::ERROR,
                     "EsploraBoard::init - openByIndex failed; Ready without pump");
        _hid->close();
    }
    else
    {
        logger().log(LogService::LogLevel::INFO, "EsploraBoard::init - HID open ({} device(s))", n);
        // Drive LED off through IRGBWLed (OUT + event-driven IN sync kick).
        if (const auto it = _RGBWLeds.find("rgbLed"); it != _RGBWLeds.end() && it->second)
        {
            (void)it->second->state(io::led::ILed::State::Off);
        }
        else
        {
            (void)_hid->writeRgb(0, 0, 0);
        }
        startHidPump();
    }

    _state = State::Ready;
    stateChanged(State::Ready);
    return 0;
}

FOUNDATION_FACTORY_REGISTER(driver::board::EsploraBoard,
                            "driver::board::EsploraBoard",
                            driver_board_EsploraBoard)
