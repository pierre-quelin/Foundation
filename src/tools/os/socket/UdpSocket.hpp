/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file UdpSocket.hpp
 * @brief UDP socket wrapper.
 */
#pragma once

#include "tools/os/socket/Socket.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tools
{
namespace os
{
namespace socket
{

/** @brief UDP datagram socket (bind / sendTo / recvFrom). */
class UdpSocket : public Socket
{
public:
    /** @return UDP socket bound to @p port on all interfaces, or invalid on failure. */
    [[nodiscard]] static UdpSocket bind(std::uint16_t port);

    /** @return Unbound UDP socket (send-only until bound), or invalid on failure. */
    [[nodiscard]] static UdpSocket open();

    /**
     * @brief Send a datagram to an IPv4 host.
     * @param host Dotted-decimal address (e.g. "127.0.0.1").
     * @return Bytes sent, or -1 on error.
     */
    [[nodiscard]] int sendTo(std::string_view data, const char* host, std::uint16_t port) const;

    [[nodiscard]] int sendTo(const char* data, std::size_t size, const char* host, std::uint16_t port) const;

    /**
     * @brief Receive a datagram.
     * @param hostOut Optional buffer for sender IPv4 string (may be null).
     * @param portOut Optional sender port in host byte order (may be null).
     * @return Bytes received, or -1 on error.
     */
    [[nodiscard]] int recvFrom(void* buffer, std::size_t size, char* hostOut = nullptr, std::size_t hostOutSize = 0, std::uint16_t* portOut = nullptr) const;

private:
    using Socket::Socket;
};

} // namespace socket
} // namespace os
} // namespace tools