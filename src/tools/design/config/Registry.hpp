/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Registry.hpp
 * @brief Application-wide config center (explicit install / current).
 */
#pragma once

#include "tools/design/config/IConfigCenter.hpp"

namespace tools::design::config
{

void install(ConfigCenterPtr center);

[[nodiscard]] ConfigCenterPtr current();

void reset();

} // namespace tools::design::config
