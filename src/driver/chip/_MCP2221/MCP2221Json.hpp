/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
  * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MCP2221Json.hpp
 * @brief nlohmann JSON serializers for MCP2221 enums — include only where config is parsed.
 *
 * Must live in namespace driver::chip so ADL finds from_json for nested enums.
 */
#pragma once

#include "driver/chip/MCP2221.h"
#include "util/json/Json.hpp"

namespace driver::chip
{

JSON_SERIALIZE_ENUM(
    MCP2221::GPPin,
    {{MCP2221::GPPin::GP0, "GP0"},
     {MCP2221::GPPin::GP1, "GP1"},
     {MCP2221::GPPin::GP2, "GP2"},
     {MCP2221::GPPin::GP3, "GP3"}})

} // namespace driver::chip
