/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Runtime.hpp
 * @brief Socket runtime bootstrap (Winsock / POSIX).
 */
#pragma once

namespace tools
{
namespace os
{
namespace socket
{

/** @brief Reference-counted Winsock init (Windows) for TCP components. */
class Runtime
{
public:
    Runtime();
    ~Runtime();

    Runtime(const Runtime&)            = delete;
    Runtime& operator=(const Runtime&) = delete;

    [[nodiscard]] bool isInitialized() const { return _initialized; }

private:
    bool _initialized{false};
};

} // namespace socket
} // namespace os
} // namespace tools