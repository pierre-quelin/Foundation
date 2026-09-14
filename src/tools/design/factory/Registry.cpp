/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
*/

#include "tools/design/factory/Registry.hpp"

#include <stdexcept>

namespace tools::design::factory
{

Registry& Registry::instance()
{
    static Registry registry;
    return registry;
}

void Registry::add(std::string name, Creator creator)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _creators.emplace(std::move(name), std::move(creator));
}

void Registry::addAlias(std::string alias, std::string target)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _aliases.emplace(std::move(alias), std::move(target));
}

void Registry::clearAliases()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _aliases.clear();
}

Creator Registry::find(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::string key{name};
    for (int depth = 0; depth < 8; ++depth)
    {
        const auto aliasIt = _aliases.find(key);
        if (aliasIt != _aliases.end())
        {
            key = aliasIt->second;
            continue;
        }

        const auto creatorIt = _creators.find(key);
        if (creatorIt != _creators.end())
        {
            return creatorIt->second;
        }

        break;
    }

    throw std::runtime_error(std::string{"factory: unknown type '"} + std::string{name} + "'");
}

bool Registry::contains(std::string_view name) const
{
    try
    {
        (void)find(name);
        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

} // namespace tools::design::factory
