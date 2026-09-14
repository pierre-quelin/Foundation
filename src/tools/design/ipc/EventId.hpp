/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EventId.hpp
 * @brief Logical event identity on the IPC bus (platform / object / event name).
 */
#pragma once

#include <string>

namespace tools::design::ipc
{

struct EventId
{
    std::string srcPlatform;
    std::string objectPath;
    std::string event;
};

} // namespace tools::design::ipc
