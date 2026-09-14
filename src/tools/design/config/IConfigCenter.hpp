/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IConfigCenter.hpp
 * @brief Configuration service interface (local or remote).
 */
#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace tools::design::config
{

class Node;

/**
 * @brief Application configuration service.
 *
 * Phase 1: LocalConfigCenter (JSON file in memory).
 * Future: RemoteConfigCenter (proxy to a distant config service).
 */
class IConfigCenter
{
public:
    virtual ~IConfigCenter() = default;

    [[nodiscard]] virtual Node root()       = 0;
    [[nodiscard]] virtual Node root() const = 0;

    /** @brief Serializes the current tree (JSON). */
    [[nodiscard]] virtual std::string toJsonString() const = 0;

    /** @brief Persists the current tree. Meaning depends on implementation. */
    virtual void save(const std::filesystem::path& path) const = 0;
};

using ConfigCenterPtr = std::shared_ptr<IConfigCenter>;

} // namespace tools::design::config
