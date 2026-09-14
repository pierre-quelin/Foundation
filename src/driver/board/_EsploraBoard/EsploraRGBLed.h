/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EsploraRGBLed.h
 * @brief RGB LED for Arduino Esplora (instance rgbLed). WHITE channel ignored / 0.
 *
 * Host OUT via optional sink (EsploraBoard → libesplora_hid writeRgb).
 */
#pragma once

#include "io/led/IRGBWLed.h"
#include "tools/design/factory/IObject.hpp"

#include <cstdint>
#include <functional>

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

class EsploraRGBLed : public tools::design::factory::IObject, public io::led::IRGBWLed
{
public:
    using RgbSink = std::function<void(std::uint8_t red, std::uint8_t green, std::uint8_t blue)>;

    ~EsploraRGBLed() override = default;
    EsploraRGBLed(tools::design::ApplicationServices& app,
                  tools::design::config::Node node);
    EsploraRGBLed(const EsploraRGBLed&)            = delete;
    EsploraRGBLed& operator=(const EsploraRGBLed&) = delete;
    EsploraRGBLed(EsploraRGBLed&&)                 = delete;
    EsploraRGBLed& operator=(EsploraRGBLed&&)      = delete;

    int init() override;

    [[nodiscard]] State state() const override;
    int state(const State& state) override;

    [[nodiscard]] BlinkCtrl blinkCtrl() const override;
    int blinkCtrl(const BlinkCtrl& blinkCtrl) override;

    [[nodiscard]] Color brightness() const override;
    int brightness(const Color& color) override;

    void setRgbSink(RgbSink sink);

private:
    void emitRgb();

    State _state{State::Off};
    BlinkCtrl _blinkCtrl{};
    Color _color{{ColorId::RED, 0.f},
                 {ColorId::GREEN, 0.f},
                 {ColorId::BLUE, 0.f},
                 {ColorId::WHITE, 0.f}};
    bool _inited{false};
    RgbSink _rgbSink;
};

} // namespace driver::board
