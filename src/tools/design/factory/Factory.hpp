/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Factory.hpp
 * @brief Create objects from ApplicationServices and config::Node.
 */
#pragma once

#include "tools/design/config/ResolvePlatform.hpp"
#include "tools/design/factory/Registry.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace tools::design::factory
{

[[nodiscard]] inline std::string instanceOfName(config::Node node)
{
    if (!node.contains("InstanceOf"))
    {
        throw std::runtime_error("factory: missing InstanceOf at " + node.path());
    }
    return node["InstanceOf"].value<std::string>();
}

[[nodiscard]] inline CreatedObject createUntyped(ApplicationServices& app,
                                                 config::Node node,
                                                 std::string_view name)
{
    const Creator creator = Registry::instance().find(name);
    return creator(app, node);
}

[[nodiscard]] inline CreatedObject createFromNode(ApplicationServices& app, config::Node node)
{
    config::PlatformResolvedNode resolved(node, app.platformName);
    const config::Node effective = resolved.get();
    const std::string name       = instanceOfName(effective);
    return createUntyped(app, effective, name);
}

template <typename T>
[[nodiscard]] OwnedPtr<T> create(ApplicationServices& app, config::Node node)
{
    return adoptAs<T>(createFromNode(app, std::move(node)));
}

template <typename T>
[[nodiscard]] OwnedPtr<T> createAs(ApplicationServices& app,
                                   config::Node node,
                                   std::string_view name)
{
    return adoptAs<T>(createUntyped(app, std::move(node), name));
}

} // namespace tools::design::factory
