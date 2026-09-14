/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file InstanceRegistry.hpp
 * @brief Named shared instances keyed by config::instancePath (Objects containers stripped).
 */
#pragma once

#include "tools/design/factory/IObject.hpp"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace tools::design::factory
{

/**
 * @brief Stores at most one shared object per config path key (@c IObject cast root).
 */
class InstanceRegistry
{
public:
    [[nodiscard]] bool contains(const std::string& key) const
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        return _instances.find(key) != _instances.end();
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> find(const std::string& key) const
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        const auto it = _instances.find(key);
        if (it == _instances.end())
        {
            return nullptr;
        }
        return std::dynamic_pointer_cast<T>(it->second);
    }

    /** @brief Register @p obj under @p key. Throws if the key is already taken. */
    template <typename T>
    void put(const std::string& key, std::shared_ptr<T> obj)
    {
        if (!obj)
        {
            throw std::logic_error("InstanceRegistry::put: null object at '" + key + "'");
        }
        IObject* root = dynamic_cast<IObject*>(obj.get());
        if (root == nullptr)
        {
            throw std::logic_error(
                "InstanceRegistry::put: type is not factory::IObject at '" + key + "'");
        }
        putObject(key, std::shared_ptr<IObject>(std::move(obj), root));
    }

    void putObject(const std::string& key, std::shared_ptr<IObject> obj)
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        const auto [it, inserted] = _instances.emplace(key, std::move(obj));
        if (!inserted)
        {
            throw std::runtime_error("InstanceRegistry: instance already registered at '" + key +
                                     "'");
        }
        (void)it;
    }

    /** @brief Remove @p key if present (no-op otherwise). */
    void erase(const std::string& key)
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        _instances.erase(key);
    }

    void clear()
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        _instances.clear();
    }

private:
    mutable std::mutex _mutex;
    std::unordered_map<std::string, std::shared_ptr<IObject>> _instances;
};

} // namespace tools::design::factory
