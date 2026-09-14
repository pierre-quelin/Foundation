/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IObject.hpp
 * @brief Polymorphic root for all factory-instantiated concrete types.
 */
#pragma once

namespace tools::design::factory
{

/**
 * @brief Cast anchor for type-erased factory ownership (@c CreatedObject / @c adoptAs).
 *
 * Every type registered with @c FOUNDATION_FACTORY_REGISTER must derive from @c IObject
 * (directly or via @c objkit::ObjKit). @c adoptAs uses @c dynamic_cast from this root.
 */
class IObject
{
public:
    virtual ~IObject() = default;
};

} // namespace tools::design::factory
