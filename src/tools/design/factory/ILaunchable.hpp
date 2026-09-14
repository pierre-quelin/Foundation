/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ILaunchable.hpp
 * @brief Optional post-construction start hook for boot roots.
 */
#pragma once

namespace tools::design::factory
{

/**
 * @brief Objects that start async work after the full root graph is constructed.
 */
class ILaunchable
{
public:
    virtual ~ILaunchable() = default;

    /** @brief Start runtime work (threads, cycles, …). May throw → boot fails. */
    virtual void launch() = 0;
};

} // namespace tools::design::factory
