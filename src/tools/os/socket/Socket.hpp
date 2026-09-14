/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Socket.hpp
 * @brief Portable socket handle and helpers.
 */
#pragma once

#include <cstdint>
#include <string_view>

namespace tools
{
namespace os
{
namespace socket
{

/**
 * @brief RAII wrapper around a BSD socket handle.
 *
 * OS-specific details (Winsock init, send/recv, close) live in Socket.cpp.
 */
class Socket
{
public:
#if defined(_WIN32)
    using Handle = std::uintptr_t;
#else
    using Handle = int;
#endif

    Socket() = default;
    virtual ~Socket();

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] bool valid() const;

    [[nodiscard]] bool sendAll(std::string_view data) const;
    [[nodiscard]] bool sendAll(const char* data, std::size_t size) const;

    /** @return Bytes received, 0 on orderly shutdown, -1 on error. */
    [[nodiscard]] int recv(void* buffer, std::size_t size) const;

    void close();

    [[nodiscard]] static int lastError();

protected:
#if defined(_WIN32)
    static constexpr Handle Invalid = static_cast<Handle>(~static_cast<Handle>(0));
#else
    static constexpr Handle Invalid = -1;
#endif

    explicit Socket(Handle handle);

    Handle _handle = Invalid;
};

} // namespace socket
} // namespace os
} // namespace tools