/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PCA9633Json.hpp
 * @brief nlohmann JSON serializers for PCA9633 enums — include only where config is parsed.
 *
 * Must live in namespace driver::chip so ADL finds from_json for nested enums.
 */
#pragma once

#include "driver/chip/PCA9633.h"
#include "util/json/Json.hpp"

namespace driver::chip
{

JSON_SERIALIZE_ENUM(
    PCA9633::Led,
    {{PCA9633::Led::LED0, "LED0"},
     {PCA9633::Led::LED1, "LED1"},
     {PCA9633::Led::LED2, "LED2"},
     {PCA9633::Led::LED3, "LED3"}})

} // namespace driver::chip
