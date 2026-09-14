/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EsploraBoard.h
 * @brief IBoard for Arduino Esplora (fixed PCB IO; HID pump via libesplora_hid).
 */
#pragma once

#include "io/adc/IADC.h"
#include "io/board/IBoard.h"
#include "io/in/IIn.h"
#include "io/led/ILed.h"
#include "io/led/IRGBWLed.h"
#include "io/out/IOut.h"
#include "io/pwm/IPWM.h"
#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/objkit/ObjKit.hpp"
#include "tools/os/thread/Thread.h"

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace esplora::hid
{
class Device;
}

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace driver::board
{

class EsploraBoard : public io::board::IBoard,
                     public tools::design::objkit::ObjKit,
                     public tools::design::factory::ILaunchable
{
public:
    ~EsploraBoard() override;
    EsploraBoard(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    EsploraBoard(const EsploraBoard&)            = delete;
    EsploraBoard& operator=(const EsploraBoard&) = delete;
    EsploraBoard(EsploraBoard&&)                 = delete;
    EsploraBoard& operator=(EsploraBoard&&)      = delete;

    void launch() override;

    State state() override;

    [[nodiscard]] std::shared_ptr<io::in::IIn> getInput(const std::string& name) const override;
    [[nodiscard]] std::shared_ptr<io::out::IOut> getOutput(const std::string& name) const override;
    [[nodiscard]] std::shared_ptr<io::led::ILed> getLed(const std::string& name) const override;
    [[nodiscard]] std::shared_ptr<io::led::IRGBWLed> getRGBWLed(const std::string& name) const override;
    [[nodiscard]] std::shared_ptr<io::pwm::IPWM> getPWM(const std::string& name) const override;
    [[nodiscard]] std::shared_ptr<io::adc::IADC> getADC(const std::string& name) const override;

private:
    int init() override;
    void startHidPump();
    void stopHidPump();
    void hidPumpLoop();
    void wireRgbSink();
    void clearRgbSink();

    std::unordered_map<std::string, std::shared_ptr<io::in::IIn>> _inputs;
    std::unordered_map<std::string, std::shared_ptr<io::led::IRGBWLed>> _RGBWLeds;
    std::unordered_map<std::string, std::shared_ptr<io::adc::IADC>> _ADCs;

    std::unique_ptr<esplora::hid::Device> _hid;
    std::unique_ptr<tools::os::thread::Thread> _hidPump;
    std::atomic<bool> _hidPumpRunning{false};

    State _state{State::NotReady};
};

} // namespace driver::board
