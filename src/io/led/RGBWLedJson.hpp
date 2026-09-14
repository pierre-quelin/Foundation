/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file RGBWLedJson.hpp
 * @brief nlohmann JSON serializers for RGBWLed enums — include only where config is parsed.
 */
#pragma once

#include "io/led/IRGBWLed.h"
#include "util/json/Json.hpp"

namespace io::led
{

JSON_SERIALIZE_ENUM(
    IRGBWLed::ColorId,
    {{IRGBWLed::ColorId::RED, "RED"},
     {IRGBWLed::ColorId::GREEN, "GREEN"},
     {IRGBWLed::ColorId::BLUE, "BLUE"},
     {IRGBWLed::ColorId::WHITE, "WHITE"}})

} // namespace io::led
