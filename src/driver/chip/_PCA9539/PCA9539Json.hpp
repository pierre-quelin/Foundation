/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PCA9539Json.hpp
 * @brief nlohmann JSON serializers for PCA9539 enums — include only where config is parsed.
 *
 * Must live in namespace driver::chip so ADL finds from_json for nested enums.
 */
#pragma once

#include "driver/chip/PCA9539.h"
#include "util/json/Json.hpp"

namespace driver::chip
{

JSON_SERIALIZE_ENUM(
    PCA9539::Pin,
    {{PCA9539::Pin::P00, "P00"},
     {PCA9539::Pin::P01, "P01"},
     {PCA9539::Pin::P02, "P02"},
     {PCA9539::Pin::P03, "P03"},
     {PCA9539::Pin::P04, "P04"},
     {PCA9539::Pin::P05, "P05"},
     {PCA9539::Pin::P06, "P06"},
     {PCA9539::Pin::P07, "P07"},
     {PCA9539::Pin::P10, "P10"},
     {PCA9539::Pin::P11, "P11"},
     {PCA9539::Pin::P12, "P12"},
     {PCA9539::Pin::P13, "P13"},
     {PCA9539::Pin::P14, "P14"},
     {PCA9539::Pin::P15, "P15"},
     {PCA9539::Pin::P16, "P16"},
     {PCA9539::Pin::P17, "P17"}})

JSON_SERIALIZE_ENUM(
    PCA9539::Polarity,
    {{PCA9539::Polarity::NORMAL, "NORMAL"},
     {PCA9539::Polarity::INVERTED, "INVERTED"}})

} // namespace driver::chip
