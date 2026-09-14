/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file USB.h
 * @brief USB Interface.
 */
#pragma once

namespace protocol
{

namespace usb
{

struct USB_ID
{
    unsigned int _vid = 0; // Vendor ID
    unsigned int _pid = 0; // Product ID
};

} // namespace usb

} // namespace protocol
