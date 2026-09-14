/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "io/led/RGBWLedByPCA9633.h"

#include "driver/chip/PCA9633Json.hpp"
#include "io/led/RGBWLedJson.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"

#include <chrono>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

using namespace driver::chip;
using namespace io;
using namespace io::led;
using namespace std;
using namespace std::chrono_literals;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

namespace
{

map<IRGBWLed::ColorId, PCA9633::Led> readRgbwMapping(const Node& node)
{
    map<IRGBWLed::ColorId, PCA9633::Led> mapping;
    const Node mappingNode = node.at("Mapping");
    for (std::size_t i = 0; i < mappingNode.size(); ++i)
    {
        const Node color                                     = mappingNode[i];
        mapping[color.at("ColorId").as<IRGBWLed::ColorId>()] = color.at("Led").as<PCA9633::Led>();
    }
    return mapping;
}

} // namespace

RGBWLedByPCA9633::RGBWLedByPCA9633(const shared_ptr<PCA9633> device,
                                   const map<IRGBWLed::ColorId, PCA9633::Led>& mapping) :
    IRGBWLed(),
    _device(device),
    _mapping(mapping),
    _state(ILed::State::Off)
{
    // Default color.
    // Used for blinking and Off -> On transition
    for (const auto& [id, led] : _mapping)
    {
        _color[id] = 100;
    }
}

RGBWLedByPCA9633::RGBWLedByPCA9633(ApplicationServices& app, Node node) :
    RGBWLedByPCA9633(obtain<PCA9633>(app, node, "PCA9633"), readRgbwMapping(node))
{
}

RGBWLedByPCA9633::~RGBWLedByPCA9633()
{
    state(State::Off);
}

int RGBWLedByPCA9633::init()
{
    // Restore previous state
    state(_state);
    brightness(_color);
    blinkCtrl(_blinkCtrl);

    return 0;
}

ILed::State RGBWLedByPCA9633::state() const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    return _state;
}

int RGBWLedByPCA9633::state(const State& state)
{
    const std::unique_lock<std::shared_mutex> lock(_mutex);
    // Change
    _state = state;

    // Apply
    std::vector<std::pair<PCA9633::Led, PCA9633::LedDriver>> drivers;
    switch (state)
    {
        case ILed::State::Off:
            for (const auto& [id, led] : _mapping)
            {
                drivers.emplace_back(std::make_pair(_mapping[id], PCA9633::LedDriver::LDRx_OFF));
            }
            break;

        case ILed::State::On:
            // Stored by the PCA9633 (brightness(_color))
            for (const auto& [id, led] : _mapping)
            {
                // Not necessary cf. Constructor
                // if (_color.find(id) != _mapping.end())
                {
                    if (_color[id] <= 0.0f)
                    {
                        drivers.emplace_back(std::make_pair(_mapping[id], PCA9633::LedDriver::LDRx_OFF));
                    }
                    else if (_color[id] >= 100.0f)
                    {
                        drivers.emplace_back(std::make_pair(_mapping[id], PCA9633::LedDriver::LDRx_ON));
                    }
                    else
                    {
                        drivers.emplace_back(std::make_pair(_mapping[id], PCA9633::LedDriver::LDRx_BRI));
                    }
                }
            }
            break;

        case ILed::State::Blinking:
            // Stored by the PCA9633 (brightness(_color))
            for (const auto& [id, led] : _mapping)
            {
                if (_color[id] <= 0.0f)
                {
                    drivers.emplace_back(std::make_pair(_mapping[id], PCA9633::LedDriver::LDRx_OFF));
                }
                else
                {
                    drivers.emplace_back(std::make_pair(_mapping[id], PCA9633::LedDriver::LDRx_BRI_BLK));
                }
            }
            break;
    }
    if (-1 == _device->setLedDrvOut(drivers))
    {
        return -1;
    }

    return 0;
}

ILed::BlinkCtrl RGBWLedByPCA9633::blinkCtrl() const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);
    return _blinkCtrl;
}

/**
 * @brief Set the blink control timing.
 * Attention, common for the 4 leds
 *
 * @param blinkCtrl The blink control timing.
 *      Min period value : 1s/24
 *      Max period value : 256s/24
 * @return 0 if successful, -1 otherwise.
 */
int RGBWLedByPCA9633::blinkCtrl(const BlinkCtrl& blinkCtrl)
{
    // Check min/max values
    std::chrono::milliseconds min = 1000ms / 24;
    std::chrono::milliseconds max = 256000ms / 24;
    if ((blinkCtrl.period < min) ||
        (blinkCtrl.period > max))
    {
        return -1;
    }

    const std::unique_lock<std::shared_mutex> lock(_mutex);

    // Change
    _blinkCtrl = blinkCtrl;

    // Apply
    return _device->setBlinkingGrpCtrl(static_cast<uint8_t>(blinkCtrl.percent * 255 / 100),
                                       static_cast<uint8_t>((blinkCtrl.period.count() * 25) / 1000 - 1));
}

IRGBWLed::Color RGBWLedByPCA9633::brightness() const
{
    const std::shared_lock<std::shared_mutex> lock(_mutex);

    return _color;
}

int RGBWLedByPCA9633::brightness(const Color& color)
{
    {
        const std::unique_lock<std::shared_mutex> lock(_mutex);
        // Change
        for (const auto& [id, intensity] : color)
        {
            // If the ColorId is available
            if (_mapping.find(id) != _mapping.end())
            {
                _color[id] = intensity; // Merges with the previous value
            }
        }

        // Apply
        for (const auto& [id, intensity] : _color)
        {
            if (-1 == _device->setBrightness(_mapping[id], static_cast<uint8_t>(intensity * 255 / 100)))
            {
                return -1;
            }
        }
    }
    // Apply immediately.
    // Needed if _state==ILed::State::On and a driver used is LDRx_ON.
    if (-1 == state(_state))
    {
        return -1;
    }

    return 0;
}

FOUNDATION_FACTORY_REGISTER(io::led::RGBWLedByPCA9633,
                            "io::led::RGBWLedByPCA9633",
                            io_led_RGBWLedByPCA9633)
