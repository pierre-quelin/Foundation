/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Config.hpp
 * @brief Configuration center factories.
 */
#pragma once

#include "tools/design/config/IConfigCenter.hpp"
#include "tools/design/config/Node.hpp"

#include <filesystem>
#include <string_view>

namespace tools::design::config
{

[[nodiscard]] ConfigCenterPtr createLocal(const std::filesystem::path& path);
[[nodiscard]] ConfigCenterPtr createLocalFromString(std::string_view json);

/** @brief Alias for createLocal. */
[[nodiscard]] inline ConfigCenterPtr load(const std::filesystem::path& path)
{
    return createLocal(path);
}

/** @brief Alias for createLocalFromString. */
[[nodiscard]] inline ConfigCenterPtr parse(std::string_view json)
{
    return createLocalFromString(json);
}

} // namespace tools::design::config
