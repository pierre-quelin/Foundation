/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Json.hpp
 * @brief nlohmann::json facade
 */
#pragma once

#include <nlohmann/json.hpp>

namespace util
{
namespace json
{
using Json = nlohmann::json;

#define JSON_SERIALIZE_ENUM(ENUM_TYPE, ...) NLOHMANN_JSON_SERIALIZE_ENUM(ENUM_TYPE, __VA_ARGS__)
} // namespace json
} // namespace util
