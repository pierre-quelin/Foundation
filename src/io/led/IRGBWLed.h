/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IRGBWLed.h
 * @brief Interface for Red, Green, Blue, White color mixing led
 */
#pragma once

#include "io/led/ILed.h"

#include <map>

namespace io::led
{

class IRGBWLed : public ILed
{
public:
    virtual ~IRGBWLed() override = default;

    enum class ColorId
    {
        RED,
        GREEN,
        BLUE,
        WHITE
    };

    using Color = std::map<ColorId, float>; /**< Light intensity in percentage per color [0;100] 0 : Off, 100 : On */
    /**
     * @brief Return the current color with used colorIds
     *
     * @return The color
     */
    [[nodiscard]] virtual Color brightness() const = 0;
    /**
     * @brief Set brightness of the specified colors.
     * If only one color is specified, the others are not cleared.
     *
     * @param color  Light intensity in % per color [0;100]. No consistency check.
     *      0 : Off
     *    100 : On
     * @return 0 if successful, -1 otherwise.
     */
    virtual int brightness(const Color& color) = 0;
};

} // namespace io::led
