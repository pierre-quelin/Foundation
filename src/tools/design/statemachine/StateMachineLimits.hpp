/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file StateMachineLimits.hpp
 * @brief Backend-specific limits for @c statemachine-cpp (Boost.MSM).
 */
#pragma once

#include <cstddef>

namespace tools::design::statemachine::smd
{

/**
 * @brief Max MSM transition rows per machine.
 *
 * Must match @c BOOST_MPL_LIMIT_VECTOR_SIZE (see @c FoundationBoostMsm and generated @c *_sm.h).
 * Above 20 rows, @c BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS is required (plain MPL headers cap at 20).
 */
inline constexpr std::size_t MsmMaxTransitionRows = 30U;

} // namespace tools::design::statemachine::smd
