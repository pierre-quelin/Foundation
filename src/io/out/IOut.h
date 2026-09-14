/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IOut.h
 * @brief Output IO Interface
 */
#pragma once

namespace io::out
{

class IOut
{
public:
    virtual ~IOut() = default;

    /**
     * @brief Initializes the output
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int init() = 0;

    /**
     * @brief Set the output value.
     *
     * @param value The output value.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int set(unsigned int value) = 0;
    /**
     * @brief Get the output value.
     *
     * @param value The output value.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int get(unsigned int& value) const = 0;
};

} // namespace io::out
