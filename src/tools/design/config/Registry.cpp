/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 */

#include "tools/design/config/Registry.hpp"

#include <stdexcept>

namespace tools::design::config
{

namespace
{

ConfigCenterPtr& storage()
{
    static ConfigCenterPtr center;
    return center;
}

} // namespace

void install(ConfigCenterPtr center)
{
    if (center == nullptr)
    {
        throw std::invalid_argument("config::install: null center");
    }
    storage() = std::move(center);
}

ConfigCenterPtr current()
{
    const ConfigCenterPtr& center = storage();
    if (center == nullptr)
    {
        throw std::logic_error("config::current: install() must be called first");
    }
    return center;
}

void reset()
{
    storage().reset();
}

} // namespace tools::design::config
