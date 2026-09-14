/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ResolvePlatform.hpp
 * @brief Optional Platform.&lt;platformName&gt; merge for shared multi-platform configs.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/os/sync/SemM.hpp"
#include "util/json/Json.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace tools::design::config
{

/**
 * @brief If @p node has @c Platform, return merged JSON for @p platformName; else nullopt.
 *
 * - No @c Platform key → nullopt (caller keeps the original node).
 * - Branch is a non-object (e.g. array for GlobalObjects) → that value replaces the node.
 * - Branch is an object → parent fields minus @c Platform, then shallow-overlaid by the branch.
 */
[[nodiscard]] inline std::optional<util::json::Json> resolvePlatformJson(
    const Node& node, std::string_view platformName)
{
    if (!node.contains("Platform"))
    {
        return std::nullopt;
    }
    if (platformName.empty())
    {
        throw std::runtime_error("config: Platform present but platformName is empty at " +
                                 node.path());
    }
    const Node platforms = node["Platform"];
    if (!platforms.contains(platformName))
    {
        throw std::runtime_error("config: Platform['" + std::string(platformName) +
                                 "'] missing at " + node.path());
    }
    const Node branch                 = platforms[platformName];
    const util::json::Json branchJson = branch.toJson();
    if (!branchJson.is_object())
    {
        return branchJson;
    }

    util::json::Json merged = node.toJson();
    if (!merged.is_object())
    {
        throw std::runtime_error("config: Platform merge expects an object parent at " +
                                 node.path());
    }
    merged.erase("Platform");
    for (auto it = branchJson.begin(); it != branchJson.end(); ++it)
    {
        merged[it.key()] = *it;
    }
    return merged;
}

/**
 * @brief Holds an optional Platform-resolved ephemeral @c Node for the duration of a call.
 */
class PlatformResolvedNode
{
public:
    PlatformResolvedNode(const Node& node, std::string_view platformName) : _original(node)
    {
        auto merged = resolvePlatformJson(node, platformName);
        if (merged)
        {
            _storage      = std::move(*merged);
            _ephemeral    = Node(&_storage, node.path(), &_mutex);
            _useEphemeral = true;
        }
    }

    [[nodiscard]] Node get() const
    {
        return _useEphemeral ? _ephemeral : _original;
    }

private:
    Node _original;
    util::json::Json _storage;
    tools::os::sync::SemM _mutex;
    Node _ephemeral;
    bool _useEphemeral = false;
};

} // namespace tools::design::config
