/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IIn.h
 * @brief Input IO Interface
 */
#pragma once

#include "tools/design/signal/Signal.hpp"

namespace io::in
{

class IIn
{
public:
    virtual ~IIn() = default;

    /**
     * @brief Initializes the specified input
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int init() = 0;

    /**
     * @brief Get the input value.
     *
     * @param value The input value.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int get(unsigned int& value) const = 0;

    /**
     * @brief In Observers
     * @note For RAII pattern see boost::signals2::scoped_connection
     */
    tools::design::signal::Signal<void(unsigned int /* value */)> valueChanged;
};

} // namespace io::in
