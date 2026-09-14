/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PCA9635Json.hpp
 * @brief nlohmann JSON serializers for PCA9635 enums — include only where config is parsed.
 *
 * Must live in namespace driver::chip so ADL finds from_json for nested enums.
 */

#pragma once

#include "driver/chip/PCA9635.h"
#include "util/json/Json.hpp"

namespace driver::chip
{

JSON_SERIALIZE_ENUM(
    PCA9635::Led,
    {{PCA9635::Led::LED0, "LED0"},
     {PCA9635::Led::LED1, "LED1"},
     {PCA9635::Led::LED2, "LED2"},
     {PCA9635::Led::LED3, "LED3"},
     {PCA9635::Led::LED4, "LED4"},
     {PCA9635::Led::LED5, "LED5"},
     {PCA9635::Led::LED6, "LED6"},
     {PCA9635::Led::LED7, "LED7"},
     {PCA9635::Led::LED8, "LED8"},
     {PCA9635::Led::LED9, "LED9"},
     {PCA9635::Led::LED10, "LED10"},
     {PCA9635::Led::LED11, "LED11"},
     {PCA9635::Led::LED12, "LED12"},
     {PCA9635::Led::LED13, "LED13"},
     {PCA9635::Led::LED14, "LED14"},
     {PCA9635::Led::LED15, "LED15"}})

} // namespace driver::chip
