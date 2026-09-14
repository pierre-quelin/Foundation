/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "EsploraRGBLed.h"

#include "tools/design/factory/Register.hpp"

#include <algorithm>
#include <cmath>

using namespace driver::board;
using namespace io::led;

namespace
{

[[nodiscard]] std::uint8_t levelToByte(float percent)
{
    const float clamped = std::clamp(percent, 0.f, 100.f);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.f / 100.f));
}

} // namespace

EsploraRGBLed::EsploraRGBLed(tools::design::ApplicationServices& /*app*/,
                             tools::design::config::Node /*node*/)
{
}

int EsploraRGBLed::init()
{
    _inited = true;
    _state  = State::Off;
    for (auto& [id, level] : _color)
    {
        level = 0.f;
    }
    // Apply off if the board already wired a live HID sink.
    emitRgb();
    return 0;
}

void EsploraRGBLed::setRgbSink(RgbSink sink)
{
    _rgbSink = std::move(sink);
}

void EsploraRGBLed::emitRgb()
{
    if (!_rgbSink)
    {
        return;
    }
    if (_state == State::Off)
    {
        _rgbSink(0, 0, 0);
        return;
    }
    _rgbSink(levelToByte(_color[ColorId::RED]),
             levelToByte(_color[ColorId::GREEN]),
             levelToByte(_color[ColorId::BLUE]));
}

ILed::State EsploraRGBLed::state() const
{
    return _state;
}

int EsploraRGBLed::state(const State& state)
{
    if (!_inited)
    {
        return -1;
    }
    _state = state;
    emitRgb();
    return 0;
}

ILed::BlinkCtrl EsploraRGBLed::blinkCtrl() const
{
    return _blinkCtrl;
}

int EsploraRGBLed::blinkCtrl(const BlinkCtrl& blinkCtrl)
{
    if (!_inited)
    {
        return -1;
    }
    _blinkCtrl = blinkCtrl;
    return 0;
}

IRGBWLed::Color EsploraRGBLed::brightness() const
{
    return _color;
}

int EsploraRGBLed::brightness(const Color& color)
{
    if (!_inited)
    {
        return -1;
    }
    for (const auto& [id, level] : color)
    {
        if (id == ColorId::WHITE)
        {
            _color[ColorId::WHITE] = 0.f;
            continue;
        }
        _color[id] = level;
    }
    emitRgb();
    return 0;
}

FOUNDATION_FACTORY_REGISTER(driver::board::EsploraRGBLed,
                            "driver::board::EsploraRGBLed",
                            driver_board_EsploraRGBLed)
