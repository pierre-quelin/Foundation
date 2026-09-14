/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file StateMachineBase.hpp
 * @brief Polymorphic state machine interface (Foundation Phase 6).
 */
#pragma once

#include <cstdint>

namespace tools::design::statemachine
{

/** @brief Minimal runtime view of a state machine instance. */
class StateMachineBase
{
public:
    virtual ~StateMachineBase() = default;

    [[nodiscard]] virtual std::uint32_t currentStateId() const = 0;

    virtual void start() = 0;
    virtual void stop()  = 0;
};

} // namespace tools::design::statemachine
