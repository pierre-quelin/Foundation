/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IADC.h
 * @brief Analog to Digital Converter Interface
 */
#pragma once

#include "tools/design/signal/Signal.hpp"

namespace io::adc
{

// units : boost (old) vs mp-units(new https://github.com/mpusz/units)
// cf. https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1935r0.html
// <boost/units/systems/si/electric_potential.hpp>
// template <class Unit>

// TODO - Rename AnalogIn
// Ref, Unit, Range,... ?

class IADC
{
public:
    virtual ~IADC() = default;

    /**
     * @brief Initializes the converter
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int init() = 0;

    /**
     * @brief Read the digital value of the ADC
     *
     * @param value The value
     * @return 0 if successful, -1 otherwise.
     */
    virtual int get(double& value) const = 0;

    /**
     * @brief ADC Observers
     * @note For RAII pattern see boost::signals2::scoped_connection
     */
    tools::design::signal::Signal<void(double /* value */)> valueChanged;
};

} // namespace io::adc
