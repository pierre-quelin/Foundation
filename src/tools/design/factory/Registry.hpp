/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Registry.hpp
 * @brief Name → creator map for config-driven instantiation.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/factory/IObject.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace tools::design::factory
{

using ErasedObject = std::shared_ptr<void>;

/**
 * @brief Factory-owned object handle (shared ownership; typically exclusive at create sites).
 *
 * Uses @c shared_ptr so the concrete deleter stays inside the control block after type erasure —
 * required for correct destruction under libstdc++ / glibc (see Linux utest double-free).
 */
template <typename T>
using OwnedPtr = std::shared_ptr<T>;

/**
 * @brief Result of a factory create: ownership + optional @c ILaunchable view.
 *
 * @c asObject / @c launchable are non-owning and point at the same concrete as @c object.
 */
struct CreatedObject
{
    ErasedObject object;
    IObject* asObject       = nullptr;
    ILaunchable* launchable = nullptr;
};

/**
 * @brief Re-type an erased factory object via @c dynamic_cast from @c IObject.
 */
template <typename T>
[[nodiscard]] inline OwnedPtr<T> adoptAs(CreatedObject&& created)
{
    if (!created.object || created.asObject == nullptr)
    {
        return nullptr;
    }

    T* typed = dynamic_cast<T*>(created.asObject);
    if (typed == nullptr)
    {
        throw std::logic_error(std::string{"factory: adoptAs failed for type "} + typeid(T).name());
    }
    return OwnedPtr<T>(std::move(created.object), typed);
}

template <typename T>
[[nodiscard]] inline std::shared_ptr<T> shareAs(CreatedObject&& created)
{
    return adoptAs<T>(std::move(created));
}

template <typename T>
[[nodiscard]] inline std::shared_ptr<T> shareOwned(OwnedPtr<T> owned)
{
    return owned;
}

using Creator = std::function<CreatedObject(ApplicationServices&, config::Node&)>;

/**
 * @brief Singleton registry of factory creators keyed by InstanceOf name.
 */
class Registry
{
public:
    [[nodiscard]] static Registry& instance();

    void add(std::string name, Creator creator);

    /**
     * @brief Redirects an InstanceOf name to another registered name.
     *
     * Primary intended use: unit tests — keep production JSON (concrete InstanceOf)
     * and redirect to a mock in the test executable only. See README.md.
     *
     * @param alias  Name appearing in JSON InstanceOf (e.g. "io::in::InByPCA9539")
     * @param target Registered concrete or mock name (e.g. "io::in::test::InMock")
     */
    void addAlias(std::string alias, std::string target);

    /** @brief Drop all InstanceOf redirects (unit-test isolation). */
    void clearAliases();

    [[nodiscard]] Creator find(std::string_view name) const;

    [[nodiscard]] bool contains(std::string_view name) const;

private:
    Registry() = default;

    mutable std::mutex _mutex;
    std::unordered_map<std::string, Creator> _creators;
    std::unordered_map<std::string, std::string> _aliases;
};

} // namespace tools::design::factory
