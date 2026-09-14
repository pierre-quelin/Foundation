/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Tree.hpp
 * @brief Item lists and factory wiring for the Objects + Item config model.
 */
#pragma once

#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/Obtain.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tools::design::factory
{

/** @brief Values of `Item : Name` entries (JSON array of `{ "Item": "..." }`). */
[[nodiscard]] inline std::vector<std::string> itemNames(config::Node node)
{
    std::vector<std::string> result;
    if (node.is_array())
    {
        result.reserve(node.size());
        for (std::size_t i = 0; i < node.size(); ++i)
        {
            const config::Node element = node[i];
            if (element.contains("Item"))
            {
                result.push_back(element["Item"].value<std::string>());
            }
        }
        return result;
    }
    if (node.is_object())
    {
        for (const auto& [key, child] : node.items())
        {
            if (key == "Item" && child.is_string())
            {
                result.push_back(child.value<std::string>());
            }
            else if (child.contains("Item"))
            {
                result.push_back(child["Item"].value<std::string>());
            }
        }
    }
    return result;
}

/** @brief Fill @p out with `createShared<T>(app, parent, name)` for each `Item` in @p listKey. */
template <typename T, typename Map>
void createSharedFromItemList(ApplicationServices& app,
                              const config::Node& parent,
                              std::string_view listKey,
                              Map& out)
{
    if (!parent.contains(listKey))
    {
        return;
    }
    for (const std::string& name : itemNames(parent[listKey]))
    {
        out.emplace(name, createShared<T>(app, parent, name));
    }
}

/** @brief Fill @p out with `obtain<T>(app, parent, name)` for each `Item` in @p listKey. */
template <typename T, typename Map>
void obtainFromItemList(ApplicationServices& app,
                        const config::Node& parent,
                        std::string_view listKey,
                        Map& out,
                        const bool createIfMissing = false)
{
    if (!parent.contains(listKey))
    {
        return;
    }
    for (const std::string& name : itemNames(parent[listKey]))
    {
        out.emplace(name, obtain<T>(app, parent, name, createIfMissing));
    }
}

/** @brief Fill @p out with `create<T>(app, parent, name)` for each `Item` in @p listKey. */
template <typename T, typename Map>
void createUniqueFromItemList(ApplicationServices& app,
                              const config::Node& parent,
                              std::string_view listKey,
                              Map& out)
{
    if (!parent.contains(listKey))
    {
        return;
    }
    for (const std::string& name : itemNames(parent[listKey]))
    {
        out.emplace(name, create<T>(app, parent, name));
    }
}

} // namespace tools::design::factory
