/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LinkState.hpp
 * @brief Connection state of the IPC bus link.
 */
#pragma once

namespace tools::design::ipc
{

enum class LinkState
{
    Offline,
    Connecting,
    Online
};

} // namespace tools::design::ipc
