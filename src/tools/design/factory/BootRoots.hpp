/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file BootRoots.hpp
 * @brief Boot-time creation / launch / destruction of explicit GlobalObjects roots.
 */
#pragma once

#include "tools/design/config/Reference.hpp"
#include "tools/design/config/ResolvePlatform.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Factory.hpp"
#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Tree.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace tools::design::factory
{

/**
 * @brief One GlobalObjects root: ownership + optional @c ILaunchable view.
 */
struct GlobalRoot
{
    std::shared_ptr<IObject> object;
    ILaunchable* launchable = nullptr;
};

/**
 * @brief Instantiate @p node, register it, and return type-erased ownership.
 */
[[nodiscard]] inline GlobalRoot createSharedErased(ApplicationServices& app, config::Node node)
{
    ensureInstances(app);
    const std::string key = config::instancePath(node.path());
    if (app.instances->contains(key))
    {
        throw std::runtime_error("factory: instance already registered at '" + key + "'");
    }

    CreatedObject created = createFromNode(app, node);
    auto obj =
        std::shared_ptr<IObject>(std::move(created.object), created.asObject);
    app.instances->putObject(key, obj);
    maybeAttachBridge(app, node);
    return GlobalRoot{std::move(obj), created.launchable};
}

/**
 * @brief Create all roots listed in @c root["GlobalObjects"] (explicit Item list only).
 *
 * Each Item names a top-level config object with @c InstanceOf. Failure on any root
 * aborts the boot (no partial silent start). Ownership is returned so the caller can
 * keep roots alive until shutdown (destroy in reverse creation order).
 *
 * @throws std::runtime_error if @c GlobalObjects is missing/empty or an Item cannot be built.
 */
[[nodiscard]] inline std::vector<GlobalRoot> createGlobalObjects(ApplicationServices& app)
{
    const config::Node root = app.root();
    if (!root.contains("GlobalObjects"))
    {
        throw std::runtime_error("factory::createGlobalObjects: missing 'GlobalObjects' at root");
    }

    config::PlatformResolvedNode resolvedGo(root["GlobalObjects"], app.platformName);
    const config::Node go = resolvedGo.get();

    const std::vector<std::string> names = itemNames(go);
    if (names.empty())
    {
        throw std::runtime_error("factory::createGlobalObjects: 'GlobalObjects' is empty");
    }

    std::vector<GlobalRoot> roots;
    roots.reserve(names.size());
    for (const std::string& name : names)
    {
        config::Node node = config::resolveNamed(root, root, name);
        try
        {
            roots.push_back(createSharedErased(app, std::move(node)));
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error("factory::createGlobalObjects: failed to create '" + name +
                                     "': " + ex.what());
        }
    }
    return roots;
}

/**
 * @brief Call @c ILaunchable::launch() on roots that implement it (creation order).
 *
 * Non-launchable roots are skipped. First throw aborts the boot.
 */
inline void launchGlobalObjects(std::vector<GlobalRoot>& roots)
{
    for (GlobalRoot& root : roots)
    {
        if (root.launchable != nullptr)
        {
            root.launchable->launch();
        }
    }
}

/**
 * @brief Destroy roots in reverse creation order; erase matching InstanceRegistry entries.
 *
 * Caller must release all other @c shared_ptr / @c obtain refs to these roots first.
 * Component @c drainScheduler stays in ObjKit destructors.
 *
 * @throws std::runtime_error if @c GlobalObjects is missing or size mismatches @p roots.
 */
inline void destroyGlobalObjects(ApplicationServices& app, std::vector<GlobalRoot>& roots)
{
    ensureInstances(app);
    const config::Node root = app.root();
    if (!root.contains("GlobalObjects"))
    {
        throw std::runtime_error("factory::destroyGlobalObjects: missing 'GlobalObjects' at root");
    }

    config::PlatformResolvedNode resolvedGo(root["GlobalObjects"], app.platformName);
    const std::vector<std::string> names = itemNames(resolvedGo.get());
    if (roots.size() != names.size())
    {
        throw std::runtime_error(
            "factory::destroyGlobalObjects: roots size does not match 'GlobalObjects'");
    }

    for (std::size_t i = roots.size(); i-- > 0;)
    {
        config::Node node     = config::resolveNamed(root, root, names[i]);
        const std::string key = config::instancePath(node.path());
        app.instances->erase(derivedInstanceKey(key, "bridge"));
        app.instances->erase(key);
        roots[i].launchable = nullptr;
        roots[i].object.reset();
    }
    roots.clear();
}

} // namespace tools::design::factory
