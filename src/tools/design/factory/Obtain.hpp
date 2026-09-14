/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Obtain.hpp
 * @brief createShared (instantiate + register) and obtain (lookup, optional create).
 */
#pragma once

#include "tools/design/config/Reference.hpp"
#include "tools/design/config/ResolvePlatform.hpp"
#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/InstanceRegistry.hpp"
#include "tools/os/sync/SemM.hpp"
#include "util/json/Json.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace tools::design::factory
{

inline void ensureInstances(ApplicationServices& app)
{
    if (app.instances == nullptr)
    {
        app.instances = std::make_shared<InstanceRegistry>();
    }
}

[[nodiscard]] inline std::string bridgedTypeName(config::Node node)
{
    if (!node.contains("Bridged") || !node["Bridged"].is_string())
    {
        return {};
    }
    return node["Bridged"].value<std::string>();
}

[[nodiscard]] inline std::string derivedInstanceKey(std::string_view base, std::string_view suffix)
{
    std::string key{base};
    key.push_back('#');
    key.append(suffix);
    return key;
}

inline void maybeAttachBridge(ApplicationServices& app, config::Node node)
{
    config::PlatformResolvedNode resolved(node, app.platformName);
    const config::Node effective = resolved.get();
    const std::string typeName   = bridgedTypeName(effective);
    if (typeName.empty())
    {
        return;
    }
    if (app.eventBus == nullptr)
    {
        throw std::logic_error("factory: Bridged object requires ApplicationServices::eventBus at " +
                               effective.path());
    }
    CreatedObject created = createUntyped(app, effective, typeName);
    if (created.launchable == nullptr)
    {
        throw std::logic_error("factory: Bridged type '" + typeName + "' is not launchable at " +
                               effective.path());
    }
    const std::string bridgeKey = derivedInstanceKey(config::instancePath(effective.path()), "bridge");
    if (app.instances->contains(bridgeKey))
    {
        throw std::runtime_error("factory: instance already registered at '" + bridgeKey + "'");
    }
    app.instances->putObject(bridgeKey,
                             std::shared_ptr<IObject>(std::move(created.object), created.asObject));
    created.launchable->launch();
}

/**
 * @brief Instantiate @p node and register it under @c config::instancePath for later @c obtain.
 *
 * Throws if an instance is already registered at that path.
 */
template <typename T>
[[nodiscard]] std::shared_ptr<T> createShared(ApplicationServices& app, config::Node node)
{
    ensureInstances(app);
    const std::string key = config::instancePath(node.path());
    if (app.instances->contains(key))
    {
        throw std::runtime_error("factory: instance already registered at '" + key + "'");
    }
    CreatedObject created = createFromNode(app, node);
    auto obj              = shareAs<T>(std::move(created));
    app.instances->put(key, obj);
    maybeAttachBridge(app, node);
    return obj;
}

template <typename T>
[[nodiscard]] std::shared_ptr<T> createShared(ApplicationServices& app,
                                              const config::Node& node,
                                              std::string_view name)
{
    return createShared<T>(app, config::resolveNamed(app.root(), node, name));
}

/**
 * @brief Instantiate under @p parent as @p name without an Objects entry in the config document.
 *
 * Builds an ephemeral definition node at @c parent/Objects/name (so @c instancePath is
 * @c parent.name), calls the factory constructor @c T(app, node), and registers the instance.
 * @p props are the fields the constructor reads (Address, I2CMaster, …); they are not persisted.
 */
template <typename T>
[[nodiscard]] std::shared_ptr<T> createSharedNamed(ApplicationServices& app,
                                                   const config::Node& parent,
                                                   std::string_view name,
                                                   util::json::Json props = {})
{
    ensureInstances(app);

    std::string ephemeralPath = parent.path();
    if (!ephemeralPath.empty())
    {
        ephemeralPath.push_back('/');
    }
    ephemeralPath += "Objects/";
    ephemeralPath.append(name);

    const std::string key = config::instancePath(ephemeralPath);
    if (app.instances->contains(key))
    {
        throw std::runtime_error("factory: instance already registered at '" + key + "'");
    }

    tools::os::sync::SemM mutex;
    config::Node ephemeral(&props, std::move(ephemeralPath), &mutex);

    std::shared_ptr<T> obj;
    if (props.contains("InstanceOf"))
    {
        obj = shareOwned(create<T>(app, ephemeral));
    }
    else
    {
        obj = std::make_shared<T>(app, ephemeral);
    }
    app.instances->put(key, obj);
    maybeAttachBridge(app, ephemeral);
    return obj;
}

/**
 * @brief New unique instance from @p name (field key or Objects entry) — not registered.
 */
template <typename T>
[[nodiscard]] OwnedPtr<T> create(ApplicationServices& app,
                                 const config::Node& node,
                                 std::string_view name)
{
    return create<T>(app, config::resolveNamed(app.root(), node, name));
}

namespace detail
{

[[nodiscard]] inline std::string joinInstance(std::string_view base, std::string_view name)
{
    std::string key{base};
    if (!key.empty())
    {
        key.push_back('.');
    }
    key.append(name);
    return key;
}

template <typename T>
[[nodiscard]] std::shared_ptr<T> obtainFromRegistry(ApplicationServices& app,
                                                    const config::Node& node,
                                                    std::string_view name)
{
    std::string key;
    if (node.contains(name))
    {
        const config::Node field = node[name];
        if (!field.is_string())
        {
            throw std::runtime_error("factory: '" + std::string(name) + "' is not a string ref at " +
                                     node.path());
        }
        const std::string ref = field.value<std::string>();
        if (config::isReference(ref))
        {
            key = config::instancePath(config::detail::referenceToPath(ref));
        }
        else
        {
            // Short ref on a definition node → relative to the owning composite
            // (e.g. Board/Objects/PCA9539 + "MCP2221" → Board.MCP2221).
            key = joinInstance(config::instancePath(config::detail::parentPath(node.path())), ref);
        }
    }
    else
    {
        // Bare name under the current node (e.g. obtain(app, board, "MCP2221")).
        key = joinInstance(config::instancePath(node.path()), name);
    }

    if (auto existing = app.instances->find<T>(key))
    {
        return existing;
    }
    throw std::runtime_error("factory: no instance registered at '" + key + "'");
}

} // namespace detail

/**
 * @brief Shared instance for @p name (field key, Objects entry, or dotted reference).
 *
 * By default looks up an instance previously registered by @c createShared / @c createSharedNamed.
 * If config resolution fails (e.g. instance created via @c createSharedNamed with no Objects entry),
 * falls back to the
 * instance registry using the logical path (@c IOBoard.PCA9633_0 → @c IOBoard.PCA9633_0, or a
 * short name relative to the owner of @p node).
 *
 * If @p createIfMissing is true and a config node resolves, falls back to @c createShared.
 */
template <typename T>
[[nodiscard]] std::shared_ptr<T> obtain(ApplicationServices& app,
                                        const config::Node& node,
                                        std::string_view name,
                                        const bool createIfMissing = false)
{
    ensureInstances(app);

    config::Node resolved;
    bool resolvedOk = false;
    try
    {
        resolved   = config::resolveNamed(app.root(), node, name);
        resolvedOk = true;
    }
    catch (const std::runtime_error&)
    {
        return detail::obtainFromRegistry<T>(app, node, name);
    }

    const std::string key = config::instancePath(resolved.path());
    if (auto existing = app.instances->find<T>(key))
    {
        return existing;
    }
    if (!createIfMissing)
    {
        // Config node exists but nothing registered — still try registry-only keys
        // (createSharedNamed registers under parent.name without a document Objects entry).
        try
        {
            return detail::obtainFromRegistry<T>(app, node, name);
        }
        catch (const std::runtime_error&)
        {
            throw std::runtime_error("factory: no instance registered at '" + key + "'");
        }
    }
    (void)resolvedOk;
    return createShared<T>(app, std::move(resolved));
}

} // namespace tools::design::factory
