/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IpcException.hpp
 * @brief Exception type for IPC / EventBus operations.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace tools::design::ipc
{

class IpcException : public std::runtime_error
{
public:
    explicit IpcException(const std::string& message) : std::runtime_error(message)
    {
    }
};

} // namespace tools::design::ipc
